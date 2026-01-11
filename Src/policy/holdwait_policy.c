#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint64_t deadlocks;   /* Anzahl der verschobenen Anfragen */
    uint32_t n_procs;     /* Anzahl der Prozesse */
    uint32_t n_res;       /* Anzahl der Ressourcenklassen */
} HWCtx;

/* --------------------------------------------------------------
   on_request Falls ein Prozess bereits Ressource hält, wird die Anfrage abgelehnt.
   Dadurch wird die Hold-and-Wait-Bedingung eliminiert.
   -------------------------------------------------------------- */
static bool hw_on_request(Policy *p,
                          SystemState *st,
                          const Event *ev)
{
    HWCtx *ctx = (HWCtx *)p->private;
    uint32_t pid = ev->pid;
    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;

    /* Prüfen, ob der Prozess bereits Ressourcen hält */
    for (uint32_t r = 0; r < st->n_classes; ++r) {
        if (st->allocation[pid][r] > 0) {
            ctx->deadlocks++;          /* Anfrage wird verschoben */
            return false;
        }
    }

    /* Prüfen, ob genügend freie Instanzen vorhanden sind */
    if (amt > st->available[rc]) {
        ctx->deadlocks++;              /* aktuell nicht erfüllbar */
        return false;
    }

    return true;   /* Anfrage genehmigen */
}

/* --------------------------------------------------------------
   on_tick für diese Policy ist keine Aktion erforderlich
   -------------------------------------------------------------- */
static void hw_on_tick(Policy *p,
                       SystemState *st,
                       uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/* --------------------------------------------------------------
   cleanup gibt alle von der Policy belegten Ressourcen frei
   -------------------------------------------------------------- */
static void hw_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/* --------------------------------------------------------------
   Factory-Funktion erzeugt eine Hold-and-Wait-Policy
   -------------------------------------------------------------- */
Policy *holdwait_policy_create(uint32_t n_procs,
                               uint32_t n_res)
{
    Policy *pol = calloc(1, sizeof(*pol));
    HWCtx  *ctx = calloc(1, sizeof(*ctx));

    ctx->deadlocks = 0;
    ctx->n_procs   = n_procs;
    ctx->n_res     = n_res;

    pol->name       = "Hold-and-Wait Elim";
    pol->on_request = hw_on_request;
    pol->on_tick    = hw_on_tick;
    pol->cleanup    = hw_cleanup;
    pol->private    = ctx;

    return pol;
}
