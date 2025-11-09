#pragma once

#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdbool.h>

#include "emergency.h"
#include "parse_env.h"

#ifndef MQ_CONSUMER_MAX_QUEUE_NAME
#define MQ_CONSUMER_MAX_QUEUE_NAME 255
#endif

#ifndef MQ_CONSUMER_DEFAULT_MAXMSG
#define MQ_CONSUMER_DEFAULT_MAXMSG 32
#endif

#ifndef MQ_CONSUMER_DEFAULT_MSGSIZE
#define MQ_CONSUMER_DEFAULT_MSGSIZE 256
#endif

typedef struct mq_consumer_t {
    mqd_t queue;
    pthread_t thread;
    size_t message_size;
    volatile sig_atomic_t running;
    char queue_name[MQ_CONSUMER_MAX_QUEUE_NAME + 1];
    int grid_width;
    int grid_height;
    bool thread_created;
    const struct emergency_type_t* emergency_types;
    size_t emergency_type_count;
} mq_consumer_t;

void mq_consumer_init(mq_consumer_t* consumer);
int mq_consumer_start(mq_consumer_t* consumer,
                      const environment_variable_t* environment,
                      const struct emergency_type_t* emergency_types,
                      size_t emergency_type_count);
void mq_consumer_request_stop(mq_consumer_t* consumer);
void mq_consumer_shutdown(mq_consumer_t* consumer);

