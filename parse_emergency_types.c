#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emergency_types.h"
#include "rescuers.h"

static rescuer_type_t* find_rescuer_type_by_name(const char* name, rescuer_type_t* types_list) {
    if (!name || !types_list) return NULL;
    for (size_t i = 0; types_list[i].rescuer_type_name; i++) {
        if (strcmp(types_list[i].rescuer_type_name, name) == 0) {
            return &types_list[i];
        }
    }
    return NULL;
}

int parse_emergency_type(const char* path,
                         emergency_type_t** out_emergency_types,
                         size_t* out_emergency_count,
                         rescuer_type_t* all_rescuer_types,
                         size_t rescuer_type_count)
{
    if (!path || !out_emergency_types || !out_emergency_count) {
        return -1;
    }

    (void)rescuer_type_count;

    *out_emergency_types = NULL;
    *out_emergency_count = 0;

    FILE* file = fopen(path, "r");
    if (!file) {
        perror("Errore nell'apertura del file");
        return -1;
    }

    char* line = NULL;
    size_t len = 0;

    size_t emergency_count = 0;
    size_t* request_counts_per_emergency = NULL;

    int status = 0;

    while (getline(&line, &len, file) != -1) {
        char* saveptr_line;
        strtok_r(line, "[", &saveptr_line);
        char* tok_name = strtok_r(NULL, "][", &saveptr_line);
        char* tok_priority = strtok_r(NULL, "]", &saveptr_line);

        if (tok_name && tok_priority) {
            size_t* new_counts = realloc(request_counts_per_emergency, (emergency_count + 1) * sizeof(size_t));
            if (!new_counts) {
                perror("Errore realloc contatori pass 1");
                status = -1;
                break;
            }
            request_counts_per_emergency = new_counts;

            size_t current_request_count = 0;

            char* saveptr_req;
            char* tok_name_resc = strtok_r(saveptr_line, ":;", &saveptr_req);

            while (tok_name_resc) {
                char* tok_required_count = strtok_r(NULL, ",", &saveptr_req);
                char* tok_time_to_manage = strtok_r(NULL, ";", &saveptr_req);

                if (tok_required_count && tok_time_to_manage) {
                    current_request_count++;
                }

                tok_name_resc = strtok_r(NULL, ":;", &saveptr_req);
            }

            request_counts_per_emergency[emergency_count] = current_request_count;
            emergency_count++;
        }
    }

    if (status != 0) {
        free(line);
        free(request_counts_per_emergency);
        fclose(file);
        return status;
    }

    if (emergency_count == 0) {
        free(line);
        free(request_counts_per_emergency);
        fclose(file);
        return 0;
    }

    emergency_type_t* emergencies = calloc(emergency_count + 1, sizeof(emergency_type_t));
    if (!emergencies) {
        perror("Errore calloc emergency_types");
        free(line);
        free(request_counts_per_emergency);
        fclose(file);
        return -1;
    }

    rewind(file);
    size_t current_emergency_idx = 0;

    while (getline(&line, &len, file) != -1) {
        char* saveptr_line;
        strtok_r(line, "[", &saveptr_line);
        char* tok_name = strtok_r(NULL, "][", &saveptr_line);
        char* tok_priority = strtok_r(NULL, "]", &saveptr_line);

        if (tok_name && tok_priority) {
            emergency_type_t* current_emergency = &emergencies[current_emergency_idx];

            current_emergency->emergency_name = strdup(tok_name);
            if (!current_emergency->emergency_name) {
                status = -1;
                break;
            }
            current_emergency->priority = (short)atoi(tok_priority);

            size_t num_requests = request_counts_per_emergency[current_emergency_idx];
            current_emergency->rescuers_req_number = (int)num_requests;

            if (num_requests > 0) {
                current_emergency->rescuer_requests = calloc(num_requests, sizeof(rescuer_request_t));
                if (!current_emergency->rescuer_requests) {
                    perror("Errore calloc rescuer_requests");
                    status = -1;
                    break;
                }

                char* saveptr_req;
                char* tok_name_resc = strtok_r(saveptr_line, ":;", &saveptr_req);

                for (size_t i = 0; i < num_requests; i++) {
                    char* tok_required_count = strtok_r(NULL, ",", &saveptr_req);
                    char* tok_time_to_manage = strtok_r(NULL, ";", &saveptr_req);

                    rescuer_request_t* current_request = &current_emergency->rescuer_requests[i];

                    current_request->type = find_rescuer_type_by_name(tok_name_resc, all_rescuer_types);
                    current_request->required_count = tok_required_count ? atoi(tok_required_count) : 0;
                    current_request->time_to_manage = tok_time_to_manage ? atoi(tok_time_to_manage) : 0;

                    tok_name_resc = strtok_r(NULL, ":;", &saveptr_req);
                }
            }

            current_emergency_idx++;
        }
    }

    free(line);
    free(request_counts_per_emergency);
    fclose(file);

    if (status != 0) {
        free_emergency_types(emergencies);
        return status;
    }

    *out_emergency_types = emergencies;
    *out_emergency_count = emergency_count;

    return 0;
}

