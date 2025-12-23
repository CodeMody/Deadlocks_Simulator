#include <gtk/gtk.h>
#include "gui/gui.h"

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    /* The window will create a fresh scheduler when you press one
       of the “Run …” buttons.  Until then the UI is empty. */
    gui_create_window();

    gtk_main();          /* enters the GTK event loop */
    return 0;
}