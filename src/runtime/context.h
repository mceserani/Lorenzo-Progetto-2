#pragma once

#include <stddef.h>

#include "../../parse_env.h"
#include "../../rescuers.h"
#include "../../emergency_types.h"

typedef struct app_context_t {
    environment_variable_t environment;
    rescuer_type_t* rescuer_types;
    size_t rescuer_type_count;
    rescuer_digital_twin_t* rescuer_twins;
    size_t rescuer_twin_count;
    emergency_type_t* emergency_types;
    size_t emergency_type_count;
} app_context_t;

void app_context_init(app_context_t* ctx);
void app_context_cleanup(app_context_t* ctx);

