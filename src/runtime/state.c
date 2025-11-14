#define _POSIX_C_SOURCE 200809L
#include "state.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../logging.h"

#ifndef RUNTIME_DEFAULT_WORKERS
#define RUNTIME_DEFAULT_WORKERS 2
#endif

static void emergency_record_cleanup(emergency_record_t* record) {
    if (!record) {
        return;
    }

    free(record->assigned_indices);
    record->assigned_indices = NULL;
    record->assigned_count = 0;

    free(record->emergency.rescuers_dt);
    record->emergency.rescuers_dt = NULL;
    record->emergency.rescuer_count = 0;
}

static void runtime_state_clear_arrays(runtime_state_t* state) {
    if (!state) {
        return;
    }

    for (size_t i = 0; i < state->waiting_count; ++i) {
        emergency_record_cleanup(&state->waiting_queue[i]);
    }
    free(state->waiting_queue);
    state->waiting_queue = NULL;
    state->waiting_count = 0;
    state->waiting_capacity = 0;

    for (size_t i = 0; i < state->active_count; ++i) {
        emergency_record_cleanup(&state->active_emergencies[i]);
    }
    free(state->active_emergencies);
    state->active_emergencies = NULL;
    state->active_count = 0;
    state->active_capacity = 0;
}

static int ensure_capacity(emergency_record_t** array,
                           size_t* capacity,
                           size_t count,
                           size_t additional) {
    if (!array || !capacity) {
        return -1;
    }

    size_t required = count + additional;
    if (required <= *capacity) {
        return 0;
    }

    size_t new_capacity = (*capacity == 0) ? 4 : *capacity;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    emergency_record_t* tmp = realloc(*array, new_capacity * sizeof(emergency_record_t));
    if (!tmp) {
        return -1;
    }

    *array = tmp;
    *capacity = new_capacity;
    return 0;
}

static const emergency_type_t* find_emergency_type(const emergency_type_t* types,
                                                   size_t type_count,
                                                   const char* name) {
    if (!types || !name) {
        return NULL;
    }

    for (size_t i = 0; i < type_count; ++i) {
        if (types[i].emergency_name && strcmp(types[i].emergency_name, name) == 0) {
            return &types[i];
        }
    }

    return NULL;
}

static int compute_manhattan_distance(const rescuer_digital_twin_t* rescuer, int x, int y) {
    if (!rescuer) {
        return INT_MAX;
    }

    int dx = rescuer->x - x;
    if (dx < 0) {
        dx = -dx;
    }
    int dy = rescuer->y - y;
    if (dy < 0) {
        dy = -dy;
    }

    return dx + dy;
}

static int compute_min_distance(const runtime_state_t* state, int x, int y) {
    if (!state || !state->rescuer_pool || state->rescuer_count == 0) {
        return INT_MAX;
    }

    int min_distance = INT_MAX;
    for (size_t i = 0; i < state->rescuer_count; ++i) {
        const rescuer_digital_twin_t* rescuer = &state->rescuer_pool[i];
        int distance = compute_manhattan_distance(rescuer, x, y);
        if (distance < min_distance) {
            min_distance = distance;
        }
    }

    return min_distance;
}

static void waiting_queue_insert_locked(runtime_state_t* state, emergency_record_t* record) {
    if (!state || !record) {
        return;
    }

    if (ensure_capacity(&state->waiting_queue,
                        &state->waiting_capacity,
                        state->waiting_count,
                        1) != 0) {
        LOG_EMERGENCY_STATUS("RT-QUEUE-ERR", "Unable to grow waiting queue for emergency '%s'", record->emergency.name);
        emergency_record_cleanup(record);
        return;
    }

    size_t idx = state->waiting_count;
    while (idx > 0) {
        emergency_record_t* prev = &state->waiting_queue[idx - 1];
        if (prev->priority_score >= record->priority_score) {
            break;
        }
        if (idx < state->waiting_capacity) {
            state->waiting_queue[idx] = state->waiting_queue[idx - 1];
        }
        --idx;
    }

    state->waiting_queue[idx] = *record;
    memset(record, 0, sizeof(*record));
    state->waiting_count++;
}

static emergency_record_t waiting_queue_pop_front_locked(runtime_state_t* state) {
    emergency_record_t record;
    memset(&record, 0, sizeof(record));

    if (!state || state->waiting_count == 0) {
        return record;
    }

    record = state->waiting_queue[0];
    if (state->waiting_count > 1) {
        memmove(&state->waiting_queue[0],
                &state->waiting_queue[1],
                (state->waiting_count - 1) * sizeof(emergency_record_t));
    }
    state->waiting_count--;

    return record;
}

