#include <gtk/gtk.h>
#include "gui/gui.h"

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    /* Das Fenster erstellt einen neuen Scheduler, sobald eine der
       „Run …“-Schaltflächen gedrückt wird.
       Bis dahin bleibt die Benutzeroberfläche leer. */
    gui_create_window();

    gtk_main();          /* startet die GTK-Ereignisschleife */
    return 0;
}
