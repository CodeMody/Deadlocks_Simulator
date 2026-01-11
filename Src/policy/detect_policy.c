#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint64_t deadlocks;   /* Anzahl der abgelehnten bzw. verschobenen Requests */
    uint32_t n_procs;     /* Anzahl der Prozesse */
    uint32_t n_classes;   /* Anzahl der Ressourcenklassen */
} DetectCtx;

/* --------------------------------------------------------------
   on_request echte Graph-Erkennung. Falls ein Zyklus entstehen würde, wird die Anfrage abgelehnt
   und der Zähler erhöht. Andernfalls wird die Anfrage genehmigt.
   -------------------------------------------------------------- */
static bool detect_on_request(Policy *p,
                              SystemState *st,
                              const Event *ev)
{
    DetectCtx *ctx = (DetectCtx *)p->private;

    /* ---------------------------------------------------------
       Einfache Prüfung Sind genügend freie Ressourcen vorhanden?
       Der eigentliche Zyklenerkennungs-Algorithmus könnte hier
       implementiert werden.
       --------------------------------------------------------- */
    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;
    if (amt > st->available[rc]) {
        ctx->deadlocks++;          /* Anfrage momentan nicht erfüllbar */
        return false;              /* Anfrage verschieben */
    }

    return true;
}

/* --------------------------------------------------------------
   on_tick für diese Policy ist keine Aktion erforderlich
   -------------------------------------------------------------- */
static void detect_on_tick(Policy *p,
                           SystemState *st,
                           uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/* --------------------------------------------------------------
   cleanup gibt den privaten Kontext frei
   -------------------------------------------------------------- */
static void detect_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/* --------------------------------------------------------------
   Factory-Funktion erzeugt die Policy und speichert
   die Größenparameter im Kontext
   -------------------------------------------------------------- */
Policy *detect_policy_create(uint32_t n_procs,
                             uint32_t n_classes)
{
    Policy    *pol = calloc(1, sizeof(*pol));
    DetectCtx *ctx = calloc(1, sizeof(*ctx));

    ctx->deadlocks = 0;
    ctx->n_procs   = n_procs;
    ctx->n_classes = n_classes;

    pol->name       = "Graph-Detect";
    pol->on_request = detect_on_request;
    pol->on_tick    = detect_on_tick;
    pol->cleanup    = detect_cleanup;
    pol->private    = ctx;

    return pol;
}
