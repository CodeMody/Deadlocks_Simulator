#ifndef STATE_H
#define STATE_H

#include "../core/types.h"

/* Creation / destruction */
SystemState *state_create(uint32_t n_classes,
                          const uint32_t *instances);
void         state_destroy(SystemState *st);

/* Allocate the two matrices once you know how many processes you will have */
void         state_allocate_process_matrices(SystemState *st,
                                            uint32_t n_procs);

/* Simple setters – used by the scheduler */
void state_set_allocation(SystemState *st,
                          uint32_t pid,
                          uint32_t class_id,
                          uint32_t amount);
void state_set_request(SystemState *st,
                       uint32_t pid,
                       uint32_t class_id,
                       uint32_t amount);

/* Safety test – used by the Banker policies */
bool state_is_safe(const SystemState *st);

/* Debug / pretty‑print */
void state_print(const SystemState *st);

#endif /* STATE_H */