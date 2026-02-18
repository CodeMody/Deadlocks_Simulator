/*=====================================================================
 *  gui.h
 *  ---------------------------------------------------------------
 *  Öffentliches Interface für das GTK-3Fronted des deadlock Simulator
 *
 *  Die Oberfläche ermöglicht
*     Auswahl einer der drei Policies (Graph-Detect, Banker,
 *    Hold-and-Wait)
 *    Manuelles Weiterlaufen der Simulation (Step) oder
 *    automatischen Ablauf
 *    Anzeige eines textuellen Snapshots des Systemzustands
 *    Anzeige eines Zählers, der angibt, wie viele Requests
 *    verschoben wurden (d. h. wie viele potenzielle Deadlocks erkannt wurden)
 *====================================================================*/

#ifndef GUI_H
#define GUI_H

/* --------------------------------------------------------------
     GTK-3-Header – definiert GtkWidget, GtkButton, GtkLabel, … */
#include <gtk/gtk.h>

/* --------------------------------------------------------------
   Standard-Ganttahltypen, uint64_t wird für den Zähler benötigt   */
#include <stdint.h>

#include "../core/scheduler.h"
#include "../policy/policy.h"

/* -----------------------------------------------------------------
   Erstellt das Hauptfenster der Anwendung.
   Gibt das Top-Level-GtkWidget zurück (der Rückgabewert kann
   ignoriert werden, wenn lediglich das Fenster angezeigt
   werden soll).
   ----------------------------------------------------------------- */
GtkWidget *gui_create_window(void);

/* -----------------------------------------------------------------
   Ersetzt die aktuell aktive Policy durch eine neu erzeugte.
   Die Funktion zerstört den alten Scheduler (inklusive seiner
   Policy) und erstellt einen neuen Scheduler, der die übergebene
   Policy verwendet.
   ----------------------------------------------------------------- */
void gui_set_policy(Policy *new_policy);

/* -----------------------------------------------------------------
   Führt die Simulation um einen logischen Zeitschritt weiter
   und aktualisiert die grafische Oberfläche.
   Gibt G_SOURCE_CONTINUE zurück, damit GTK den Timer weiter laufen lässt.
   ----------------------------------------------------------------- */
gboolean gui_step(gpointer user_data);

/* -----------------------------------------------------------------
   Gibt den aktuellen Deadlock- bzw. Verschiebungszähler zurück
   (aus dem privaten Kontext der Policy gelesen).
   Falls keine Policy aktiv ist, wird 0 zurückgegeben.
   ----------------------------------------------------------------- */
uint64_t gui_get_deadlock_counter(void);

#endif /* GUI_H */