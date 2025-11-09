#pragma once
#include <stddef.h>
#include "rescuers.h"

int parse_rescuer_type(const char* path, rescuer_type_t** rescuer_types, rescuer_digital_twin_t** out_rescuer_twins);