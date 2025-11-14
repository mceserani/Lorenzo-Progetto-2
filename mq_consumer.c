#define _POSIX_C_SOURCE 200809L
#include "mq_consumer.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "logging.h"

static char* trim_whitespace(char* str) {
    if (!str) {
        return str;
    }

    while (*str && isspace((unsigned char)*str)) {
        ++str;
    }

    if (*str == '\0') {
        return str;
    }

    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }

    return str;
}

static bool mq_consumer_parse_message(mq_consumer_t* consumer, const char* message, emergency_request_t* out_request) {
    if (!consumer || !message || !out_request) {
        return false;
    }

    char buffer[MQ_CONSUMER_DEFAULT_MSGSIZE + 1];
    size_t copy_len = strlen(message);
    if (copy_len >= sizeof(buffer)) {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Received message too large (%zu bytes)", copy_len);
        return false;
    }

    memcpy(buffer, message, copy_len + 1);

    char* saveptr = NULL;
    char* raw_name = strtok_r(buffer, ";", &saveptr);
    char* raw_x = strtok_r(NULL, ";", &saveptr);
    char* raw_y = strtok_r(NULL, ";", &saveptr);
    char* raw_ts = strtok_r(NULL, ";", &saveptr);
    char* extra = strtok_r(NULL, ";", &saveptr);

    if (!raw_name || !raw_x || !raw_y || !raw_ts || extra) {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Invalid message format: '%s'", message);
        return false;
    }

    char* name = trim_whitespace(raw_name);
    char* x_str = trim_whitespace(raw_x);
    char* y_str = trim_whitespace(raw_y);
    char* ts_str = trim_whitespace(raw_ts);

    size_t name_len = strlen(name);
    if (name_len == 0 || name_len >= EMERGENCY_NAME_LENGTH) {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Invalid emergency name '%s'", name);
        return false;
    }

    char* endptr = NULL;
    errno = 0;
    long x_val = strtol(x_str, &endptr, 10);
    if (errno != 0 || !endptr || *endptr != '\0') {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Invalid X coordinate '%s'", x_str);
        return false;
    }

    errno = 0;
    long y_val = strtol(y_str, &endptr, 10);
    if (errno != 0 || !endptr || *endptr != '\0') {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Invalid Y coordinate '%s'", y_str);
        return false;
    }

    errno = 0;
    long long ts_val = strtoll(ts_str, &endptr, 10);
    if (errno != 0 || !endptr || *endptr != '\0') {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Invalid timestamp '%s'", ts_str);
        return false;
    }

    if (x_val < 0 || (consumer->grid_width > 0 && x_val >= consumer->grid_width)) {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "X coordinate out of bounds: %ld", x_val);
        return false;
    }

    if (y_val < 0 || (consumer->grid_height > 0 && y_val >= consumer->grid_height)) {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Y coordinate out of bounds: %ld", y_val);
        return false;
    }

    time_t now = time(NULL);
    if (ts_val <= 0 || (now != (time_t)-1 && ts_val > (long long)(now + 60))) {
        LOG_MESSAGE_QUEUE("MQ-INVALID", "Timestamp not acceptable: %lld", ts_val);
        return false;
    }

    memset(out_request, 0, sizeof(*out_request));
    strncpy(out_request->emergency_name, name, sizeof(out_request->emergency_name) - 1);
    out_request->x = (int)x_val;
    out_request->y = (int)y_val;
    out_request->timestamp = (time_t)ts_val;

    return true;
}

static void* mq_consumer_thread(void* arg) {
    mq_consumer_t* consumer = (mq_consumer_t*)arg;
    if (!consumer) {
        return NULL;
    }

    LOG_MESSAGE_QUEUE("MQ-THREAD-START", "Consumer thread started for queue '%s'", consumer->queue_name);

    char* buffer = calloc(consumer->message_size + 1, sizeof(char));
    if (!buffer) {
        LOG_MESSAGE_QUEUE("MQ-THREAD-ERROR", "Failed to allocate message buffer");
        return NULL;
    }

    while (consumer->running) {
        struct timespec abs_timeout;
        clock_gettime(CLOCK_REALTIME, &abs_timeout);
        abs_timeout.tv_sec += 1;

        ssize_t received = mq_timedreceive(consumer->queue, buffer, consumer->message_size, NULL, &abs_timeout);
        if (received < 0) {
            if (errno == ETIMEDOUT || errno == EAGAIN) {
                continue;
            }
            if (errno == EINTR) {
                continue;
            }

            LOG_MESSAGE_QUEUE("MQ-RECEIVE-ERR", "mq_receive failed: %s", strerror(errno));
            continue;
        }

        buffer[received] = '\0';

        emergency_request_t request;
        if (mq_consumer_parse_message(consumer, buffer, &request)) {
            LOG_MESSAGE_QUEUE(
                "MQ-EMERGENCY",
                "Emergency '%s' received at (%d,%d) timestamp=%ld",
                request.emergency_name,
                request.x,
                request.y,
                (long)request.timestamp);

            if (consumer->runtime_state) {
                if (runtime_state_dispatch_request(consumer->runtime_state,
                                                   &request,
                                                   consumer->emergency_types,
                                                   consumer->emergency_type_count) != 0) {
                    LOG_EMERGENCY_STATUS("RT-DISPATCH-FAIL",
                                         "Failed to enqueue emergency '%s'",
                                         request.emergency_name);
                }
            }
        }
    }

    free(buffer);
    LOG_MESSAGE_QUEUE("MQ-THREAD-STOP", "Consumer thread stopping for queue '%s'", consumer->queue_name);
    return NULL;
}

