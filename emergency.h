#define EMERGENCY_NAME_LENGTH 64

typedef enum {
    WAITING, 
    ASSIGNED, 
    IN_PROGRESS,
    PAUSED, 
    COMPLETED, 
    CANCELED, 
    TIMEOUT
} emergency_status_t ;

typedef struct {
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int x;
    int y;
    time_t timestamp;
} emergency_request_t;

typedef struct emergency_t {
    emergency_type_t type; 
    emergency_status_t status;
    int x;
    int y;
    time_t time; 
    int rescuer_count;
    rescuer_digital_twin_t* rescuers_dt;
} emergency_t;
