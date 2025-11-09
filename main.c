#include <stdio.h>
#include <stdlib.h>

#include "parse_env.h"
#include "parse_rescuers.h"
#include "parse_emergency_types.h"
#include "config_validation.h"
#include "src/runtime/context.h"
#include "logging.h"

int main(void) {
    app_context_t context;
    app_context_init(&context);

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

cleanup:
    if (status != 0) {
        LOG_SYSTEM("SYS-SHUTDOWN-ERROR", "Shutting down with errors (status=%d)", status);
    } else {
        LOG_SYSTEM("SYS-SHUTDOWN", "Graceful shutdown");
    }

    app_context_cleanup(&context);
    log_shutdown();
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

