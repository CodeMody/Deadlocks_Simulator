#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* Privater Kontext – nur ein Zähler */
typedef struct {
    uint64_t deadlocks;   /* how many times we refused a request */
    uint32_t n_procs;
    uint32_t n_res;
} BankerCtx;

/* --------------------------------------------------------------
on_request – Simuliert die Anfrage, führt die Sicherheitsprüfung durch,
und gewährt die Berechtigung nur, wenn der resultierende Zustand sicher ist.
   -------------------------------------------------------------- */
static bool banker_on_request(Policy *p,
                              SystemState *st,
                              const Event *ev)
{
    BankerCtx *ctx = (BankerCtx *)p->private;

    uint32_t pid = ev->pid;
    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;

    /* ---------------------------------------------------------
       Quick check: enough free instances?
       --------------------------------------------------------- */
    if (amt > st->available[rc]) {
        ctx->deadlocks++;
        return false;          /* postpone */
    }

    /* ---------------------------------------------------------
       Simulate the allocation
       --------------------------------------------------------- */
    st->available[rc]       -= amt;
    st->allocation[pid][rc] += amt;
    st->request[pid][rc]    -= amt;

    bool safe = state_is_safe(st);

    /* ---------------------------------------------------------
       Roll back the simulation
       --------------------------------------------------------- */
    st->available[rc]       += amt;
    st->allocation[pid][rc] -= amt;
    st->request[pid][rc]    += amt;

    if (!safe) {
        ctx->deadlocks++;      /* would lead to deadlock → postpone */
        return false;
    }
    return true;               /* safe → grant */
}

/* --------------------------------------------------------------
   on_tick – nothing needed.
   -------------------------------------------------------------- */
static void banker_on_tick(Policy *p,
                           SystemState *st,
                           uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/* --------------------------------------------------------------
   cleanup
   -------------------------------------------------------------- */
static void banker_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/* --------------------------------------------------------------
   Factory
   -------------------------------------------------------------- */
Policy *banker_policy_create(uint32_t n_procs,
                             uint32_t n_res)
{
    Policy    *pol = calloc(1, sizeof(*pol));
    BankerCtx *ctx = calloc(1, sizeof(*ctx));

    ctx->deadlocks = 0;
    ctx->n_procs   = n_procs;
    ctx->n_res     = n_res;

    pol->name       = "Banker (multi)";
    pol->on_request = banker_on_request;
    pol->on_tick    = banker_on_tick;
    pol->cleanup    = banker_cleanup;
    pol->private    = ctx;
    return pol;

}
