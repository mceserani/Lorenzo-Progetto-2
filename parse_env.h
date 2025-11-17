#pragma once
#include <stddef.h>

typedef struct environment_variable_t {
    char* queue;
    int height;
    int width;
    unsigned int priority_timeouts[3];
    unsigned int aging_start_seconds;
    unsigned int aging_step_seconds;
} environment_variable_t;


int parse_environment_variables(const char* path, environment_variable_t* env_vars);