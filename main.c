#include <stdio.h>
#include <stdlib.h>

#include "parse_env.h"
#include "parse_rescuers.h"
#include "parse_emergency_types.h"
#include "config_validation.h"
#include "src/runtime/context.h"

int main(void) {
    app_context_t context;
    app_context_init(&context);

    int status = parse_environment_variables("environment.txt", &context.environment);
    if (status != 0) {
        fprintf(stderr, "Failed to parse environment configuration.\n");
        goto cleanup;
    }

    status = parse_rescuer_type("rescuers.txt",
                                &context.rescuer_types,
                                &context.rescuer_type_count,
                                &context.rescuer_twins,
                                &context.rescuer_twin_count);
    if (status != 0) {
        fprintf(stderr, "Failed to parse rescuer configuration.\n");
        goto cleanup;
    }

    status = parse_emergency_type("emergency.txt",
                                   &context.emergency_types,
                                   &context.emergency_type_count,
                                   context.rescuer_types,
                                   context.rescuer_type_count);
    if (status != 0) {
        fprintf(stderr, "Failed to parse emergency configuration.\n");
        goto cleanup;
    }

    status = validate_configuration(&context);
    if (status != 0) {
        fprintf(stderr, "Configuration validation failed.\n");
        goto cleanup;
    }

cleanup:
    app_context_cleanup(&context);
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

