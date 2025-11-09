#pragma once
#include <stddef.h>
#include "emergency_types.h"
#include "rescuers.h"

int parse_emergency_type(const char* path,
                         emergency_type_t** out_emergency_types,
                         size_t* out_emergency_count,
                         rescuer_type_t* all_rescuer_types,
                         size_t rescuer_type_count);