static size_t active_list_add_locked(runtime_state_t* state, emergency_record_t* record) {
    if (!state || !record) {
        return (size_t)-1;
    }

    if (ensure_capacity(&state->active_emergencies,
                        &state->active_capacity,
                        state->active_count,
                        1) != 0) {
        LOG_EMERGENCY_STATUS("RT-ACTIVE-ERR", "Unable to track active emergency '%s'", record->emergency.name);
        emergency_record_cleanup(record);
        return (size_t)-1;
    }

    state->active_emergencies[state->active_count] = *record;
    size_t index = state->active_count;
    memset(record, 0, sizeof(*record));
    state->active_count++;
    return index;
}

static void active_list_remove_index_locked(runtime_state_t* state, size_t index) {
    if (!state || index >= state->active_count) {
        return;
    }

    emergency_record_cleanup(&state->active_emergencies[index]);
    if (index + 1 < state->active_count) {
        memmove(&state->active_emergencies[index],
                &state->active_emergencies[index + 1],
                (state->active_count - index - 1) * sizeof(emergency_record_t));
    }
    state->active_count--;
}

static unsigned int compute_travel_time_seconds(const rescuer_digital_twin_t* rescuer,
                                                int target_x,
                                                int target_y) {
    if (!rescuer) {
        return 1;
    }

    int distance = compute_manhattan_distance(rescuer, target_x, target_y);
    int speed = 1;
    if (rescuer->type && rescuer->type->speed > 0) {
        speed = rescuer->type->speed;
    }

    int time_needed = distance / speed;
    if (distance % speed != 0) {
        ++time_needed;
    }

    if (time_needed <= 0) {
        time_needed = 1;
    }

    return (unsigned int)time_needed;
}

static unsigned int compute_management_time_seconds(const emergency_record_t* record) {
    if (!record || !record->emergency.type.rescuer_requests || record->emergency.type.rescuers_req_number == 0) {
        return 1;
    }

    unsigned int max_time = 1;
    for (int i = 0; i < record->emergency.type.rescuers_req_number; ++i) {
        const rescuer_request_t* req = &record->emergency.type.rescuer_requests[i];
        if (req->time_to_manage > (int)max_time) {
            max_time = (unsigned int)req->time_to_manage;
        }
    }

    if (max_time == 0) {
        max_time = 1;
    }

    return max_time;
}

static void log_rescuer_transition(const rescuer_digital_twin_t* rescuer,
                                   rescuer_status_t old_status,
                                   rescuer_status_t new_status,
                                   const char* emergency_name) {
    if (!rescuer) {
        return;
    }

    const char* rescuer_type_name = rescuer->type ? rescuer->type->rescuer_type_name : "unknown";
    LOG_RESCUER_STATUS("RESC-STATE",
                       "Rescuer %d (%s) %s -> %s for emergency '%s'",
                       rescuer->id,
                       rescuer_type_name,
                       old_status == IDLE ? "IDLE" :
                       old_status == EN_ROUTE_TO_SCENE ? "EN_ROUTE_TO_SCENE" :
                       old_status == ON_SCENE ? "ON_SCENE" :
                       old_status == RETURNING_TO_BASE ? "RETURNING_TO_BASE" : "UNKNOWN",
                       new_status == IDLE ? "IDLE" :
                       new_status == EN_ROUTE_TO_SCENE ? "EN_ROUTE_TO_SCENE" :
                       new_status == ON_SCENE ? "ON_SCENE" :
                       new_status == RETURNING_TO_BASE ? "RETURNING_TO_BASE" : "UNKNOWN",
                       emergency_name ? emergency_name : "");
}

static void* runtime_worker_thread(void* arg);

int runtime_state_init(runtime_state_t* state,
                       const rescuer_digital_twin_t* rescuers,
                       size_t rescuer_count) {
    if (!state) {
        return -1;
    }

    memset(state, 0, sizeof(*state));

    if (pthread_mutex_init(&state->mutex, NULL) != 0) {
        return -1;
    }

    if (pthread_cond_init(&state->emergency_available_cond, NULL) != 0) {
        pthread_mutex_destroy(&state->mutex);
        return -1;
    }

    if (pthread_cond_init(&state->rescuer_available_cond, NULL) != 0) {
        pthread_cond_destroy(&state->emergency_available_cond);
        pthread_mutex_destroy(&state->mutex);
        return -1;
    }

    if (rescuer_count > 0) {
        state->rescuer_pool = calloc(rescuer_count, sizeof(rescuer_digital_twin_t));
        if (!state->rescuer_pool) {
            pthread_cond_destroy(&state->rescuer_available_cond);
            pthread_cond_destroy(&state->emergency_available_cond);
            pthread_mutex_destroy(&state->mutex);
            return -1;
        }

        memcpy(state->rescuer_pool, rescuers, rescuer_count * sizeof(rescuer_digital_twin_t));
        for (size_t i = 0; i < rescuer_count; ++i) {
            state->rescuer_pool[i].status = IDLE;
        }
    }
    state->rescuer_count = rescuer_count;
    state->shutdown_requested = 0;

    return 0;
}

