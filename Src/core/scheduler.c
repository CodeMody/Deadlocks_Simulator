#define _XOPEN_SOURCE 600
#include "../core/scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* -----------------------------------------------------------------
   Scheduler internal representation.
   ----------------------------------------------------------------- */
struct Scheduler {
    SystemState *state;
    Policy      *policy;
    EventHeap   *heap;
    uint64_t     now;
};

/* -----------------------------------------------------------------
   Create / destroy
   ----------------------------------------------------------------- */
Scheduler *scheduler_create(SystemState *st, Policy *policy)
{
    Scheduler *s = calloc(1, sizeof(*s));
    s->state  = st;
    s->policy = policy;
    s->heap   = event_heap_create();
    s->now    = 0;
    return s;
}

void scheduler_destroy(Scheduler *s)
{
    if (!s) return;
    event_heap_destroy(s->heap);
    if (s->policy && s->policy->cleanup) s->policy->cleanup(s->policy);
    free(s);
}

/* -----------------------------------------------------------------
   Insert a new event.
   ----------------------------------------------------------------- */
void scheduler_schedule_event(Scheduler *sch, const Event *ev)
{
    event_push(sch->heap, ev);
}

/* -----------------------------------------------------------------
   Helper that actually updates the matrices.
   ----------------------------------------------------------------- */
static void handle_request(Scheduler *s, const Event *ev)
{
    bool grant = s->policy->on_request(s->policy, s->state, ev);

    /* -------------------------------------------------------------
       Even if the policy says “grant”, we must still verify that the
       resources are really free.  If not, treat it as a postponed request.
       ------------------------------------------------------------- */
    if (grant) {
        uint32_t pid   = ev->pid;
        uint32_t rc    = ev->class_id;
        uint32_t amt   = ev->amount;
        uint32_t avail = s->state->available[rc];
        uint32_t need  = s->state->request[pid][rc];

        if (amt > avail || amt > need) {
            grant = false;   /* not enough free instances or need */
        }
    }

    if (grant) {
        uint32_t pid = ev->pid;
        uint32_t rc  = ev->class_id;
        uint32_t amt = ev->amount;

        s->state->allocation[pid][rc] += amt;
        s->state->request[pid][rc]    -= amt;
        s->state->available[rc]       -= amt;
    } else {
        /* postpone – re‑queue one tick later */
        Event retry = *ev;
        retry.time = s->now + 1;
        event_push(s->heap, &retry);
    }
}

/* -----------------------------------------------------------------
   Run the simulation up to (and including) `until_time`.
   ----------------------------------------------------------------- */
void scheduler_run_until(Scheduler *s, uint64_t until_time)
{
    Event ev;

    while (event_peek(s->heap, &ev) && ev.time <= until_time) {
        event_pop(s->heap, &ev);
        s->now = ev.time;

        switch (ev.type) {
            case EV_REQUEST:
                handle_request(s, &ev);
                break;

            case EV_RELEASE:
                s->state->allocation[ev.pid][ev.class_id] -= ev.amount;
                s->state->available[ev.class_id]          += ev.amount;
                break;

            case EV_TERMINATE:
                for (uint32_t r = 0; r < s->state->n_classes; ++r) {
                    uint32_t held = s->state->allocation[ev.pid][r];
                    if (held) {
                        s->state->allocation[ev.pid][r] = 0;
                        s->state->available[r]        += held;
                    }
                }
                break;

            case EV_CHECKPOINT:
                /* not used in the minimal demo */
                break;

            default:
                fprintf(stderr,
                        "scheduler: unknown event type %d (pid=%u, time=%lu)\n",
                        ev.type, ev.pid, ev.time);
                break;
        }

        if (s->policy->on_tick)
            s->policy->on_tick(s->policy, s->state, s->now);
    }
}

/* -----------------------------------------------------------------
   Accessors (useful for printing / GUI)
   ----------------------------------------------------------------- */
uint64_t scheduler_current_time(const Scheduler *s) { return s->now; }
SystemState *scheduler_state(const Scheduler *s)   { return s->state; }
Policy *scheduler_policy(const Scheduler *s)       { return s->policy; }