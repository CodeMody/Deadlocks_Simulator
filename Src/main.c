#include <gtk/gtk.h>
#include "gui/gui.h"

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    /* Das Fenster erstellt einen neuen Scheduler, wenn Sie eine der
Schaltflächen „Ausführen…“ drücken. Bis dahin ist die Benutzeroberfläche leer. */
    gui_create_window();

    gtk_main();          /* tritt in die GTK-Ereignisschleife ein */
    return 0;
}
