#define _GNU_SOURCE
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include "emergency_types.h" // Assicurati che queste definizioni...
#include "rescuers.h"        // ...siano corrette e includano 'IDLE'

/**
 * Funzione di parsing sicura in due passate.
 * 1. Legge il file per contare il numero di tipi e il numero totale di "gemelli".
 * 2. Alloca tutta la memoria necessaria in un unico blocco.
 * 3. Rilegge il file per popolare gli array, stabilendo i collegamenti in sicurezza.
 */
int parse_rescuer_type(const char* path, rescuer_type_t** rescuer_types, rescuer_digital_twin_t** out_rescuer_twins) {
    
    FILE* file = fopen(path, "r");
    if (!file) {
        perror("Errore nell'apertura del file");
        exit(1);
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    
    size_t type_count = 0;
    size_t total_twin_count = 0;

    // -----------------------------------------------------------------
    // --- PASSATA 1: Contare gli elementi per l'allocazione ---
    // -----------------------------------------------------------------
    while ((read = getline(&line, &len, file)) != -1) {
        // NOTA: strtok_r modifica 'line'. Per la seconda passata,
        // getline() rileggerà comunque la riga dal file, sovrascrivendo
        // il buffer 'line' con il contenuto originale, quindi è sicuro.
        
        char* saveptr;
        char* tok_name = strtok_r(line, "][", &saveptr);
        char* tok_num = strtok_r(NULL, "][", &saveptr);
        char* tok_speed = strtok_r(NULL, "][", &saveptr);
        char* tok_x = strtok_r(NULL, "[;", &saveptr);
        char* tok_y = strtok_r(NULL, "]\n", &saveptr);
        
        // Contiamo solo le righe ben formate
        if(tok_name && tok_num && tok_speed && tok_x && tok_y) {
            type_count++;
            total_twin_count += atoi(tok_num);
        }
    }

    // Se il file è vuoto o malformato, esci
    if (type_count == 0) {
        *rescuer_types = NULL;
        *out_rescuer_twins = NULL;
        free(line);
        fclose(file);
        return 0;
    }

    // -----------------------------------------------------------------
    // --- Allocazione Unica (pre-passata 2) ---
    // -----------------------------------------------------------------
    
    // Alloca spazio per tutti i tipi + 1 per il terminatore NULL
    // Usiamo calloc per azzerare tutta la memoria:
    // questo imposta automaticamente il terminatore a NULL.
    *rescuer_types = calloc(type_count + 1, sizeof(rescuer_type_t));
    if (!*rescuer_types) {
        perror("Errore di allocazione (pass 2) per rescuer_types");
        free(line);
        fclose(file);
        exit(1);
    }

    // Alloca spazio per il numero esatto di "gemelli"
    *out_rescuer_twins = calloc(total_twin_count, sizeof(rescuer_digital_twin_t));
    if (!*out_rescuer_twins) {
        perror("Errore di allocazione (pass 2) per out_rescuer_twins");
        free(*rescuer_types); // Libera la memoria già allocata
        free(line);
        fclose(file);
        exit(1);
    }

    // -----------------------------------------------------------------
    // --- PASSATA 2: Popolare gli array ---
    // -----------------------------------------------------------------

    rewind(file); // Torna all'inizio del file

    size_t current_type_idx = 0;
    size_t current_twin_idx = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        char* saveptr;
        char* tok_name = strtok_r(line, "][", &saveptr);
        char* tok_num = strtok_r(NULL, "][", &saveptr);
        char* tok_speed = strtok_r(NULL, "][", &saveptr);
        char* tok_x = strtok_r(NULL, "[;", &saveptr);
        char* tok_y = strtok_r(NULL, "]\n", &saveptr);
        
        if(tok_name && tok_num && tok_speed && tok_x && tok_y) {
            
            // Ottieni un puntatore alla struct tipo corrente (già allocata)
            rescuer_type_t *current_type_ptr = &((*rescuer_types)[current_type_idx]);
            current_type_ptr->rescuer_type_name = strdup(tok_name);
            current_type_ptr->speed = atoi(tok_speed);
            current_type_ptr->x = atoi(tok_x);
            current_type_ptr->y = atoi(tok_y);
            
            // Gestisci i "gemelli"
            int num_twins_for_this = atoi(tok_num);
            for (int i = 0; i < num_twins_for_this; i++) {
                
                // Ottieni un puntatore alla struct gemello corrente (già allocata)
                rescuer_digital_twin_t *current_twin_ptr = &((*out_rescuer_twins)[current_twin_idx]);
                current_twin_ptr->id = current_twin_idx + 1; // ID progressivo
                current_twin_ptr->x = current_type_ptr->x;
                current_twin_ptr->y = current_type_ptr->y;
                current_twin_ptr->status = IDLE;
                
                // Questo è il collegamento critico.
                // Ora è sicuro perché 'current_type_ptr' punta a memoria 
                // che non sarà mai più spostata da realloc.
                current_twin_ptr->type = current_type_ptr;
                
                current_twin_idx++; // Avanza al prossimo slot gemello
            }
            
            current_type_idx++; // Avanza al prossimo slot tipo
        }
    }

    // -----------------------------------------------------------------
    // --- Pulizia ---
    // -----------------------------------------------------------------
    
    // Nota: Il terminatore (*rescuer_types)[type_count].rescuer_type_name
    // è già NULL grazie a calloc(). Non c'è bisogno di impostarlo di nuovo.

    free(line); // Libera la memoria allocata da getline
    fclose(file);
    
    return type_count; // Restituisce il numero di tipi letti
}