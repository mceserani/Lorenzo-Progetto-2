#pragma once

#include <time.h>

#include "rescuers.h"

struct emergency_type_t;

#ifndef EMERGENCY_NAME_LENGTH
#define EMERGENCY_NAME_LENGTH 64
#endif

typedef struct emergency_request_t {
    char emergency_type[EMERGENCY_NAME_LENGTH];
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int x;
    int y;
    time_t timestamp;
} emergency_request_t;

typedef enum emergency_status_t {
    EMERGENCY_WAITING,
    EMERGENCY_ASSIGNED,
    EMERGENCY_IN_PROGRESS,
    EMERGENCY_PAUSED,
    EMERGENCY_COMPLETED,
    EMERGENCY_CANCELED,
    EMERGENCY_TIMEOUT,
} emergency_status_t;

typedef struct emergency_t {
    const struct emergency_type_t* type;
    emergency_status_t status;
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int x;
    int y;
    time_t timestamp;
    int rescuer_count;
    rescuer_digital_twin_t* rescuers_dt;
} emergency_t;

