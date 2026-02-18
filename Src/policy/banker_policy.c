/**
 * @file banker_policy.c
 * @brief Implementierung der Bankier-Algorithmus-Policy.
 *
 * Diese Policy implementiert Dijkstras Bankier-Algorithmus zur
 * Deadlock-Vermeidung. Bei jeder Ressourcenanforderung wird die
 * Zuweisung zunächst simuliert und anschließend der Systemzustand
 * auf Sicherheit geprüft. Nur wenn nach der simulierten Vergabe
 * noch ein sicherer Ablaufplan für alle Prozesse existiert, wird
 * die Anfrage genehmigt. Andernfalls wird sie verschoben.
 *
 * Der private Kontext (@c BankerCtx) zählt die Anzahl abgelehnter
 * Anfragen, die als potenzielle Deadlock-Situationen gewertet werden.
 */

#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Privater Kontext der Bankier-Policy.
 *
 * Wird im @c private-Feld der @c Policy-Struktur gespeichert und
 * enthält statistische Informationen sowie Konfigurationsparameter.
 */
typedef struct {
    uint64_t deadlocks; /**< Anzahl der abgelehnten (verschobenen) Anfragen. */
    uint32_t n_procs;   /**< Anzahl der simulierten Prozesse. */
    uint32_t n_res;     /**< Anzahl der Ressourcenklassen. */
} BankerCtx;

/**
 * @brief Prüft eine Ressourcenanfrage mit dem Bankier-Algorithmus.
 *
 * Simuliert die Zuweisung der angeforderten Ressourcen, indem die
 * Matrizen temporär aktualisiert werden. Anschließend wird
 * @c state_is_safe() aufgerufen. Nach der Prüfung werden alle
 * temporären Änderungen rückgängig gemacht, sodass der Systemzustand
 * unverändert bleibt. Nur bei einem sicheren Zustand wird @c true
 * zurückgegeben.
 *
 * @param p   Zeiger auf das Policy-Objekt (inkl. @c BankerCtx).
 * @param st  Aktueller Systemzustand.
 * @param ev  Das zu prüfende Anforderungsereignis.
 * @return    @c true, wenn die Anfrage sicher genehmigt werden kann; sonst @c false.
 */
static bool banker_on_request(Policy *p,
                              SystemState *st,
                              const Event *ev)
{
    BankerCtx *ctx = (BankerCtx *)p->private;

    uint32_t pid = ev->pid;
    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;

    /* Temporäre Simulation der Ressourcenvergabe */
    st->available[rc]       -= amt;
    st->allocation[pid][rc] += amt;
    st->request[pid][rc]    -= amt;

    bool safe = state_is_safe(st);

    /* Simulation rückgängig machen — Zustand bleibt unverändert */
    st->available[rc]       += amt;
    st->allocation[pid][rc] -= amt;
    st->request[pid][rc]    += amt;

    if (!safe) {
        ctx->deadlocks++;  /* Unsicherer Zustand: Anfrage wird verschoben */
        return false;
    }
    return true;           /* Sicherer Zustand: Anfrage wird genehmigt */
}

/**
 * @brief Periodischer Tick-Callback der Bankier-Policy.
 *
 * Der Bankier-Algorithmus benötigt keine periodischen Wartungsaktionen,
 * da die Prüfung vollständig in @c banker_on_request() erfolgt.
 * Alle Parameter werden bewusst ignoriert.
 *
 * @param p    Nicht verwendet.
 * @param st   Nicht verwendet.
 * @param now  Nicht verwendet.
 */
static void banker_on_tick(Policy *p,
                           SystemState *st,
                           uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/**
 * @brief Gibt alle von der Bankier-Policy belegten Ressourcen frei.
 *
 * Gibt den privaten Kontext (@c BankerCtx) und die Policy-Struktur selbst frei.
 *
 * @param p  Zeiger auf die freizugebende Policy.
 */
static void banker_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/**
 * @brief Factory-Funktion: Erstellt eine neue Bankier-Policy.
 *
 * Allokiert und initialisiert die @c Policy-Struktur sowie den
 * privaten @c BankerCtx. Belegt die Callbacks mit den
 * entsprechenden Funktionen und setzt den Namen auf "Banker (multi)".
 *
 * @param n_procs  Anzahl der simulierten Prozesse.
 * @param n_res    Anzahl der Ressourcenklassen.
 * @return         Zeiger auf die neu erstellte @c Policy.
 */
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
