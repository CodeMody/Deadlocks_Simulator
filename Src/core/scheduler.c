/**
 * @file scheduler.c
 * @brief Implementierung des ereignisgesteuerten Simulator-Kerns.
 *
 * Der Scheduler ist die zentrale Steuereinheit der Simulation. Er verwaltet
 * eine zeitgeordnete Event-Queue (Min-Heap) und verarbeitet Ereignisse
 * chronologisch. Ressourcenanforderungen werden an die aktive Policy
 * delegiert; abgelehnte Anfragen werden automatisch für den nächsten Tick
 * erneut eingeplant, bis das Retry-Limit (@c MAX_RETRIES) erreicht ist.
 *
 * Die interne Struktur (@c struct Scheduler) ist nach dem Opaque-Pointer-
 * Muster versteckt, sodass externe Module nur über die öffentliche API
 * aus @c scheduler.h mit dem Scheduler interagieren können.
 */

#define _XOPEN_SOURCE 600
#include "../core/scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/** @brief Maximale Anzahl von Wiederholungsversuchen für eine Ressourcenanfrage. */
#define MAX_RETRIES 100

/**
 * @brief Interne Repräsentation des Schedulers.
 *
 * Kapselt den Systemzustand, die aktive Policy, den Ereignis-Heap
 * und die aktuelle Simulationszeit.
 */
struct Scheduler {
    SystemState *state;   /**< Zeiger auf den verwalteten Systemzustand (Ressourcen, Matrizen). */
    Policy      *policy;  /**< Zeiger auf die aktive Deadlock-Behandlungsstrategie. */
    EventHeap   *heap;    /**< Min-Heap für zeitlich sortierte Ereignisse. */
    uint64_t     now;     /**< Aktuelle Simulationszeit in logischen Ticks. */
};

/**
 * @brief Erstellt einen neuen Scheduler.
 *
 * Initialisiert den Scheduler mit dem übergebenen Systemzustand und der
 * Policy. Der Ereignis-Heap wird leer erstellt. Die Simulationszeit beginnt bei 0.
 *
 * @param st      Zeiger auf den Systemzustand (Eigentümerschaft verbleibt beim Aufrufer).
 * @param policy  Zeiger auf die aktive Policy (Eigentümerschaft verbleibt beim Aufrufer).
 * @return        Zeiger auf den neu erstellten @c Scheduler.
 */
Scheduler *scheduler_create(SystemState *st, Policy *policy)
{
    Scheduler *s = calloc(1, sizeof(*s));
    if (!s) { fprintf(stderr, "scheduler_create: out of memory\n"); exit(EXIT_FAILURE); }
    s->state  = st;
    s->policy = policy;
    s->heap   = event_heap_create();
    s->now    = 0;
    return s;
}

/**
 * @brief Zerstört einen Scheduler und gibt seinen Speicher frei.
 *
 * Gibt den internen Ereignis-Heap frei und ruft die @c cleanup-Funktion
 * der aktiven Policy auf. Der @c SystemState wird @b nicht freigegeben.
 *
 * @param s  Zeiger auf den zu zerstörenden Scheduler. NULL wird ignoriert.
 */
void scheduler_destroy(Scheduler *s)
{
    if (!s) return;
    event_heap_destroy(s->heap);
    if (s->policy && s->policy->cleanup)
        s->policy->cleanup(s->policy);
    free(s);
}

/**
 * @brief Fügt ein Ereignis in die Event-Queue ein.
 *
 * @param sch  Zeiger auf den Scheduler.
 * @param ev   Zeiger auf das einzuplanende Ereignis (wird per Wert kopiert).
 */
void scheduler_schedule_event(Scheduler *sch, const Event *ev)
{
    event_push(sch->heap, ev);
}

/**
 * @brief Verarbeitet ein einzelnes @c EV_REQUEST-Ereignis.
 *
 * Die Policy-Funktion @c on_request wird aufgerufen. Genehmigt sie die
 * Anfrage, wird zusätzlich geprüft, ob tatsächlich genügend Ressourcen
 * verfügbar sind und der angeforderte Betrag den verbleibenden Bedarf
 * nicht überschreitet. Erst dann werden die Matrizen aktualisiert.
 *
 * Wird die Anfrage abgelehnt, wird sie für den nächsten Tick mit
 * erhöhtem @c retries-Zähler erneut eingeplant. Nach @c MAX_RETRIES
 * Versuchen wird das Ereignis verworfen.
 *
 * @param s   Zeiger auf den Scheduler.
 * @param ev  Zeiger auf das zu verarbeitende Anforderungsereignis.
 */
