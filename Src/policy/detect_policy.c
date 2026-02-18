#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t deadlocks;   /* Anzahl der abgelehnten bzw. verschobenen Requests */
    uint32_t n_procs;     /* Anzahl der Prozesse */
    uint32_t n_classes;   /* Anzahl der Ressourcenklassen */
} DetectCtx;

/* --------------------------------------------------------------
   Hilfsfunktion: Prüft ob es im Wait-For-Graphen einen Zyklus gibt.
   Prozess A wartet auf Prozess B, wenn:
     - A noch Ressourcen braucht (request[A][r] > 0)
     - B diese Ressourcenklasse bereits hält (allocation[B][r] > 0)
   Ein Zyklus im Wait-For-Graphen bedeutet einen Deadlock.
   -------------------------------------------------------------- */
static bool dfs_has_cycle(uint32_t node,
                          bool **adj,       /* Adjazenzmatrix des Wait-For-Graphen */
                          bool *visited,
                          bool *on_stack,
                          uint32_t n_procs)
{
    visited[node]  = true;
    on_stack[node] = true;

    for (uint32_t next = 0; next < n_procs; ++next) {
        if (!adj[node][next]) continue;
        if (!visited[next]) {
            if (dfs_has_cycle(next, adj, visited, on_stack, n_procs))
                return true;
        } else if (on_stack[next]) {
            return true;   /* Rückkante gefunden → Zyklus */
        }
    }
    on_stack[node] = false;
    return false;
}

static bool detect_cycle_in_state(const SystemState *st)
{
    uint32_t n = st->n_procs;

    /* Adjazenzmatrix aufbauen */
    bool **adj = calloc(n, sizeof(bool *));
    for (uint32_t i = 0; i < n; ++i)
        adj[i] = calloc(n, sizeof(bool));

    for (uint32_t a = 0; a < n; ++a) {
        for (uint32_t b = 0; b < n; ++b) {
            if (a == b) continue;
            /* A wartet auf B, wenn A eine Ressource braucht, die B hält */
            for (uint32_t r = 0; r < st->n_classes; ++r) {
                if (st->request[a][r] > 0 && st->allocation[b][r] > 0) {
                    adj[a][b] = true;
                    break;
                }
            }
        }
    }

    bool *visited  = calloc(n, sizeof(bool));
    bool *on_stack = calloc(n, sizeof(bool));
    bool  cycle    = false;

    for (uint32_t i = 0; i < n && !cycle; ++i) {
        if (!visited[i])
            cycle = dfs_has_cycle(i, adj, visited, on_stack, n);
    }

    for (uint32_t i = 0; i < n; ++i) free(adj[i]);
    free(adj);
    free(visited);
    free(on_stack);
    return cycle;
}

/* --------------------------------------------------------------
   on_request: echte Graph-Erkennung.
   Falls durch Genehmigung ein Zyklus entstehen würde,
   wird die Anfrage abgelehnt und der Zähler erhöht.
   -------------------------------------------------------------- */
static bool detect_on_request(Policy *p,
                              SystemState *st,
                              const Event *ev)
{
    DetectCtx *ctx = (DetectCtx *)p->private;

    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;

    /* Nicht genug freie Ressourcen → sofort ablehnen */
    if (amt > st->available[rc]) {
        ctx->deadlocks++;
        return false;
    }

    /* Simuliere die Vergabe */
    st->available[rc]              -= amt;
    st->allocation[ev->pid][rc]    += amt;
    st->request[ev->pid][rc]       -= amt;

    bool cycle = detect_cycle_in_state(st);

    /* Simulation rückgängig machen */
    st->available[rc]              += amt;
    st->allocation[ev->pid][rc]    -= amt;
    st->request[ev->pid][rc]       += amt;

    if (cycle) {
        ctx->deadlocks++;   /* Zyklus würde entstehen → Anfrage verschieben */
        return false;
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
