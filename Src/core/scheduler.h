#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../core/types.h"
#include "event.h"
#include "../policy/policy.h"

/* Opaque Pointer Pattern = Information Hiding
 * Der Nutzer kann nur mit Pointern auf Scheduler arbeiten,
 * aber nicht direkt auf die Felder zugreifen
 */
typedef struct Scheduler Scheduler;

/* Öffentliche API
 *Erstellt und zerstört einen Scheduler*/
Scheduler *scheduler_create(SystemState *st, Policy *policy);
void       scheduler_destroy(Scheduler *sch);

/*Plant ein Ereignis ein und führt bis zu einem Zeitpunkt aus*/
void       scheduler_schedule_event(Scheduler *sch, const Event *ev);
void       scheduler_run_until(Scheduler *sch, uint64_t until_time);

/*Zugriff auf aktuelle Zeit, Systemzustand und Policy*/
uint64_t   scheduler_current_time(const Scheduler *sch);
SystemState *scheduler_state(const Scheduler *sch);
Policy    *scheduler_policy(const Scheduler *sch);

#endif /* SCHEDULER_H */