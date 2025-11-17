#include "config_validation.h"

#include <stdio.h>

#include "logging.h"

static int validate_queue(const environment_variable_t* env) {
    if (!env->queue || env->queue[0] == '\0') {
        fprintf(stderr, "Invalid queue configuration: queue name is empty.\n");
        LOG_CONFIGURATION("CFG-QUEUE-EMPTY", "Queue configuration is invalid: queue name is empty");
        return -1;
    }

    return 0;
}

static int validate_grid_size(const environment_variable_t* env) {
    if (env->height <= 0 || env->width <= 0) {
        fprintf(stderr, "Invalid grid size: height and width must be positive.\n");
        LOG_CONFIGURATION("CFG-GRID-INVALID", "Invalid grid size height=%d width=%d", env->height, env->width);
        return -1;
    }

    return 0;
}

static int validate_environment_timeouts(const environment_variable_t* env) {
    for (size_t i = 0; i < 3; ++i) {
        if (env->priority_timeouts[i] == 0) {
            fprintf(stderr, "Priority timeout %zu cannot be zero.\n", i);
            LOG_CONFIGURATION("CFG-TIMEOUT-INVALID",
                              "Priority timeout for level %zu is zero (array=[%u,%u,%u])",
                              i,
                              env->priority_timeouts[0],
                              env->priority_timeouts[1],
                              env->priority_timeouts[2]);
            return -1;
        }
    }

    if (env->aging_step_seconds == 0) {
        fprintf(stderr, "Aging step cannot be zero.\n");
        LOG_CONFIGURATION("CFG-AGING-STEP-INVALID", "Aging step cannot be zero");
        return -1;
    }

    return 0;
}

static int validate_rescuer_positions(const app_context_t* ctx) {
    for (size_t i = 0; i < ctx->rescuer_type_count; ++i) {
        const rescuer_type_t* type = &ctx->rescuer_types[i];
        if (type->x < 0 || type->y < 0 || type->x >= ctx->environment.width || type->y >= ctx->environment.height) {
            fprintf(stderr, "Rescuer type '%s' position is outside the grid.\n", type->rescuer_type_name);
            LOG_CONFIGURATION("CFG-RESCUER-OOB", "Rescuer type '%s' position (%d,%d) outside grid %dx%d", type->rescuer_type_name, type->x, type->y, ctx->environment.width, ctx->environment.height);
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
                LOG_CONFIGURATION("CFG-UNKNOWN-RESCUER", "Emergency '%s' references an unknown rescuer type", emergency->emergency_name);
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

    if (validate_environment_timeouts(&ctx->environment) != 0) {
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

