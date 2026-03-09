/**
 * @file detect_policy.c
 * @brief Implementierung der Wait-For-Graph-Zykluserkennungs-Policy.
 *
 * Diese Policy erkennt potenzielle Deadlocks, indem sie nach jeder
 * Ressourcenanforderung den Wait-For-Graphen aufbaut und mittels
 * iterativer Tiefensuche (DFS) auf Zyklen prüft. Ein Zyklus bedeutet,
 * dass eine zirkuläre Warteabhängigkeit entstehen würde — eine der
 * notwendigen Bedingungen für einen Deadlock.
 *
 * Prozess A wartet auf Prozess B (@c adj[A][B] = true), wenn:
 * - A mindestens eine Ressource noch benötigt (request[A][r] > 0) und
 * - B genau diese Ressourcenklasse aktuell hält (allocation[B][r] > 0).
 *
 * Die Anfrage wird nur genehmigt, wenn durch die simulierte Zuweisung
 * kein Zyklus im Wait-For-Graphen entsteht.
 */

#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Privater Kontext der Graph-Erkennungs-Policy.
 */
typedef struct {
    uint64_t deadlocks; /**< Anzahl der abgelehnten (verschobenen) Anfragen. */
    uint32_t n_procs;   /**< Anzahl der simulierten Prozesse. */
    uint32_t n_classes; /**< Anzahl der Ressourcenklassen. */
} DetectCtx;

/**
 * @brief Tiefensuche zur Zykliserkennung im Wait-For-Graphen.
 *
 * Klassische DFS mit einem "on-stack"-Bit: Wird ein Knoten erreicht,
 * der sich bereits auf dem aktuellen DFS-Pfad befindet, wurde eine
 * Rückkante gefunden — es existiert ein Zyklus.
 *
 * @param node      Aktueller Startknoten der DFS.
 * @param adj       Adjazenzmatrix des Wait-For-Graphen (n_procs × n_procs).
 * @param visited   Markierungsarray: Knoten wurden bereits besucht.
 * @param on_stack  Markierungsarray: Knoten befinden sich auf dem DFS-Pfad.
 * @param n_procs   Gesamtanzahl der Knoten (Prozesse).
 * @return          @c true, wenn ein Zyklus gefunden wurde; sonst @c false.
 */
static bool dfs_has_cycle(uint32_t node,
                          bool **adj,
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
            return true;  /* Rückkante entdeckt → Zyklus vorhanden */
        }
    }
    on_stack[node] = false;
    return false;
}

/**
 * @brief Baut den Wait-For-Graphen auf und prüft ihn auf Zyklen.
 *
 * Für jedes Prozess-Paar (A, B) wird eine Kante A → B eingetragen,
 * wenn A eine Ressourcenklasse benötigt, die B aktuell hält.
 * Anschließend wird eine DFS von jedem noch nicht besuchten Knoten
 * aus gestartet.
 *
 * @param st  Aktueller Systemzustand (const).
 * @return    @c true, wenn ein Zyklus (Deadlock) erkannt wurde; sonst @c false.
 */
static bool detect_cycle_in_state(const SystemState *st)
{
    uint32_t n = st->n_procs;

    /* Adjazenzmatrix für den Wait-For-Graphen aufbauen */
    bool **adj = calloc(n, sizeof(bool *));
    for (uint32_t i = 0; i < n; ++i)
        adj[i] = calloc(n, sizeof(bool));

    for (uint32_t a = 0; a < n; ++a) {
        for (uint32_t b = 0; b < n; ++b) {
            if (a == b) continue;
            /* Kante A → B: A wartet auf eine Ressource, die B hält */
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

    /* DFS von jedem nicht besuchten Knoten starten */
    for (uint32_t i = 0; i < n && !cycle; ++i) {
        if (!visited[i])
            cycle = dfs_has_cycle(i, adj, visited, on_stack, n);
    }

    /* Speicher freigeben */
    for (uint32_t i = 0; i < n; ++i) free(adj[i]);
    free(adj);
    free(visited);
    free(on_stack);
    return cycle;
}

/**
 * @brief Prüft eine Ressourcenanfrage mit der Graph-Zykliserkennung.
 *
 * Werden nicht ausreichend Ressourcen verfügbar, wird sofort abgelehnt.
 * Ansonsten wird die Vergabe temporär simuliert, der Wait-For-Graph
 * auf Zyklen geprüft und die Simulation wieder rückgängig gemacht.
 * Bei erkanntem Zyklus wird der Zähler erhöht und @c false zurückgegeben.
 *
 * @param p   Zeiger auf das Policy-Objekt (inkl. @c DetectCtx).
 * @param st  Aktueller Systemzustand.
 * @param ev  Das zu prüfende Anforderungsereignis.
 * @return    @c true, wenn die Anfrage ohne Zyklus genehmigt werden kann; sonst @c false.
 */
static bool detect_on_request(Policy *p,
                              SystemState *st,
                              const Event *ev)
{
    DetectCtx *ctx = (DetectCtx *)p->private;

    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;

    /* Sofortablehnug bei unzureichenden verfügbaren Ressourcen */
    if (amt > st->available[rc]) {
        ctx->deadlocks++;
        return false;
    }

    /* Temporäre Simulation der Vergabe */
    st->available[rc]              -= amt;
    st->allocation[ev->pid][rc]    += amt;
    st->request[ev->pid][rc]       -= amt;

    bool cycle = detect_cycle_in_state(st);

    /* Simulation rückgängig machen */
    st->available[rc]              += amt;
    st->allocation[ev->pid][rc]    -= amt;
    st->request[ev->pid][rc]       += amt;

    if (cycle) {
        ctx->deadlocks++;  /* Zyklus würde entstehen → Anfrage verschieben */
        return false;
    }
    return true;
}

/**
 * @brief Periodischer Tick-Callback der Graph-Erkennungs-Policy.
 *
 * Für diese Policy sind keine periodischen Aktionen erforderlich.
 * Alle Parameter werden bewusst ignoriert.
 *
 * @param p    Nicht verwendet.
 * @param st   Nicht verwendet.
 * @param now  Nicht verwendet.
 */
static void detect_on_tick(Policy *p,
                           SystemState *st,
                           uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/**
 * @brief Gibt alle von der Graph-Erkennungs-Policy belegten Ressourcen frei.
 *
 * @param p  Zeiger auf die freizugebende Policy.
 */
static void detect_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/**
 * @brief Factory-Funktion: Erstellt eine neue Graph-Erkennungs-Policy.
 *
 * Allokiert und initialisiert die @c Policy-Struktur sowie den
 * privaten @c DetectCtx. Setzt den Namen auf "Graph-Detect".
 *
 * @param n_procs    Anzahl der simulierten Prozesse.
 * @param n_classes  Anzahl der Ressourcenklassen.
 * @return           Zeiger auf die neu erstellte @c Policy.
 */
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
