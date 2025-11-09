#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_env.h"
#include "logging.h"

int parse_environment_variables(const char* path, environment_variable_t* env_vars) {
    if (!env_vars || !path) {
        return -1;
    }

    env_vars->height = 0;
    env_vars->width = 0;
    free(env_vars->queue);
    env_vars->queue = NULL;

    LOG_FILE_PARSING("ENV-PARSE-START", "Parsing environment file '%s'", path);

    FILE* file = fopen(path, "r");
    if (!file) {
        LOG_FILE_PARSING("ENV-PARSE-OPEN-ERR", "Unable to open environment file '%s': %s", path, strerror(errno));
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
                    LOG_FILE_PARSING("ENV-PARSE-ALLOC-ERR", "Failed to duplicate queue name while parsing '%s'", path);
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
        LOG_FILE_PARSING("ENV-PARSE-MISSING-QUEUE", "Missing queue entry in environment file '%s'", path);
        result = -1;
    } else if (result == 0) {
        LOG_FILE_PARSING("ENV-PARSE-SUCCESS", "Parsed environment queue='%s' height=%d width=%d", env_vars->queue, env_vars->height, env_vars->width);
    }

    return result;
}

