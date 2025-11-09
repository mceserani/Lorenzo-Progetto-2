#pragma once
#include <stddef.h>
#include "rescuers.h"

int parse_rescuer_type(const char* path,
                       rescuer_type_t** rescuer_types,
                       size_t* out_rescuer_type_count,
                       rescuer_digital_twin_t** out_rescuer_twins,
                       size_t* out_rescuer_twin_count);

