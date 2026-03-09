/**
 * @file policy.h
 * @brief Öffentliches Interface für alle Deadlock-Behandlungsstrategien.
 *
 * Eine @c Policy kapselt eine konkrete Strategie zur Vermeidung oder
 * Erkennung von Deadlocks. Das Interface basiert auf Funktionszeigern
 * und ermöglicht so einen austauschbaren Strategy-Pattern-Ansatz:
 * Der Scheduler ruft stets dieselben Callbacks auf, unabhängig davon,
 * welche konkrete Policy gerade aktiv ist.
 *
 * Aktuell werden drei Strategien unterstützt:
 * - @b Graph-Erkennung: Baut den Wait-For-Graphen auf und prüft auf Zyklen.
 * - @b Bankier-Algorithmus: Genehmigt Anfragen nur bei nachgewiesener Sicherheit.
 * - @b Hold-and-Wait-Eliminierung: Lehnt Anfragen von Prozessen ab, die bereits Ressourcen halten.
 */

#ifndef POLICY_H
#define POLICY_H

#include "../core/types.h"

/**
 * @brief Policy-Objekt, das eine Deadlock-Behandlungsstrategie darstellt.
 *
 * Jede konkrete Strategie befüllt diese Struktur mit ihrem Namen,
 * ihren Callback-Funktionen und einem optionalen privaten Kontext.
 * Der Scheduler hält einen Zeiger auf dieses Objekt und ruft die
 * Callbacks entsprechend auf.
 */
typedef struct Policy {
    const char       *name;       /**< Menschenlesbarer Name der Policy (z. B. "Banker (multi)"). */
    policy_request_f  on_request; /**< Callback: wird bei jedem @c EV_REQUEST-Ereignis aufgerufen. */
    policy_tick_f     on_tick;    /**< Callback: optionale Wartungsfunktion nach jedem verarbeiteten Ereignis. */
    policy_cleanup_f  cleanup;    /**< Callback: gibt den gesamten Speicher der Policy frei. */
    void             *private;    /**< Policy-spezifischer Kontext (opak für den Scheduler). */
} Policy;

/**
 * @brief Erstellt eine Policy basierend auf Wait-For-Graph-Zykluserkennung.
 *
 * Bei jeder Ressourcenanfrage wird der Wait-For-Graph um die neue Zuweisung
 * erweitert und mittels Tiefensuche auf Zyklen geprüft. Eine Anfrage wird
 * nur genehmigt, wenn kein Zyklus entsteht.
 *
 * @param n_procs    Anzahl der simulierten Prozesse.
 * @param n_classes  Anzahl der Ressourcenklassen.
 * @return           Zeiger auf die neue @c Policy.
 */
Policy *detect_policy_create(uint32_t n_procs,
                             uint32_t n_classes);

/**
 * @brief Erstellt eine Policy basierend auf dem Bankier-Algorithmus.
 *
 * Vor jeder Ressourcenvergabe wird die Zuweisung temporär simuliert
 * und der Sicherheitszustand des Systems geprüft. Nur bei einem sicheren
 * Zustand wird die Anfrage genehmigt.
 *
 * @param n_procs  Anzahl der simulierten Prozesse.
 * @param n_res    Anzahl der Ressourcenklassen.
 * @return         Zeiger auf die neue @c Policy.
 */
Policy *banker_policy_create(uint32_t n_procs,
                             uint32_t n_res);

/**
 * @brief Erstellt eine Policy zur Eliminierung der Hold-and-Wait-Bedingung.
 *
 * Verhindert Deadlocks durch das Unterbinden der Hold-and-Wait-Bedingung:
 * Hält ein Prozess bereits Ressourcen irgendeiner Klasse, wird jede neue
 * Anfrage desselben Prozesses abgelehnt.
 *
 * @param n_procs  Anzahl der simulierten Prozesse.
 * @param n_res    Anzahl der Ressourcenklassen.
 * @return         Zeiger auf die neue @c Policy.
 */
Policy *holdwait_policy_create(uint32_t n_procs,
                               uint32_t n_res);

#endif /* POLICY_H */
