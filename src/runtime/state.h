#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "../../emergency.h"
#include "../../emergency_types.h"
#include "../../rescuers.h"
#include "../../parse_env.h"

typedef struct emergency_record_t {
    emergency_t emergency;
    int priority_score;
    int min_distance;
    int* assigned_indices;
    size_t assigned_count;

    unsigned int manage_time_total;
    unsigned int manage_time_remaining;

    bool preempted;
} emergency_record_t;

typedef struct runtime_state_t {
    pthread_mutex_t mutex;
    pthread_cond_t emergency_available_cond;
    pthread_cond_t rescuer_available_cond;
    pthread_cond_t progress_cond;

    emergency_record_t** waiting_queue;
    size_t waiting_count;
    size_t waiting_capacity;

    emergency_record_t** active_emergencies;
    size_t active_count;
    size_t active_capacity;

    rescuer_digital_twin_t* rescuer_pool;
    size_t rescuer_count;

    pthread_t* workers;
    size_t worker_count;

    pthread_t monitor_thread;
    int monitor_running;

    unsigned int priority_timeouts[3];
    unsigned int aging_start_seconds;
    unsigned int aging_step_seconds;

    int shutdown_requested;
} runtime_state_t;

int runtime_state_init(runtime_state_t* state,
                       const rescuer_digital_twin_t* rescuers,
                       size_t rescuer_count,
                       const environment_variable_t* environment);

void runtime_state_destroy(runtime_state_t* state);

int runtime_state_start_workers(runtime_state_t* state, size_t worker_count);
void runtime_state_request_shutdown(runtime_state_t* state);
void runtime_state_join_workers(runtime_state_t* state);

int runtime_state_dispatch_request(runtime_state_t* state,
                                   const emergency_request_t* request,
                                   const emergency_type_t* emergency_types,
                                   size_t emergency_type_count);