void runtime_state_destroy(runtime_state_t* state) {
    if (!state) {
        return;
    }

    runtime_state_request_shutdown(state);
    runtime_state_join_workers(state);

    runtime_state_clear_arrays(state);
    free(state->rescuer_pool);
    state->rescuer_pool = NULL;
    state->rescuer_count = 0;

    pthread_cond_destroy(&state->rescuer_available_cond);
    pthread_cond_destroy(&state->emergency_available_cond);
    pthread_mutex_destroy(&state->mutex);
}

int runtime_state_start_workers(runtime_state_t* state, size_t worker_count) {
    if (!state) {
        return -1;
    }

    if (worker_count == 0) {
        worker_count = RUNTIME_DEFAULT_WORKERS;
    }

    state->workers = calloc(worker_count, sizeof(pthread_t));
    if (!state->workers) {
        return -1;
    }

    state->worker_count = worker_count;

    for (size_t i = 0; i < worker_count; ++i) {
        if (pthread_create(&state->workers[i], NULL, runtime_worker_thread, state) != 0) {
            state->worker_count = i;
            runtime_state_request_shutdown(state);
            runtime_state_join_workers(state);
            free(state->workers);
            state->workers = NULL;
            return -1;
        }
    }

    LOG_SYSTEM("RT-WORKERS", "Runtime dispatcher started with %zu workers", worker_count);
    return 0;
}

void runtime_state_request_shutdown(runtime_state_t* state) {
    if (!state) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->shutdown_requested = 1;
    pthread_cond_broadcast(&state->emergency_available_cond);
    pthread_cond_broadcast(&state->rescuer_available_cond);
    pthread_mutex_unlock(&state->mutex);
}

void runtime_state_join_workers(runtime_state_t* state) {
    if (!state) {
        return;
    }

    if (state->workers) {
        for (size_t i = 0; i < state->worker_count; ++i) {
            pthread_join(state->workers[i], NULL);
        }
        free(state->workers);
        state->workers = NULL;
        state->worker_count = 0;
    }
}

static int emergency_record_prepare(emergency_record_t* record,
                                    const emergency_request_t* request,
                                    const emergency_type_t* type,
                                    const runtime_state_t* state) {
    if (!record || !request || !type) {
        return -1;
    }

    memset(record, 0, sizeof(*record));
    record->emergency.status = WAITING;
    record->emergency.type = *type;
    record->emergency.x = request->x;
    record->emergency.y = request->y;
    record->emergency.time = request->timestamp;
    record->emergency.rescuer_count = 0;
    record->emergency.rescuers_dt = NULL;
    strncpy(record->emergency.name, request->emergency_name, sizeof(record->emergency.name) - 1);

    if (type->rescuer_requests && type->rescuers_req_number > 0) {
        int total = 0;
        for (int i = 0; i < type->rescuers_req_number; ++i) {
            if (type->rescuer_requests[i].required_count > 0) {
                total += type->rescuer_requests[i].required_count;
            }
        }
        if (total > 0) {
            record->emergency.rescuer_count = total;
        }
    }

    record->min_distance = compute_min_distance(state, request->x, request->y);
    if (record->min_distance == INT_MAX) {
        record->min_distance = 1000000;
    }

    int priority = type->priority;
    record->priority_score = priority * 100000 - record->min_distance;

    LOG_EMERGENCY_STATUS("RT-DISPATCH-QUEUE",
                         "Emergency '%s' queued with priority=%d min_distance=%d",
                         record->emergency.name,
                         record->priority_score,
                         record->min_distance);

    return 0;
}

