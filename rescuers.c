#include "rescuers.h"

#include <stdlib.h>

void free_rescuer_types(rescuer_type_t* types) {
    if (!types) {
        return;
    }

    for (size_t i = 0; types[i].rescuer_type_name != NULL; ++i) {
        free(types[i].rescuer_type_name);
        types[i].rescuer_type_name = NULL;
    }

    free(types);
}

void free_rescuer_twins(rescuer_digital_twin_t* twins) {
    free(twins);
}

