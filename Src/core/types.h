#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>          /* size_t */

typedef enum {
    EV_REQUEST,
    EV_RELEASE,
    EV_CHECKPOINT,
    EV_TERMINATE
} EventType;

/* --------------------------------------------------------------
   Event – a single entry in the discrete‑event queue.
   -------------------------------------------------------------- */
typedef struct {
    uint64_t   time;       /* logical time (ticks)                */
    EventType  type;       /* kind of event                       */
    uint32_t   pid;        /* process that generated the event    */
    uint32_t   class_id;   /* resource class affected              */
    uint32_t   amount;     /* #instances requested / released      */
} Event;

/* --------------------------------------------------------------
   Forward declaration of Policy – the full definition lives in
   policy.h.  This lets the callbacks use `struct Policy *`.
   -------------------------------------------------------------- */
struct Policy;

/* Callback prototypes (they need the forward declaration). */
typedef bool  (*policy_request_f)(struct Policy *p,
                                  struct SystemState *st,
                                  const Event *ev);
typedef void  (*policy_tick_f)(struct Policy *p,
                               struct SystemState *st,
                               uint64_t now);
typedef void  (*policy_cleanup_f)(struct Policy *p);

/* --------------------------------------------------------------
   SystemState – the *resource* part of the simulation.
   -------------------------------------------------------------- */
typedef struct SystemState {
    uint32_t n_classes;      /* number of distinct resource classes   */
    uint32_t n_procs;        /* **added** – number of processes       */
    uint32_t *instances;    /* total instances per class (E)          */
    uint32_t *available;    /* free instances per class   (A)          */
    uint32_t **allocation;  /* C matrix – proc × class (held)         */
    uint32_t **request;     /* R matrix – proc × class (still needed) */
} SystemState;

#endif /* TYPES_H */