int runtime_state_dispatch_request(runtime_state_t* state,
                                   const emergency_request_t* request,
                                   const emergency_type_t* emergency_types,
                                   size_t emergency_type_count) {
    if (!state || !request || !emergency_types || emergency_type_count == 0) {
        return -1;
    }

    pthread_mutex_lock(&state->mutex);

    if (state->shutdown_requested) {
        pthread_mutex_unlock(&state->mutex);
        return -1;
    }

    const emergency_type_t* type = find_emergency_type(emergency_types, emergency_type_count, request->emergency_name);
    if (!type) {
        LOG_EMERGENCY_STATUS("RT-DISPATCH-UNKNOWN", "Unknown emergency type '%s'", request->emergency_name);
        pthread_mutex_unlock(&state->mutex);
        return -1;
    }

    emergency_record_t record;
    if (emergency_record_prepare(&record, request, type, state) != 0) {
        pthread_mutex_unlock(&state->mutex);
        return -1;
    }

    waiting_queue_insert_locked(state, &record);
    pthread_cond_signal(&state->emergency_available_cond);
    pthread_mutex_unlock(&state->mutex);
    return 0;
}

static bool try_allocate_rescuers_locked(runtime_state_t* state,
                                         emergency_record_t* record,
                                         int** out_indices,
                                         size_t* out_count) {
    if (!state || !record || !out_indices || !out_count) {
        return false;
    }

    const emergency_type_t* type = &record->emergency.type;
    if (!type->rescuer_requests || type->rescuers_req_number == 0) {
        *out_indices = NULL;
        *out_count = 0;
        return true;
    }

    size_t total_needed = 0;
    for (int i = 0; i < type->rescuers_req_number; ++i) {
        if (type->rescuer_requests[i].required_count > 0) {
            total_needed += (size_t)type->rescuer_requests[i].required_count;
        }
    }

    if (total_needed == 0) {
        *out_indices = NULL;
        *out_count = 0;
        return true;
    }

    int* selections = calloc(total_needed, sizeof(int));
    if (!selections) {
        return false;
    }

    size_t selection_index = 0;

    for (int req_idx = 0; req_idx < type->rescuers_req_number; ++req_idx) {
        const rescuer_request_t* req = &type->rescuer_requests[req_idx];
        if (!req->type || req->required_count <= 0) {
            continue;
        }

        for (int needed = 0; needed < req->required_count; ++needed) {
            int best_index = -1;
            int best_distance = INT_MAX;
            for (size_t rescuer_idx = 0; rescuer_idx < state->rescuer_count; ++rescuer_idx) {
                rescuer_digital_twin_t* rescuer = &state->rescuer_pool[rescuer_idx];
                if (rescuer->status != IDLE) {
                    continue;
                }
                if (rescuer->type != req->type) {
                    continue;
                }

                bool already_selected = false;
                for (size_t check = 0; check < selection_index; ++check) {
                    if (selections[check] == (int)rescuer_idx) {
                        already_selected = true;
                        break;
                    }
                }
                if (already_selected) {
                    continue;
                }

                int distance = compute_manhattan_distance(rescuer, record->emergency.x, record->emergency.y);
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = (int)rescuer_idx;
                }
            }

            if (best_index < 0) {
                free(selections);
                return false;
            }

            selections[selection_index++] = best_index;
        }
    }

    *out_indices = selections;
    *out_count = selection_index;
    return true;
}

static void update_rescuer_status_locked(runtime_state_t* state,
                                         int index,
                                         rescuer_status_t new_status,
                                         const char* emergency_name) {
    if (!state || index < 0 || (size_t)index >= state->rescuer_count) {
        return;
    }

    rescuer_digital_twin_t* rescuer = &state->rescuer_pool[index];
    rescuer_status_t old_status = rescuer->status;
    rescuer->status = new_status;
    log_rescuer_transition(rescuer, old_status, new_status, emergency_name);
}

static void update_rescuer_position_locked(runtime_state_t* state, int index, int x, int y) {
    if (!state || index < 0 || (size_t)index >= state->rescuer_count) {
        return;
    }
    state->rescuer_pool[index].x = x;
    state->rescuer_pool[index].y = y;
}

