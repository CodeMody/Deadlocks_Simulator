/*=====================================================================
 *  gui.h
 *  ---------------------------------------------------------------
 *  Public interface for the GTK‑3 front‑end of the dead‑lock simulator.
 *
 *  The GUI lets you:
 *      • choose one of the three policies (Graph‑Detect, Banker,
 *        Hold‑and‑Wait)
 *      • step the simulation manually or let it run automatically
 *      • see a textual snapshot of the system state
 *      • see a counter that shows how many requests have been
 *        postponed (i.e. how many potential dead‑locks have been
 *        detected)
 *
 *  Compile‑time requirement: the GTK‑3 development package must be
 *  installed (e.g. `sudo apt‑get install libgtk-3-dev` on Ubuntu,
 *  `brew install gtk+3` on macOS, or the MSYS2 `mingw-w64-x86_64-gtk3`
 *  package on Windows).
 *====================================================================*/

#ifndef GUI_H
#define GUI_H

/* --------------------------------------------------------------
   GTK‑3 header – defines GtkWidget, GtkButton, GtkLabel, …   */
#include <gtk/gtk.h>

/* --------------------------------------------------------------
   Standard integer types – we need uint64_t for the counter.   */
#include <stdint.h>

/* --------------------------------------------------------------
   Project‑specific headers.                                    */
#include "../core/scheduler.h"
#include "../policy/policy.h"

/* -----------------------------------------------------------------
   Create the main application window.  Returns the top‑level
   GtkWidget (you can ignore the return value if you just want the
   window to appear).
   ----------------------------------------------------------------- */
GtkWidget *gui_create_window(void);

/* -----------------------------------------------------------------
   Replace the current policy with a newly created one.  The function
   destroys the old scheduler (and its policy) and builds a fresh
   scheduler that uses the supplied policy.
   ----------------------------------------------------------------- */
void gui_set_policy(Policy *new_policy);

/* -----------------------------------------------------------------
   Advance the simulation by one logical tick and refresh the UI.
   ----------------------------------------------------------------- */
void gui_step(void);

/* -----------------------------------------------------------------
   Return the current dead‑lock / postponement counter (read from the
   policy’s private context).  If no policy is active, returns 0.
   ----------------------------------------------------------------- */
uint64_t gui_get_deadlock_counter(void);

#endif /* GUI_H */