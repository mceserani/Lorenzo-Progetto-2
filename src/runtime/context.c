#include "context.h"

#include <stdlib.h>
#include <string.h>

#include "../../rescuers.h"
#include "../../emergency_types.h"

void app_context_init(app_context_t* ctx) {
    if (!ctx) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
}

void app_context_cleanup(app_context_t* ctx) {
    if (!ctx) {
        return;
    }

    free(ctx->environment.queue);
    ctx->environment.queue = NULL;

    free_rescuer_twins(ctx->rescuer_twins);
    ctx->rescuer_twins = NULL;
    ctx->rescuer_twin_count = 0;

    free_rescuer_types(ctx->rescuer_types);
    ctx->rescuer_types = NULL;
    ctx->rescuer_type_count = 0;

    free_emergency_types(ctx->emergency_types);
    ctx->emergency_types = NULL;
    ctx->emergency_type_count = 0;
}

