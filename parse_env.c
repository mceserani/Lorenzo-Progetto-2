#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "parse_env.h"

int parse_environment_variables(const char* path, environment_variable_t* env_vars) {
    if (!env_vars || !path) {
        return -1;
    }

    env_vars->height = 0;
    env_vars->width = 0;
    free(env_vars->queue);
    env_vars->queue = NULL;

    FILE* file = fopen(path, "r");
    if (!file) {
        perror("Errore nell'apertura del file");
        return -1;
    }

    char* line = NULL;
    size_t len = 0;

    int result = 0;

    while (getline(&line, &len, file) != -1) {
        char* saveptr;
        char* tok_key = strtok_r(line, "=", &saveptr);
        char* tok_value = strtok_r(NULL, "\n", &saveptr);

        if (tok_key && tok_value) {
            if (strcmp(tok_key, "queue") == 0) {
                char* dup = strdup(tok_value);
                if (!dup) {
                    result = -1;
                    break;
                }
                free(env_vars->queue);
                env_vars->queue = dup;
            } else if (strcmp(tok_key, "height") == 0) {
                env_vars->height = atoi(tok_value);
            } else if (strcmp(tok_key, "width") == 0) {
                env_vars->width = atoi(tok_value);
            }
        }
    }

    free(line);
    fclose(file);

    if (!env_vars->queue) {
        result = -1;
    }

    return result;
}