void mq_consumer_init(mq_consumer_t* consumer) {
    if (!consumer) {
        return;
    }

    memset(consumer, 0, sizeof(*consumer));
    consumer->queue = (mqd_t)-1;
    consumer->message_size = MQ_CONSUMER_DEFAULT_MSGSIZE;
    consumer->running = 0;
    consumer->grid_height = 0;
    consumer->grid_width = 0;
    consumer->thread_created = false;
    consumer->emergency_types = NULL;
    consumer->emergency_type_count = 0;
    consumer->runtime_state = NULL;
}

int mq_consumer_start(mq_consumer_t* consumer,
                      const environment_variable_t* environment,
                      runtime_state_t* runtime_state,
                      const emergency_type_t* emergency_types,
                      size_t emergency_type_count) {
    if (!consumer || !environment || !environment->queue) {
        return -1;
    }

    if (!emergency_types || emergency_type_count == 0) {
        return -1;
    }

    if (!runtime_state) {
        return -1;
    }

    mq_consumer_init(consumer);

    consumer->grid_width = environment->width;
    consumer->grid_height = environment->height;
    consumer->emergency_types = emergency_types;
    consumer->emergency_type_count = emergency_type_count;
    consumer->runtime_state = runtime_state;

    const char* queue_name = environment->queue;
    if (queue_name[0] == '/') {
        if (strlen(queue_name) >= sizeof(consumer->queue_name)) {
            LOG_MESSAGE_QUEUE("MQ-INIT-ERR", "Queue name '%s' is too long", queue_name);
            return -1;
        }
        strncpy(consumer->queue_name, queue_name, sizeof(consumer->queue_name) - 1);
        consumer->queue_name[sizeof(consumer->queue_name) - 1] = '\0';
        consumer->queue_name[strlen(consumer->queue_name)] = '\0';
    } else {
        size_t needed = strlen(queue_name) + 1; // plus slash
        if (needed >= sizeof(consumer->queue_name)) {
            LOG_MESSAGE_QUEUE("MQ-INIT-ERR", "Queue name '%s' is too long", queue_name);
            return -1;
        }
        consumer->queue_name[0] = '/';
        strncpy(consumer->queue_name + 1, queue_name, sizeof(consumer->queue_name) - 2);
        consumer->queue_name[sizeof(consumer->queue_name) - 1] = '\0';
        consumer->queue_name[needed] = '\0';
    }

    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = MQ_CONSUMER_DEFAULT_MAXMSG,
        .mq_msgsize = MQ_CONSUMER_DEFAULT_MSGSIZE,
        .mq_curmsgs = 0,
    };

    consumer->message_size = attr.mq_msgsize;

    consumer->queue = mq_open(consumer->queue_name, O_RDONLY | O_CREAT, 0660, &attr);
    if (consumer->queue == (mqd_t)-1) {
        LOG_MESSAGE_QUEUE("MQ-INIT-ERR", "Failed to open queue '%s': %s", consumer->queue_name, strerror(errno));
        return -1;
    }

    consumer->running = 1;

    int rc = pthread_create(&consumer->thread, NULL, mq_consumer_thread, consumer);
    if (rc != 0) {
        LOG_MESSAGE_QUEUE("MQ-INIT-ERR", "Failed to create consumer thread: %s", strerror(rc));
        consumer->running = 0;
        mq_close(consumer->queue);
        mq_unlink(consumer->queue_name);
        consumer->queue = (mqd_t)-1;
        return -1;
    }

    consumer->thread_created = true;

    LOG_MESSAGE_QUEUE(
        "MQ-INIT",
        "Message queue '%s' initialized (msg_size=%ld max_msg=%ld)",
        consumer->queue_name,
        (long)attr.mq_msgsize,
        (long)attr.mq_maxmsg);

    return 0;
}

void mq_consumer_request_stop(mq_consumer_t* consumer) {
    if (!consumer) {
        return;
    }

    consumer->running = 0;
}

void mq_consumer_shutdown(mq_consumer_t* consumer) {
    if (!consumer) {
        return;
    }

    mq_consumer_request_stop(consumer);

    if (consumer->thread_created) {
        pthread_join(consumer->thread, NULL);
        consumer->thread = (pthread_t)0;
        consumer->thread_created = false;
    }

    if (consumer->queue != (mqd_t)-1) {
        mq_close(consumer->queue);
        if (consumer->queue_name[0] != '\0') {
            mq_unlink(consumer->queue_name);
        }
        consumer->queue = (mqd_t)-1;
    }

    consumer->message_size = MQ_CONSUMER_DEFAULT_MSGSIZE;
    consumer->queue_name[0] = '\0';
    consumer->emergency_types = NULL;
    consumer->emergency_type_count = 0;
    consumer->runtime_state = NULL;
}

