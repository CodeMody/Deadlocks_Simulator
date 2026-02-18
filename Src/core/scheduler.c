#define _XOPEN_SOURCE 600
#include "../core/scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* -----------------------------------------------------------------
   Interne repräsentation des Schedulers
   ----------------------------------------------------------------- */
struct Scheduler {
    SystemState *state;     /* Zeiger auf den Systemzustand (Ressourcen, Prozesse) */
    Policy      *policy;    /* Zeiger auf die Scheduling-Richtlinie (z.B. Banker's Algorithmus) */
    EventHeap   *heap;      /* Min-Heap für zeitliche sortierte Events */
    uint64_t     now;       /* Aktuelle Simulationszeit */
};

/* -----------------------------------------------------------------
   Erstellen / Zerstören
   ----------------------------------------------------------------- */
Scheduler *scheduler_create(SystemState *st, Policy *policy)
{
    Scheduler *s = calloc(1, sizeof(*s));
    if (!s) { fprintf(stderr, "scheduler_create: out of memory\n"); exit(EXIT_FAILURE); }
    s->state  = st;                     /* Systemzustand übernehmen */
    s->policy = policy;                 /* Policy-Strategie übernehmen */
    s->heap   = event_heap_create();    /* Simulationszeit startet bei 0 */
    s->now    = 0;
    return s;
}

void scheduler_destroy(Scheduler *s)
{
    if (!s) return;
    event_heap_destroy(s->heap);
    if (s->policy && s->policy->cleanup) s->policy->cleanup(s->policy);
    free(s);
}

/* -----------------------------------------------------------------
   Neues Event einfügen
   ----------------------------------------------------------------- */
void scheduler_schedule_event(Scheduler *sch, const Event *ev)
{
    event_push(sch->heap, ev);
}

/* -----------------------------------------------------------------
   Hilfsfunktion, die tatsächlich die Matrizen aktualisiert
   ----------------------------------------------------------------- */
static void handle_request(Scheduler *s, const Event *ev)
{
    bool grant = s->policy->on_request(s->policy, s->state, ev);

    /* -------------------------------------------------------------
       Selbst wenn die Policy "gewähren" sagt, müssen wir noch prüfen,
       ob die Ressourcen wirklich frei sind. Falls nicht, behandle es
       als aufgeschobene Anfrage
       ------------------------------------------------------------- */
    if (grant) {
        uint32_t pid   = ev->pid;                       /*Prozess-ID*/
        uint32_t rc    = ev->class_id;                  /*Ressourcenklasse*/
        uint32_t amt   = ev->amount;                    /*Angeforderte Menge*/
        uint32_t avail = s->state->available[rc];       /*Verfügbare Instanzen*/
        uint32_t need  = s->state->request[pid][rc];    /*Noch benötigte Menge*/

        /*Prüft, ob genug Ressourcen verfügbar sind UND
        * ob die Anfrage nicht den Bedarf übersteigt */
        if (amt > avail || amt > need) {
            grant = false;   /* Nicht genug freie Instanzen oder Bedarf */
        }
    }

    /*Wenn die Anfrage genehmight */
    if (grant) {
        uint32_t pid = ev->pid;
        uint32_t rc  = ev->class_id;
        uint32_t amt = ev->amount;

        /*Aktualisiert die Matrizen
        * Erhöht die Allokation für diesen Prozess
        * Verringert die ausstehende Anfrage
        * Verringert die verfügbaren Ressourcen */
        s->state->allocation[pid][rc] += amt;
        s->state->request[pid][rc]    -= amt;
        s->state->available[rc]       -= amt;
    } else {
        /* Anfrage wird aufgeschoben — aber nur bis zum Retry-Limit */
#define MAX_RETRIES 100
        if (ev->retries >= MAX_RETRIES) {
            fprintf(stderr,
                    "scheduler: request from pid=%u class=%u amt=%u "
                    "dropped after %u retries (unsatisfiable)\n",
                    ev->pid, ev->class_id, ev->amount, MAX_RETRIES);
            return;  /* Event verwerfen statt endlos wiederholen */
        }
        Event retry  = *ev;           /* Kopiert das Event */
        retry.time   = s->now + 1;    /* Setzt Zeit auf "Jetzt +1" */
        retry.retries++;              /* Erhöht den Retry-Zähler */
        event_push(s->heap, &retry);  /* Fügt es wieder in den Heap ein */
    }
}

