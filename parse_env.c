#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h> 
#include "parse_env.h"
#include <stdio.h>

int parse_environment_variables(const char* path, environment_variable_t* env_vars) {
    FILE* file = fopen(path, "r");
    if (!file) {
        perror("Errore nell'apertura del file");
        exit(1);
    }

    char* line = NULL;
    size_t len = 0;

    while (getline(&line, &len, file) != -1) {
        char* saveptr;
        char* tok_key = strtok_r(line, "=", &saveptr);
        char* tok_value = strtok_r(NULL, "\n", &saveptr);

        if (tok_key && tok_value) {
            if (strcmp(tok_key, "queue") == 0) {
                env_vars->queue = strdup(tok_value);
            } else if (strcmp(tok_key, "height") == 0) {
                env_vars->height = atoi(tok_value);
            } else if (strcmp(tok_key, "width") == 0) {
                env_vars->width = atoi(tok_value);
            }
        }
    }

    fclose(file);
    return 0;
}