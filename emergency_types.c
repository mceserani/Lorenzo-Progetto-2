#include "emergency_types.h"

#include <stdlib.h>

void free_emergency_types(emergency_type_t* emergency_types) {
    if (!emergency_types) {
        return;
    }

    for (size_t i = 0; emergency_types[i].emergency_name != NULL; ++i) {
        free(emergency_types[i].emergency_name);
        emergency_types[i].emergency_name = NULL;
        free(emergency_types[i].rescuer_requests);
        emergency_types[i].rescuer_requests = NULL;
        emergency_types[i].rescuers_req_number = 0;
    }

    free(emergency_types);
}