/* -----------------------------------------------------------------
   Führt die Simulation bis "until_time" aus
   ----------------------------------------------------------------- */
/*Hauptschleife: Verarbeitet alle Events bis zu einer bestimmten Zeit*/
void scheduler_run_until(Scheduler *s, uint64_t until_time)
{
    Event ev;

    /*Solange es Events gibt, die in unserem Zeitfenster liegen */
    while (event_peek(s->heap, &ev) && ev.time <= until_time) {
        /*Entferne das nchste Event aus dem Heap*/
        event_pop(s->heap, &ev);
        /*Aktualisiert die aktuelle Simulationszeit*/
        s->now = ev.time;

        /*Verarbeitet das Event basierend auf seinem Typ*/
        switch (ev.type) {
            case EV_REQUEST:
            /*Prozess fordert Ressourcen an*/
                handle_request(s, &ev);
                break;

            case EV_RELEASE:
            /*Prozess gibt Ressourcen frei */
            {
                uint32_t held = s->state->allocation[ev.pid][ev.class_id];
                if (ev.amount > held) {
                    fprintf(stderr,
                            "scheduler: EV_RELEASE over-release pid=%u class=%u "
                            "held=%u requested=%u — clamping to held\n",
                            ev.pid, ev.class_id, held, ev.amount);
                    ev.amount = held;   /* Clamp: kann nicht mehr freigeben als gehalten */
                }
                s->state->allocation[ev.pid][ev.class_id] -= ev.amount;
                s->state->available[ev.class_id]          += ev.amount;
                break;
            }

            case EV_TERMINATE:
                /*Prozess terminiert - gebe alle gehaltenen Ressourcen frei*/
                for (uint32_t r = 0; r < s->state->n_classes; ++r) {
                    uint32_t held = s->state->allocation[ev.pid][r];
                    if (held) {
                    /*Setze Allokation auf 0 und gebe ressourcen zurück */
                        s->state->allocation[ev.pid][r] = 0;
                        s->state->available[r]         += held;
                    }
                    /* Auch den Request-Vektor löschen, damit der
                       Banker-Algorithmus keine veralteten Bedarfswerte sieht */
                    s->state->request[ev.pid][r] = 0;
                }
                break;

            case EV_CHECKPOINT:
                /* Wird in dieser minimalen Demo nicht verwendet*/
                break;

            default:
            /*Unbekannter Event-Typ gibt eine Warnung aus*/
                fprintf(stderr,
                        "scheduler: unknown event type %d (pid=%u, time=%lu)\n",
                        ev.type, ev.pid, ev.time);
                break;
        }
        /*Falls die Policy eine Tick-Funktion hat, rufe sie nach jedem Event auf*/
        if (s->policy->on_tick)
            s->policy->on_tick(s->policy, s->state, s->now);
    }
}

/* -----------------------------------------------------------------
   Zugriffsfunktionen hilfreich für die Ausgabe der GUI
   ----------------------------------------------------------------- */
/*Gibt die aktuelle Simulationszeit zurück*/
uint64_t scheduler_current_time(const Scheduler *s) { return s->now; }
/*Gibt einen Zeiger auf den Systemzustand zurück*/
SystemState *scheduler_state(const Scheduler *s)   { return s->state; }
/*Gibt einen Zeiger auf die Policy zurück*/
Policy *scheduler_policy(const Scheduler *s)       { return s->policy; }