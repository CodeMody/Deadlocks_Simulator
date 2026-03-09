/**
 * @file gui.h
 * @brief Öffentliches Interface für das GTK-3-Frontend des Deadlock-Simulators.
 *
 * Dieses Modul implementiert die grafische Benutzeroberfläche mit folgenden
 * Elementen und Funktionen:
 * - Drei Policy-Schaltflächen zur Auswahl der Deadlock-Behandlungsstrategie.
 * - Eine Step-Schaltfläche für manuelle Einzelschritte der Simulation.
 * - Eine Statusleiste mit aktivem Policy-Namen, Verschiebungs-Zähler und Tick.
 * - Ein scrollbares Textfenster für den Echtzeit-Snapshot des Systemzustands.
 * - Eine Legende zur Erklärung der angezeigten Felder.
 * - Automatisches Update der Anzeige einmal pro Sekunde via GTK-Timer.
 */

#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include <stdint.h>
#include "../core/scheduler.h"
#include "../policy/policy.h"

/**
 * @brief Erstellt das Hauptfenster der Anwendung und zeigt es an.
 *
 * Initialisiert alle GTK-Widgets (Buttons, Labels, TextView, Legende),
 * lädt das CSS-Theme und startet den periodischen Timer für automatische
 * Simulationsschritte (1 Sekunde Intervall). Das Fenster wird sofort
 * angezeigt, die Simulation startet jedoch erst nach Auswahl einer Policy.
 *
 * @return Zeiger auf das erstellte Top-Level-GtkWidget (Hauptfenster).
 */
GtkWidget *gui_create_window(void);

/**
 * @brief Wechselt die aktive Deadlock-Policy und startet die Simulation neu.
 *
 * Die Funktion führt folgende Schritte durch:
 * 1. Stoppt den periodischen GTK-Timer.
 * 2. Gibt den alten Scheduler, den SystemState und die Policy frei.
 * 3. Erstellt einen neuen Demo-Systemzustand.
 * 4. Erstellt einen neuen Scheduler mit der übergebenen Policy.
 * 5. Initialisiert den Prozess-Simulator neu.
 * 6. Aktualisiert alle GUI-Elemente.
 * 7. Startet den periodischen Timer neu.
 *
 * @param new_policy  Zeiger auf die neu zu aktivierende Policy. Der Scheduler
 *                    übernimmt die Eigentümerschaft und gibt die Policy beim
 *                    nächsten Aufruf von @c gui_set_policy() oder beim
 *                    Beenden automatisch frei.
 */
void gui_set_policy(Policy *new_policy);

/**
 * @brief Führt einen einzelnen Simulationsschritt aus und aktualisiert die GUI.
 *
 * Erzeugt prozedurale Ereignisse für den nächsten Tick, ruft
 * @c scheduler_run_until() auf und aktualisiert den Snapshot-Text,
 * den Verschiebungs-Zähler und den Tick-Counter.
 *
 * Die Signatur ist kompatibel mit @c GSourceFunc, damit GTK diese
 * Funktion als periodischen Timer-Callback verwenden kann.
 *
 * @param user_data  Wird nicht verwendet (GTK-Konvention).
 * @return           @c G_SOURCE_CONTINUE, damit der Timer weiterläuft.
 */
gboolean gui_step(gpointer user_data);

/**
 * @brief Gibt den aktuellen Verschiebungs-/Deadlock-Zähler zurück.
 *
 * Liest das erste @c uint64_t-Feld aus dem privaten Kontext der aktiven Policy,
 * das bei allen drei Policy-Implementierungen als Zähler für verschobene
 * Anfragen dient.
 *
 * @return Anzahl der bisher verschobenen Anfragen, oder 0 wenn keine Policy aktiv ist.
 */
uint64_t gui_get_deadlock_counter(void);

#endif /* GUI_H */
