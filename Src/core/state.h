#ifndef STATE_H
#define STATE_H

#include "../core/types.h"

/* Erstellt bzw. zerstört den Systemzustand */
SystemState *state_create(uint32_t n_classes,
                          const uint32_t *instances);
void         state_destroy(SystemState *st);

/* Reserviert Speicher für die Matrizen (nachdem Prozesse bekannt sind) */
void         state_allocate_process_matrices(SystemState *st,
                                            uint32_t n_procs);

/* Setzt Werte in den Matrizen (wird vom Scheduler genutzt) */
void state_set_allocation(SystemState *st,
                          uint32_t pid,
                          uint32_t class_id,
                          uint32_t amount);
void state_set_request(SystemState *st,
                       uint32_t pid,
                       uint32_t class_id,
                       uint32_t amount);

/* Prüft mit dem Bankier-Algorithmus, ob der Zustand sicher ist */
bool state_is_safe(const SystemState *st);

/* Gibt den aktuellen Zustand auf der Konsole aus */
void state_print(const SystemState *st);

#endif /* STATE_H */