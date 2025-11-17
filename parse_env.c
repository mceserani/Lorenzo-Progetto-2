#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_env.h"
#include "logging.h"

#define DEFAULT_PRIORITY_TIMEOUT_LOW 180
#define DEFAULT_PRIORITY_TIMEOUT_MEDIUM 120
#define DEFAULT_PRIORITY_TIMEOUT_HIGH 60
#define DEFAULT_AGING_START 90
#define DEFAULT_AGING_STEP 30

int parse_environment_variables(const char* path, environment_variable_t* env_vars) {
    if (!env_vars || !path) {
        return -1;
    }

    env_vars->height = 0;
    env_vars->width = 0;
    env_vars->priority_timeouts[0] = DEFAULT_PRIORITY_TIMEOUT_LOW;
    env_vars->priority_timeouts[1] = DEFAULT_PRIORITY_TIMEOUT_MEDIUM;
    env_vars->priority_timeouts[2] = DEFAULT_PRIORITY_TIMEOUT_HIGH;
    env_vars->aging_start_seconds = DEFAULT_AGING_START;
    env_vars->aging_step_seconds = DEFAULT_AGING_STEP;
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
            } else if (strcmp(tok_key, "priority0_timeout") == 0) {
                env_vars->priority_timeouts[0] = (unsigned int)atoi(tok_value);
            } else if (strcmp(tok_key, "priority1_timeout") == 0) {
                env_vars->priority_timeouts[1] = (unsigned int)atoi(tok_value);
            } else if (strcmp(tok_key, "priority2_timeout") == 0) {
                env_vars->priority_timeouts[2] = (unsigned int)atoi(tok_value);
            } else if (strcmp(tok_key, "aging_start") == 0) {
                env_vars->aging_start_seconds = (unsigned int)atoi(tok_value);
            } else if (strcmp(tok_key, "aging_step") == 0) {
                env_vars->aging_step_seconds = (unsigned int)atoi(tok_value);
            }
        }
    }

    free(line);
    fclose(file);

    if (!env_vars->queue) {
        LOG_FILE_PARSING("ENV-PARSE-MISSING-QUEUE", "Missing queue entry in environment file '%s'", path);
        result = -1;
    } else if (result == 0) {
        LOG_FILE_PARSING("ENV-PARSE-SUCCESS",
                         "Parsed environment queue='%s' height=%d width=%d timeout=[%u,%u,%u] aging_start=%u aging_step=%u",
                         env_vars->queue,
                         env_vars->height,
                         env_vars->width,
                         env_vars->priority_timeouts[0],
                         env_vars->priority_timeouts[1],
                         env_vars->priority_timeouts[2],
                         env_vars->aging_start_seconds,
                         env_vars->aging_step_seconds);
    }

    return result;
}

