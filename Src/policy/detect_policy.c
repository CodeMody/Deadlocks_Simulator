#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* Private context – just a counter */
typedef struct {
    uint64_t deadlocks;   /* how many times we refused a request */
    uint32_t n_procs;
    uint32_t n_classes;
} DetectCtx;

/* --------------------------------------------------------------
   on_request – real graph detection (simplified for the demo).
   If a cycle would appear we *refuse* the request and increment
   the counter.  Otherwise we grant it.
   -------------------------------------------------------------- */
static bool detect_on_request(Policy *p,
                              SystemState *st,
                              const Event *ev)
{
    DetectCtx *ctx = (DetectCtx *)p->private;

    /* ---------------------------------------------------------
       Very small “is there enough free resource?” check.
       The real cycle‑detection algorithm can be inserted here.
       --------------------------------------------------------- */
    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;
    if (amt > st->available[rc]) {
        ctx->deadlocks++;          /* request cannot be satisfied now */
        return false;              /* postpone */
    }

    /* No cycle detection for the demo – always grant if enough free */
    return true;
}

/* --------------------------------------------------------------
   on_tick – nothing needed for this policy.
   -------------------------------------------------------------- */
static void detect_on_tick(Policy *p,
                           SystemState *st,
                           uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/* --------------------------------------------------------------
   cleanup – free the private context.
   -------------------------------------------------------------- */
static void detect_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/* --------------------------------------------------------------
   Factory – allocate the context and store the size values.
   -------------------------------------------------------------- */
Policy *detect_policy_create(uint32_t n_procs,
                             uint32_t n_classes)
{
    Policy   *pol = calloc(1, sizeof(*pol));
    DetectCtx *ctx = calloc(1, sizeof(*ctx));

    ctx->deadlocks  = 0;
    ctx->n_procs    = n_procs;
    ctx->n_classes  = n_classes;

    pol->name       = "Graph‑Detect";
    pol->on_request = detect_on_request;
    pol->on_tick    = detect_on_tick;
    pol->cleanup    = detect_cleanup;
    pol->private    = ctx;
    return pol;
}