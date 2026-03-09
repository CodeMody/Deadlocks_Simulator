/**
 * @file holdwait_policy.c
 * @brief Implementierung der Hold-and-Wait-Eliminierungs-Policy.
 *
 * Diese Policy verhindert Deadlocks durch das Unterbinden der
 * Hold-and-Wait-Bedingung: Hält ein Prozess bereits Ressourcen
 * irgendeiner Klasse, wird jede neue Anforderung desselben
 * Prozesses abgelehnt und verschoben. Nur Prozesse ohne gehaltene
 * Ressourcen dürfen neue Ressourcen anfordern.
 *
 * Dieses Verfahren ist einfach umzusetzen und sehr effizient,
 * kann jedoch zu Starvation führen, da Prozesse unter Umständen
 * dauerhaft auf das vollständige Freigeben warten müssen, bevor
 * sie neu anfordern können.
 */

#include "policy.h"
#include "../core/state.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Privater Kontext der Hold-and-Wait-Policy.
 */
typedef struct {
    uint64_t deadlocks; /**< Anzahl der abgelehnten (verschobenen) Anfragen. */
    uint32_t n_procs;   /**< Anzahl der simulierten Prozesse. */
    uint32_t n_res;     /**< Anzahl der Ressourcenklassen. */
} HWCtx;

/**
 * @brief Prüft eine Ressourcenanfrage anhand der Hold-and-Wait-Bedingung.
 *
 * Die Anfrage wird abgelehnt, wenn:
 * - der anfragende Prozess bereits Ressourcen einer beliebigen Klasse hält, oder
 * - die angeforderte Menge die derzeit verfügbaren Instanzen übersteigt.
 *
 * Andernfalls wird die Anfrage genehmigt (@c true).
 *
 * @param p   Zeiger auf das Policy-Objekt (inkl. @c HWCtx).
 * @param st  Aktueller Systemzustand.
 * @param ev  Das zu prüfende Anforderungsereignis.
 * @return    @c true, wenn die Anfrage genehmigt werden kann; sonst @c false.
 */
static bool hw_on_request(Policy *p,
                          SystemState *st,
                          const Event *ev)
{
    HWCtx *ctx = (HWCtx *)p->private;
    uint32_t pid = ev->pid;
    uint32_t rc  = ev->class_id;
    uint32_t amt = ev->amount;

    /* Hold-and-Wait-Prüfung: Hält der Prozess bereits Ressourcen irgendeiner Klasse? */
    for (uint32_t r = 0; r < st->n_classes; ++r) {
        if (st->allocation[pid][r] > 0) {
            ctx->deadlocks++;  /* Hold-and-Wait würde entstehen → Anfrage verschieben */
            return false;
        }
    }

    /* Verfügbarkeitscheck: Sind genügend Instanzen der angeforderten Klasse frei? */
    if (amt > st->available[rc]) {
        ctx->deadlocks++;  /* Ressource aktuell nicht verfügbar */
        return false;
    }

    return true;  /* Anfrage genehmigen */
}

/**
 * @brief Periodischer Tick-Callback der Hold-and-Wait-Policy.
 *
 * Für diese Policy sind keine periodischen Aktionen erforderlich.
 * Alle Parameter werden bewusst ignoriert.
 *
 * @param p    Nicht verwendet.
 * @param st   Nicht verwendet.
 * @param now  Nicht verwendet.
 */
static void hw_on_tick(Policy *p,
                       SystemState *st,
                       uint64_t now)
{
    (void)p; (void)st; (void)now;
}

/**
 * @brief Gibt alle von der Hold-and-Wait-Policy belegten Ressourcen frei.
 *
 * @param p  Zeiger auf die freizugebende Policy.
 */
static void hw_cleanup(Policy *p)
{
    free(p->private);
    free(p);
}

/**
 * @brief Factory-Funktion: Erstellt eine neue Hold-and-Wait-Policy.
 *
 * Allokiert und initialisiert die @c Policy-Struktur sowie den
 * privaten @c HWCtx. Setzt den Namen auf "Hold-and-Wait Elim".
 *
 * @param n_procs  Anzahl der simulierten Prozesse.
 * @param n_res    Anzahl der Ressourcenklassen.
 * @return         Zeiger auf die neu erstellte @c Policy.
 */
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
