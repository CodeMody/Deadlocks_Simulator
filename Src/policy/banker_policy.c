#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* Privater Kontext enthält einen Zähler */
typedef struct {
    uint64_t deadlocks;   /* Anzahl der abgelehnten Requests */
    uint32_t n_procs;     /* Anzahl der Prozesse */
    uint32_t n_res;       /* Anzahl der Ressourcenklassen */
} BankerCtx;

/* --------------------------------------------------------------
   on_request simuliert die Ressourcenanforderung,
   führt den Safety-Check des Bankiers aus und
   genehmigt die Anfrage nur, wenn der resultierende Zustand sicher ist.
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
       Schneller test sind genügend freie Instanzen vorhanden?
       --------------------------------------------------------- */
    if (amt > st->available[rc]) {
        ctx->deadlocks++;
        return false;          /* Anfrage verschieben */
    }

    /* ---------------------------------------------------------
       Simulation der Ressourcenvergabe
       --------------------------------------------------------- */
    st->available[rc]       -= amt;
    st->allocation[pid][rc] += amt;
    st->request[pid][rc]    -= amt;

    bool safe = state_is_safe(st);

    /* ---------------------------------------------------------
       Rückgängigmachen der Simulation
       --------------------------------------------------------- */
    st->available[rc]       += amt;
    st->allocation[pid][rc] -= amt;
    st->request[pid][rc]    += amt;

    if (!safe) {
        ctx->deadlocks++;      /* würde zu einem Deadlock führen → verschieben */
        return false;
    }
    return true;               /* sicherer Zustand → Anfrage genehmigen */
}

/* --------------------------------------------------------------
   on_tick keine Aktion erforderlich
   -------------------------------------------------------------- */
static void banker_on_tick(Policy *p,
                           SystemState *st,
                           uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/* --------------------------------------------------------------
   cleanup gibt alle von der Policy belegten Ressourcen frei
   -------------------------------------------------------------- */
static void banker_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/* --------------------------------------------------------------
   Factory-Funktion erzeugt eine neue Banker-Policy
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
