/**
 * @file types.h
 * @brief Gemeinsame Typdefinitionen für den Deadlock-Simulator.
 *
 * Diese Datei definiert alle grundlegenden Datenstrukturen und Typen,
 * die von den Modulen @c core und @c policy gemeinsam genutzt werden.
 * Dazu gehören Ereignistypen, die Ereignisstruktur, der Systemzustand
 * sowie Funktionszeiger für das Policy-Interface.
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Aufzählung aller möglichen Ereignistypen in der Simulation.
 *
 * Jedes Ereignis in der Event-Queue besitzt einen dieser Typen,
 * der bestimmt, wie der Scheduler das Ereignis verarbeitet.
 */
typedef enum {
    EV_REQUEST,    /**< Ein Prozess fordert Ressourcen einer Klasse an. */
    EV_RELEASE,    /**< Ein Prozess gibt zuvor gehaltene Ressourcen frei. */
    EV_CHECKPOINT, /**< Prüfereignis; kann für Zustandstests genutzt werden. */
    EV_TERMINATE   /**< Ein Prozess beendet sich und gibt alle Ressourcen frei. */
} EventType;

/**
 * @brief Einzelnes Ereignis in der zeitgesteuerten Event-Queue.
 *
 * Ereignisse werden in einem Min-Heap nach @c time sortiert.
 * Bei abgelehnten Anfragen wird das Ereignis mit erhöhtem @c retries-Zähler
 * erneut in die Queue eingetragen.
 */
typedef struct {
    uint64_t  time;      /**< Logischer Zeitstempel des Ereignisses (in Ticks). */
    EventType type;      /**< Art des Ereignisses (Anforderung, Freigabe, …). */
    uint32_t  pid;       /**< ID des betroffenen Prozesses. */
    uint32_t  class_id;  /**< Betroffene Ressourcenklasse (Index in den Matrizen). */
    uint32_t  amount;    /**< Anzahl der angeforderten oder freigegebenen Instanzen. */
    uint32_t  retries;   /**< Anzahl bisheriger Wiederholungsversuche (für Retry-Limit). */
} Event;

struct Policy;

/**
 * @brief Vorwärts-Deklaration des Systemzustands.
 *
 * Notwendig, damit @c SystemState in den Funktionszeigern des
 * Policy-Interfaces sichtbar ist, bevor die vollständige Definition folgt.
 */
typedef struct SystemState SystemState;

/**
 * @brief Funktionszeiger-Typ für den Request-Callback einer Policy.
 *
 * Wird vom Scheduler aufgerufen, wenn ein @c EV_REQUEST-Ereignis verarbeitet
 * wird. Die Funktion entscheidet, ob die Anfrage genehmigt wird.
 *
 * @param p   Zeiger auf das Policy-Objekt (inkl. privatem Kontext).
 * @param st  Aktueller Systemzustand.
 * @param ev  Das zu bearbeitende Anforderungsereignis.
 * @return    @c true, wenn die Anfrage genehmigt werden soll; sonst @c false.
 */
typedef bool (*policy_request_f)(struct Policy *p,
                                 struct SystemState *st,
                                 const Event *ev);

/**
 * @brief Funktionszeiger-Typ für den periodischen Tick-Callback einer Policy.
 *
 * Wird nach jedem verarbeiteten Ereignis aufgerufen und ermöglicht der
 * Policy periodische Wartungsaufgaben (z. B. Deadlock-Erkennung).
 *
 * @param p    Zeiger auf das Policy-Objekt.
 * @param st   Aktueller Systemzustand.
 * @param now  Aktuelle Simulationszeit.
 */
typedef void (*policy_tick_f)(struct Policy *p,
                              struct SystemState *st,
                              uint64_t now);

/**
 * @brief Funktionszeiger-Typ für die Aufräumfunktion einer Policy.
 *
 * Gibt den gesamten dynamisch allokierten Speicher der Policy frei,
 * einschließlich des privaten Kontexts und der Policy-Struktur selbst.
 *
 * @param p  Zeiger auf das freizugebende Policy-Objekt.
 */
typedef void (*policy_cleanup_f)(struct Policy *p);

/**
 * @brief Zentraler Systemzustand des Ressourcen-Managers.
 *
 * Enthält alle Informationen über Ressourcenklassen und Prozesse:
 * Gesamtanzahl, verfügbare Instanzen sowie die Allokations- und
 * Anforderungsmatrizen. Der Banker-Algorithmus operiert auf dieser Struktur.
 *
 * @note Die Matrizen @c allocation und @c request werden erst nach dem
 *       Aufruf von @c state_allocate_process_matrices() gültig.
 */
struct SystemState {
    uint32_t  n_classes;    /**< Anzahl verschiedener Ressourcenklassen. */
    uint32_t  n_procs;      /**< Anzahl der simulierten Prozesse. */
    uint32_t *instances;    /**< Gesamtanzahl der Instanzen je Klasse (Vektor E). */
    uint32_t *available;    /**< Derzeit verfügbare Instanzen je Klasse (Vektor A). */
    uint32_t **allocation;  /**< Allokationsmatrix C: allocation[pid][class] = gehaltene Instanzen. */
    uint32_t **request;     /**< Bedarfsmatrix R: request[pid][class] = noch benötigte Instanzen. */
};

#endif /* TYPES_H */
