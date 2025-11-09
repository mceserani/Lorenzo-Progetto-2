#include <stdio.h>
#include <stdlib.h>
#include "parse_env.h" 
#include "parse_rescuers.h"
#include "parse_emergency_types.h"
#include "rescuers.h"
#include "emergency_types.h"


int main() {  
    rescuer_type_t* rescuer_types = NULL;
    rescuer_digital_twin_t* rescuer_twins = NULL;
    parse_rescuer_type("rescuers.txt", &rescuer_types, &rescuer_twins);
    printf("Parsed Rescuer Types:\n");
    for (size_t i = 0; rescuer_types && rescuer_types[i].rescuer_type_name; i++) {
        printf("Rescuer Type: %s, Speed: %d, X: %d, Y: %d\n",
               rescuer_types[i].rescuer_type_name,
               rescuer_types[i].speed,
               rescuer_types[i].x,
               rescuer_types[i].y);
    }
    printf("+\n");
    emergency_type_t* emergency_types = NULL;
    parse_emergency_type("emergency.txt", &emergency_types, rescuer_types); 
    
    printf("Parsed Emergency Types:\n");
    for (size_t i = 0; emergency_types && emergency_types[i].emergency_name != NULL; i++) {
        printf("Emergency Type: %s, Priority: %d\n",
               emergency_types[i].emergency_name,
               emergency_types[i].priority);
        for (int j = 0; j < emergency_types[i].rescuers_req_number; j++) {
            printf("  Rescuer Request: %s, Required Count: %d, Time to Manage: %d\n",
                   emergency_types[i].rescuer_requests[j].type->rescuer_type_name,
                   emergency_types[i].rescuer_requests[j].required_count,
                   emergency_types[i].rescuer_requests[j].time_to_manage);
        }
    }
    return 0; 
}