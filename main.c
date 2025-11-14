#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parse_env.h"
#include "parse_rescuers.h"
#include "parse_emergency_types.h"
#include "config_validation.h"
#include "src/runtime/context.h"
#include "src/runtime/state.h"
#include "logging.h"
#include "mq_consumer.h"

static volatile sig_atomic_t g_shutdown_requested = 0;
static volatile sig_atomic_t g_shutdown_signal = 0;

static void handle_shutdown_signal(int signo) {
    g_shutdown_signal = signo;
    g_shutdown_requested = 1;
}

int main(void) {
    app_context_t context;
    app_context_init(&context);

    mq_consumer_t consumer;
    mq_consumer_init(&consumer);
    bool consumer_started = false;
    runtime_state_t runtime_state;
    bool runtime_initialized = false;
    bool workers_started = false;

    if (log_init(NULL) != 0) {
        fprintf(stderr, "Failed to initialize logging.\n");
    } else {
        LOG_SYSTEM("SYS-START", "Application startup");
    }

    int status = parse_environment_variables("environment.txt", &context.environment);
    if (status != 0) {
        fprintf(stderr, "Failed to parse environment configuration.\n");
        LOG_SYSTEM("SYS-ERROR", "Environment parsing failed with status %d", status);
        goto cleanup;
    }

    status = parse_rescuer_type("rescuers.txt",
                                &context.rescuer_types,
                                &context.rescuer_type_count,
                                &context.rescuer_twins,
                                &context.rescuer_twin_count);
    if (status != 0) {
        fprintf(stderr, "Failed to parse rescuer configuration.\n");
        LOG_SYSTEM("SYS-ERROR", "Rescuer parsing failed with status %d", status);
        goto cleanup;
    }

    status = parse_emergency_type("emergency.txt",
                                   &context.emergency_types,
                                   &context.emergency_type_count,
                                   context.rescuer_types,
                                   context.rescuer_type_count);
    if (status != 0) {
        fprintf(stderr, "Failed to parse emergency configuration.\n");
        LOG_SYSTEM("SYS-ERROR", "Emergency parsing failed with status %d", status);
        goto cleanup;
    }

    status = validate_configuration(&context);
    if (status != 0) {
        fprintf(stderr, "Configuration validation failed.\n");
        LOG_SYSTEM("SYS-ERROR", "Configuration validation failed with status %d", status);
        goto cleanup;
    }

    LOG_SYSTEM("SYS-READY", "Configuration parsed and validated successfully");

    if (runtime_state_init(&runtime_state, context.rescuer_twins, context.rescuer_twin_count) != 0) {
        fprintf(stderr, "Failed to initialize runtime state.\n");
        LOG_SYSTEM("SYS-ERROR", "Runtime state initialization failed");
        status = -1;
        goto cleanup;
    }
    runtime_initialized = true;

    if (runtime_state_start_workers(&runtime_state, 0) != 0) {
        fprintf(stderr, "Failed to start runtime workers.\n");
        LOG_SYSTEM("SYS-ERROR", "Runtime worker startup failed");
        status = -1;
        goto cleanup;
    }
    workers_started = true;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
        LOG_SYSTEM("SYS-SIGNAL-ERR", "Failed to configure signal handlers: %s", strerror(errno));
        fprintf(stderr, "Failed to configure signal handlers.\n");
        status = -1;
        goto cleanup;
    }

    if (mq_consumer_start(&consumer,
                          &context.environment,
                          &runtime_state,
                          context.emergency_types,
                          context.emergency_type_count) != 0) {
        LOG_SYSTEM("SYS-ERROR", "Unable to start message queue consumer");
        status = -1;
        goto cleanup;
    }
    consumer_started = true;

    LOG_SYSTEM("SYS-WAIT", "Waiting for shutdown signal (PID=%d)", getpid());

    while (!g_shutdown_requested) {
        pause();
    }

    if (g_shutdown_signal != 0) {
        LOG_SYSTEM("SYS-SIGNAL", "Received signal %d, shutting down", g_shutdown_signal);
    }

cleanup:
    if (consumer_started) {
        mq_consumer_shutdown(&consumer);
        consumer_started = false;
    }

    if (workers_started) {
        runtime_state_request_shutdown(&runtime_state);
        runtime_state_join_workers(&runtime_state);
        workers_started = false;
    }

    if (runtime_initialized) {
        runtime_state_destroy(&runtime_state);
        runtime_initialized = false;
    }

    if (status != 0) {
        LOG_SYSTEM("SYS-SHUTDOWN-ERROR", "Shutting down with errors (status=%d)", status);
    } else {
        LOG_SYSTEM("SYS-SHUTDOWN", "Graceful shutdown");
    }

    app_context_cleanup(&context);
    log_shutdown();
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

