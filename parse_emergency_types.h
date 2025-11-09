#pragma once   
#include <stddef.h>
#include "emergency_types.h"

int parse_emergency_type(const char* path, emergency_type_t** emergency_types, rescuer_type_t* all_rescuer_types);