static void* runtime_worker_thread(void* arg) {
    runtime_state_t* state = (runtime_state_t*)arg;
    if (!state) {
        return NULL;
    }

    while (true) {
        pthread_mutex_lock(&state->mutex);
        while (!state->shutdown_requested && state->waiting_count == 0) {
            pthread_cond_wait(&state->emergency_available_cond, &state->mutex);
        }

        if (state->shutdown_requested) {
            pthread_mutex_unlock(&state->mutex);
            break;
        }

        emergency_record_t record = waiting_queue_pop_front_locked(state);
        if (record.emergency.name[0] == '\0') {
            pthread_mutex_unlock(&state->mutex);
            continue;
        }

        int* assigned_indices = NULL;
        size_t assigned_count = 0;
        if (!try_allocate_rescuers_locked(state, &record, &assigned_indices, &assigned_count)) {
            waiting_queue_insert_locked(state, &record);
            pthread_cond_wait(&state->rescuer_available_cond, &state->mutex);
            pthread_mutex_unlock(&state->mutex);
            continue;
        }

        record.assigned_indices = assigned_indices;
        record.assigned_count = assigned_count;

        if (record.emergency.rescuer_count > 0) {
            record.emergency.rescuers_dt = calloc((size_t)record.emergency.rescuer_count,
                                                  sizeof(rescuer_digital_twin_t));
        }

        for (size_t i = 0; i < assigned_count; ++i) {
            int idx = assigned_indices[i];
            rescuer_digital_twin_t* rescuer = &state->rescuer_pool[idx];
            if (record.emergency.rescuers_dt && (int)i < record.emergency.rescuer_count) {
                record.emergency.rescuers_dt[i] = *rescuer;
            }
            update_rescuer_status_locked(state, idx, EN_ROUTE_TO_SCENE, record.emergency.name);
        }

        emergency_status_t old_status = record.emergency.status;
        record.emergency.status = ASSIGNED;
        LOG_EMERGENCY_STATUS("RT-ASSIGNED",
                             "Emergency '%s' %s -> %s (%zu rescuers)",
                             record.emergency.name,
                             old_status == WAITING ? "WAITING" : "UNKNOWN",
                             "ASSIGNED",
                             assigned_count);

        size_t active_index = active_list_add_locked(state, &record);
        emergency_record_t* active_record = NULL;
        if (active_index != (size_t)-1) {
            active_record = &state->active_emergencies[active_index];
        }

        pthread_mutex_unlock(&state->mutex);

        if (!active_record) {
            continue;
        }

        unsigned int travel_time = 1;
        for (size_t i = 0; i < active_record->assigned_count; ++i) {
            int rescuer_idx = active_record->assigned_indices[i];
            if (rescuer_idx >= 0 && (size_t)rescuer_idx < state->rescuer_count) {
                unsigned int t = compute_travel_time_seconds(&state->rescuer_pool[rescuer_idx],
                                                             active_record->emergency.x,
                                                             active_record->emergency.y);
                if (t > travel_time) {
                    travel_time = t;
                }
            }
        }

        sleep(travel_time);

        pthread_mutex_lock(&state->mutex);
        for (size_t i = 0; i < active_record->assigned_count; ++i) {
            int idx = active_record->assigned_indices[i];
            update_rescuer_position_locked(state, idx, active_record->emergency.x, active_record->emergency.y);
            update_rescuer_status_locked(state, idx, ON_SCENE, active_record->emergency.name);
        }

        old_status = active_record->emergency.status;
        active_record->emergency.status = IN_PROGRESS;
        LOG_EMERGENCY_STATUS("RT-INPROGRESS",
                             "Emergency '%s' %s -> %s",
                             active_record->emergency.name,
                             old_status == ASSIGNED ? "ASSIGNED" : "UNKNOWN",
                             "IN_PROGRESS");
        pthread_mutex_unlock(&state->mutex);

        unsigned int manage_time = compute_management_time_seconds(active_record);
        sleep(manage_time);

        pthread_mutex_lock(&state->mutex);
        for (size_t i = 0; i < active_record->assigned_count; ++i) {
            int idx = active_record->assigned_indices[i];
            update_rescuer_status_locked(state, idx, RETURNING_TO_BASE, active_record->emergency.name);
        }
        pthread_mutex_unlock(&state->mutex);

        unsigned int return_time = travel_time;
        if (return_time == 0) {
            return_time = 1;
        }
        sleep(return_time);

        pthread_mutex_lock(&state->mutex);
        for (size_t i = 0; i < active_record->assigned_count; ++i) {
            int idx = active_record->assigned_indices[i];
            if (idx < 0 || (size_t)idx >= state->rescuer_count) {
                continue;
            }
            rescuer_digital_twin_t* rescuer = &state->rescuer_pool[idx];
            if (rescuer->type) {
                update_rescuer_position_locked(state, idx, rescuer->type->x, rescuer->type->y);
            }
            update_rescuer_status_locked(state, idx, IDLE, active_record->emergency.name);
        }

        old_status = active_record->emergency.status;
        active_record->emergency.status = COMPLETED;
        LOG_EMERGENCY_STATUS("RT-COMPLETED",
                             "Emergency '%s' %s -> %s",
                             active_record->emergency.name,
                             old_status == IN_PROGRESS ? "IN_PROGRESS" : "UNKNOWN",
                             "COMPLETED");

        pthread_cond_broadcast(&state->rescuer_available_cond);
        active_list_remove_index_locked(state, active_index);
        pthread_mutex_unlock(&state->mutex);
    }

    return NULL;
}

