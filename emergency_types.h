#pragma once
#include <stddef.h>
#include "rescuers.h"



typedef struct rescuer_request_t {
    rescuer_type_t* type;
    int required_count;
    int time_to_manage; // in sec
} rescuer_request_t;

typedef struct emergency_type_t  {
    short priority; // 0 (low) to 2 (high)
    char* emergency_name;
    rescuer_request_t* rescuer_requests;
    int rescuers_req_number;
} emergency_type_t;
