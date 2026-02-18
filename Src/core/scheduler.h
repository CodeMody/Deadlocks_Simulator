/**
 * @file scheduler.h
 * @brief Öffentliches Interface für den ereignisgesteuerten Simulator-Kern.
 *
 * Der @c Scheduler ist das zentrale Steuerungselement der Simulation.
 * Er verwaltet eine zeitgeordnete Event-Queue (Min-Heap), delegiert
 * Ressourcenanforderungen an die aktive @c Policy und aktualisiert
 * den @c SystemState entsprechend dem Ergebnis.
 *
 * Die interne Struktur ist nach dem Opaque-Pointer-Muster versteckt
 * (Information Hiding): Externe Module können nur über die hier
 * deklarierten Funktionen mit dem Scheduler interagieren.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../core/types.h"
#include "event.h"
#include "../policy/policy.h"

/**
 * @brief Opaker Zeiger auf die interne Scheduler-Struktur.
 *
 * Die vollständige Definition von @c Scheduler ist nur in @c scheduler.c
 * sichtbar. Alle externen Module arbeiten ausschließlich mit diesem Zeiger.
 */
typedef struct Scheduler Scheduler;

/**
 * @brief Erstellt einen neuen Scheduler.
 *
 * Initialisiert den Scheduler mit dem angegebenen Systemzustand und der
 * gewählten Policy. Die Simulationszeit startet bei 0. Der Scheduler
 * übernimmt @b keine Eigentümerschaft über @p st oder @p policy;
 * deren Lebensdauer muss vom Aufrufer verwaltet werden.
 *
 * @param st      Zeiger auf den zu verwaltenden Systemzustand.
 * @param policy  Zeiger auf die zu verwendende Deadlock-Policy.
 * @return        Zeiger auf den neu erstellten @c Scheduler.
 */
Scheduler *scheduler_create(SystemState *st, Policy *policy);

/**
 * @brief Zerstört einen Scheduler und gibt seinen Speicher frei.
 *
 * Gibt den internen Ereignis-Heap frei und ruft die @c cleanup-Funktion
 * der aktiven Policy auf. Der @c SystemState wird @b nicht freigegeben
 * und muss vom Aufrufer separat zerstört werden.
 *
 * @param sch  Zeiger auf den zu zerstörenden Scheduler. NULL wird ignoriert.
 */
void scheduler_destroy(Scheduler *sch);

/**
 * @brief Plant ein Ereignis in der Event-Queue ein.
 *
 * Das Ereignis wird nach seinem Zeitstempel in den Min-Heap eingefügt
 * und beim nächsten Aufruf von @c scheduler_run_until() verarbeitet,
 * sofern es im angegebenen Zeitfenster liegt.
 *
 * @param sch  Zeiger auf den Scheduler.
 * @param ev   Zeiger auf das einzuplanende Ereignis (wird per Wert kopiert).
 */
void scheduler_schedule_event(Scheduler *sch, const Event *ev);

/**
 * @brief Verarbeitet alle Ereignisse bis einschließlich @p until_time.
 *
 * In einer Schleife werden Ereignisse aus dem Heap entnommen und
 * typ-abhängig verarbeitet:
 * - @c EV_REQUEST: Delegiert an die Policy; bei Ablehnung wird das Ereignis
 *   mit erhöhtem @c retries-Zähler erneut eingeplant (max. @c MAX_RETRIES mal).
 * - @c EV_RELEASE: Gibt die angegebene Menge einer Ressourcenklasse frei.
 * - @c EV_TERMINATE: Gibt alle vom Prozess gehaltenen Ressourcen frei.
 * - @c EV_CHECKPOINT: Platzhalter, aktuell ohne Aktion.
 *
 * Nach jedem verarbeiteten Ereignis wird der @c on_tick-Callback der Policy
 * aufgerufen, falls vorhanden.
 *
 * @param sch         Zeiger auf den Scheduler.
 * @param until_time  Logischer Zeitstempel, bis zu dem Events verarbeitet werden.
 */
void scheduler_run_until(Scheduler *sch, uint64_t until_time);

/**
 * @brief Gibt die aktuelle logische Simulationszeit zurück.
 *
 * @param sch  Zeiger auf den Scheduler (const).
 * @return     Aktueller Zeitstempel in Ticks.
 */
uint64_t scheduler_current_time(const Scheduler *sch);

/**
 * @brief Gibt einen Zeiger auf den verwalteten Systemzustand zurück.
 *
 * @param sch  Zeiger auf den Scheduler (const).
 * @return     Zeiger auf den @c SystemState.
 */
SystemState *scheduler_state(const Scheduler *sch);

/**
 * @brief Gibt einen Zeiger auf die aktive Policy zurück.
 *
 * @param sch  Zeiger auf den Scheduler (const).
 * @return     Zeiger auf die aktive @c Policy.
 */
Policy *scheduler_policy(const Scheduler *sch);

#endif /* SCHEDULER_H */
