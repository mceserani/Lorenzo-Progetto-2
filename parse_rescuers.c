#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emergency_types.h"
#include "rescuers.h"

int parse_rescuer_type(const char* path,
                       rescuer_type_t** rescuer_types,
                       size_t* out_rescuer_type_count,
                       rescuer_digital_twin_t** out_rescuer_twins,
                       size_t* out_rescuer_twin_count) {
    if (!path || !rescuer_types || !out_rescuer_type_count || !out_rescuer_twins || !out_rescuer_twin_count) {
        return -1;
    }

    *rescuer_types = NULL;
    *out_rescuer_type_count = 0;
    *out_rescuer_twins = NULL;
    *out_rescuer_twin_count = 0;

    FILE* file = fopen(path, "r");
    if (!file) {
        perror("Errore nell'apertura del file");
        return -1;
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    size_t type_count = 0;
    size_t total_twin_count = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        char* saveptr;
        char* tok_name = strtok_r(line, "][", &saveptr);
        char* tok_num = strtok_r(NULL, "][", &saveptr);
        char* tok_speed = strtok_r(NULL, "][", &saveptr);
        char* tok_x = strtok_r(NULL, "[;", &saveptr);
        char* tok_y = strtok_r(NULL, "]\n", &saveptr);

        if (tok_name && tok_num && tok_speed && tok_x && tok_y) {
            type_count++;
            total_twin_count += (size_t)atoi(tok_num);
        }
    }

    if (type_count == 0) {
        free(line);
        fclose(file);
        return 0;
    }

    rescuer_type_t* types = calloc(type_count + 1, sizeof(rescuer_type_t));
    if (!types) {
        perror("Errore di allocazione (pass 2) per rescuer_types");
        free(line);
        fclose(file);
        return -1;
    }

    rescuer_digital_twin_t* twins = calloc(total_twin_count, sizeof(rescuer_digital_twin_t));
    if (!twins) {
        perror("Errore di allocazione (pass 2) per out_rescuer_twins");
        free(types);
        free(line);
        fclose(file);
        return -1;
    }

    rewind(file);

    size_t current_type_idx = 0;
    size_t current_twin_idx = 0;
    int status = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        char* saveptr;
        char* tok_name = strtok_r(line, "][", &saveptr);
        char* tok_num = strtok_r(NULL, "][", &saveptr);
        char* tok_speed = strtok_r(NULL, "][", &saveptr);
        char* tok_x = strtok_r(NULL, "[;", &saveptr);
        char* tok_y = strtok_r(NULL, "]\n", &saveptr);

        if (tok_name && tok_num && tok_speed && tok_x && tok_y) {
            rescuer_type_t* current_type_ptr = &types[current_type_idx];
            current_type_ptr->rescuer_type_name = strdup(tok_name);
            if (!current_type_ptr->rescuer_type_name) {
                status = -1;
                break;
            }
            current_type_ptr->speed = atoi(tok_speed);
            current_type_ptr->x = atoi(tok_x);
            current_type_ptr->y = atoi(tok_y);

            int num_twins_for_this = atoi(tok_num);
            for (int i = 0; i < num_twins_for_this; i++) {
                rescuer_digital_twin_t* current_twin_ptr = &twins[current_twin_idx];
                current_twin_ptr->id = (int)current_twin_idx + 1;
                current_twin_ptr->x = current_type_ptr->x;
                current_twin_ptr->y = current_type_ptr->y;
                current_twin_ptr->status = IDLE;
                current_twin_ptr->type = current_type_ptr;
                current_twin_idx++;
            }

            current_type_idx++;
        }
    }

    free(line);
    fclose(file);

    if (status != 0) {
        free_rescuer_types(types);
        free_rescuer_twins(twins);
        return status;
    }

    *rescuer_types = types;
    *out_rescuer_twins = twins;
    *out_rescuer_type_count = type_count;
    *out_rescuer_twin_count = total_twin_count;

    return 0;
}