static void handle_request(Scheduler *s, const Event *ev)
{
    /* Policy entscheidet über die grundsätzliche Genehmigung */
    bool grant = s->policy->on_request(s->policy, s->state, ev);

    if (grant) {
        uint32_t pid   = ev->pid;
        uint32_t rc    = ev->class_id;
        uint32_t amt   = ev->amount;
        uint32_t avail = s->state->available[rc];
        uint32_t need  = s->state->request[pid][rc];

        /* Zusätzliche Plausibilitätsprüfung: Ressourcen verfügbar und Bedarf nicht überschritten? */
        if (amt > avail || amt > need) {
            grant = false;
        }
    }

    if (grant) {
        uint32_t pid = ev->pid;
        uint32_t rc  = ev->class_id;
        uint32_t amt = ev->amount;

        /* Matrizen und Verfügbarkeitsvektor aktualisieren */
        s->state->allocation[pid][rc] += amt;  /* Allokation erhöhen */
        s->state->request[pid][rc]    -= amt;  /* Bedarf verringern */
        s->state->available[rc]       -= amt;  /* Verfügbare Ressourcen verringern */
    } else {
        /* Anfrage abgelehnt: erneut einplanen, sofern Retry-Limit nicht erreicht */
        if (ev->retries >= MAX_RETRIES) {
            fprintf(stderr,
                    "scheduler: request from pid=%u class=%u amt=%u "
                    "dropped after %u retries (unsatisfiable)\n",
                    ev->pid, ev->class_id, ev->amount, MAX_RETRIES);
            return;
        }
        Event retry  = *ev;
        retry.time   = s->now + 1;  /* Für den nächsten Tick einplanen */
        retry.retries++;
        event_push(s->heap, &retry);
    }
}

/**
 * @brief Verarbeitet alle Ereignisse bis einschließlich des Zeitstempels @p until_time.
 *
 * Die Hauptschleife der Simulation: Solange der Heap nicht leer ist und das
 * nächste Ereignis im Zeitfenster liegt, wird es entnommen und typ-abhängig
 * verarbeitet. Nach jedem Ereignis wird der @c on_tick-Callback der Policy
 * aufgerufen, sofern vorhanden.
 *
 * @param s           Zeiger auf den Scheduler.
 * @param until_time  Obergrenze des zu verarbeitenden Zeitfensters (inklusiv).
 */
void scheduler_run_until(Scheduler *s, uint64_t until_time)
{
    Event ev;

    while (event_peek(s->heap, &ev) && ev.time <= until_time) {
        event_pop(s->heap, &ev);
        s->now = ev.time;

        switch (ev.type) {

            case EV_REQUEST:
                /* Ressourcenanforderung: Policy und Plausibilitätsprüfung */
                handle_request(s, &ev);
                break;

            case EV_RELEASE:
                /* Ressourcenfreigabe: gib angeforderte Menge zurück */
            {
                uint32_t held = s->state->allocation[ev.pid][ev.class_id];
                if (ev.amount > held) {
                    /* Schutz vor Über-Freigabe: Betrag auf das tatsächlich Gehaltene begrenzen */
                    fprintf(stderr,
                            "scheduler: EV_RELEASE over-release pid=%u class=%u "
                            "held=%u requested=%u — clamping to held\n",
                            ev.pid, ev.class_id, held, ev.amount);
                    ev.amount = held;
                }
                s->state->allocation[ev.pid][ev.class_id] -= ev.amount;
                s->state->available[ev.class_id]          += ev.amount;
                break;
            }

            case EV_TERMINATE:
                /* Prozessende: alle gehaltenen Ressourcen freigeben */
                for (uint32_t r = 0; r < s->state->n_classes; ++r) {
                    uint32_t held = s->state->allocation[ev.pid][r];
                    if (held) {
                        s->state->allocation[ev.pid][r] = 0;
                        s->state->available[r]         += held;
                    }
                    /* Bedarfsvektor zurücksetzen: verhindert veraltete Werte im Banker-Check */
                    s->state->request[ev.pid][r] = 0;
                }
                break;

            case EV_CHECKPOINT:
                /* Platzhalter: in dieser Implementierung ohne Aktion */
                break;

            default:
                fprintf(stderr,
                        "scheduler: unknown event type %d (pid=%u, time=%lu)\n",
                        ev.type, ev.pid, ev.time);
                break;
        }

        /* Periodischen Tick-Callback der Policy aufrufen */
        if (s->policy->on_tick)
            s->policy->on_tick(s->policy, s->state, s->now);
    }
}

/**
 * @brief Gibt die aktuelle logische Simulationszeit zurück.
 *
 * @param s  Zeiger auf den Scheduler (const).
 * @return   Aktueller Zeitstempel in Ticks.
 */
uint64_t scheduler_current_time(const Scheduler *s) { return s->now; }

/**
 * @brief Gibt einen Zeiger auf den verwalteten Systemzustand zurück.
 *
 * @param s  Zeiger auf den Scheduler (const).
 * @return   Zeiger auf den @c SystemState.
 */
SystemState *scheduler_state(const Scheduler *s)   { return s->state; }

/**
 * @brief Gibt einen Zeiger auf die aktive Policy zurück.
 *
 * @param s  Zeiger auf den Scheduler (const).
 * @return   Zeiger auf die aktive @c Policy.
 */
Policy *scheduler_policy(const Scheduler *s)       { return s->policy; }
