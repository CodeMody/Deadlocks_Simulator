#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* Private context – just a counter */
typedef struct {
    uint64_t deadlocks;
    uint32_t n_procs;
    uint32_t n_res;
} HWCtx;

/* --------------------------------------------------------------
   on_request – if the process already holds *any* resource,
   we refuse the request (this eliminates the Hold‑and‑Wait condition).
   -------------------------------------------------------------- */
static bool hw_on_request(Policy *p,
                          SystemState *st,
                          const Event *ev)
{
    HWCtx *ctx = (HWCtx *)p->private;
    uint32_t pid = ev->pid;
    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;

    /* Does the process already hold something? */
    for (uint32_t r = 0; r < st->n_classes; ++r) {
        if (st->allocation[pid][r] > 0) {
            ctx->deadlocks++;          /* we are postponing */
            return false;
        }
    }

    /* Enough free instances? */
    if (amt > st->available[rc]) {
        ctx->deadlocks++;
        return false;
    }

    return true;   /* grant */
}

/* --------------------------------------------------------------
   on_tick – nothing needed.
   -------------------------------------------------------------- */
static void hw_on_tick(Policy *p,
                       SystemState *st,
                       uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/* --------------------------------------------------------------
   cleanup
   -------------------------------------------------------------- */
static void hw_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/* --------------------------------------------------------------
   Factory
   -------------------------------------------------------------- */
Policy *holdwait_policy_create(uint32_t n_procs,
                               uint32_t n_res)
{
    Policy *pol = calloc(1, sizeof(*pol));
    HWCtx  *ctx = calloc(1, sizeof(*ctx));

    ctx->deadlocks = 0;
    ctx->n_procs   = n_procs;
    ctx->n_res     = n_res;

    pol->name       = "Hold‑and‑Wait Elim";
    pol->on_request = hw_on_request;
    pol->on_tick    = hw_on_tick;
    pol->cleanup    = hw_cleanup;
    pol->private    = ctx;
    return pol;
}