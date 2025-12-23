#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../core/types.h"
#include "event.h"
#include "../policy/policy.h"

/* Opaque scheduler type */
typedef struct Scheduler Scheduler;

/* Public API */
Scheduler *scheduler_create(SystemState *st, Policy *policy);
void       scheduler_destroy(Scheduler *sch);

void       scheduler_schedule_event(Scheduler *sch, const Event *ev);
void       scheduler_run_until(Scheduler *sch, uint64_t until_time);

uint64_t   scheduler_current_time(const Scheduler *sch);
SystemState *scheduler_state(const Scheduler *sch);
Policy    *scheduler_policy(const Scheduler *sch);

#endif /* SCHEDULER_H */