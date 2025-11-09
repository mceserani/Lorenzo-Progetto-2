#include "config_validation.h"

#include <stdio.h>

static int validate_queue(const environment_variable_t* env) {
    if (!env->queue || env->queue[0] == '\0') {
        fprintf(stderr, "Invalid queue configuration: queue name is empty.\n");
        return -1;
    }

    return 0;
}

static int validate_grid_size(const environment_variable_t* env) {
    if (env->height <= 0 || env->width <= 0) {
        fprintf(stderr, "Invalid grid size: height and width must be positive.\n");
        return -1;
    }

    return 0;
}

static int validate_rescuer_positions(const app_context_t* ctx) {
    for (size_t i = 0; i < ctx->rescuer_type_count; ++i) {
        const rescuer_type_t* type = &ctx->rescuer_types[i];
        if (type->x < 0 || type->y < 0 || type->x >= ctx->environment.width || type->y >= ctx->environment.height) {
            fprintf(stderr, "Rescuer type '%s' position is outside the grid.\n", type->rescuer_type_name);
            return -1;
        }
    }

    return 0;
}

static int validate_emergency_rescuer_links(const app_context_t* ctx) {
    for (size_t i = 0; i < ctx->emergency_type_count; ++i) {
        const emergency_type_t* emergency = &ctx->emergency_types[i];
        for (int j = 0; j < emergency->rescuers_req_number; ++j) {
            const rescuer_request_t* request = &emergency->rescuer_requests[j];
            if (!request->type) {
                fprintf(stderr, "Emergency '%s' references an unknown rescuer type.\n", emergency->emergency_name);
                return -1;
            }
        }
    }

    return 0;
}

int validate_configuration(const app_context_t* ctx) {
    if (!ctx) {
        return -1;
    }

    if (validate_queue(&ctx->environment) != 0) {
        return -1;
    }

    if (validate_grid_size(&ctx->environment) != 0) {
        return -1;
    }

    if (validate_rescuer_positions(ctx) != 0) {
        return -1;
    }

    if (validate_emergency_rescuer_links(ctx) != 0) {
        return -1;
    }

    return 0;
}

