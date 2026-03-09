/**
 * @file main.c
 * @brief Einstiegspunkt des Deadlock-Simulators.
 *
 * Initialisiert das GTK-3-Framework, erstellt das Hauptfenster
 * der grafischen Benutzeroberfläche und startet die GTK-Ereignisschleife.
 * Die Simulation selbst beginnt erst, wenn der Benutzer über die GUI
 * eine der drei Deadlock-Policies auswählt.
 */

#include <gtk/gtk.h>
#include "gui/gui.h"

/**
 * @brief Hauptfunktion des Programms.
 *
 * Initialisiert GTK mit den übergebenen Kommandozeilenargumenten,
 * erstellt das Hauptfenster und startet die GTK-Ereignisschleife.
 * Das Programm kehrt aus @c gtk_main() zurück, sobald das Fenster
 * geschlossen wird.
 *
 * @param argc  Anzahl der Kommandozeilenargumente.
 * @param argv  Array der Kommandozeilenargumente.
 * @return      0 bei normalem Programmende.
 */
int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    /*
     * Erstellt das Hauptfenster mit allen Steuerelementen.
     * Die Simulation startet erst, wenn der Benutzer eine Policy-Schaltfläche
     * betätigt – bis dahin bleibt der Anzeigebereich leer.
     */
    gui_create_window();

    gtk_main();  /* Startet die GTK-Ereignisschleife; kehrt bei Fenster-Schließen zurück. */
    return 0;
}
