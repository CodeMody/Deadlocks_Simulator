/*=====================================================================
 *  gui.c
 *  ---------------------------------------------------------------
 *  GTK‑3 front‑end for the dead‑lock simulator.
 *  gui.c does the following
 *   • stops the periodic timeout before destroying the old scheduler,
 *   • destroys the scheduler, its policy and the SystemState cleanly,
 *   • creates a fresh scheduler with the newly selected policy,
 *   • resets the dead‑lock counter label,
 *   • restarts the timeout.
 *====================================================================*/

#include "../gui/gui.h"
#include "../core/state.h"
#include "../core/event.h"
#include "../core/types.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------
   Global objects – there is only one scheduler/policy at a time.
   ----------------------------------------------------------------- */
static Scheduler   *g_sched          = NULL;   /* current scheduler   */
static Policy      *g_policy         = NULL;   /* current policy      */
static GtkTextView *g_text_view      = NULL;   /* snapshot view       */
static GtkLabel    *g_deadlock_label = NULL;   /* dead‑lock counter   */

/* -----------------------------------------------------------------
   Timeout source ID – we keep it so we can stop the timeout while
   we rebuild the simulation.
   ----------------------------------------------------------------- */
static guint g_timeout_id = 0;

/* -----------------------------------------------------------------
   Helper: allocate the allocation / request matrices and store the
   number of processes in the SystemState (required by the Banker
   safety check).
   ----------------------------------------------------------------- */
static void allocate_state_matrices(SystemState *st, uint32_t n_procs)
{
    uint32_t n_classes = st->n_classes;

    st->allocation = calloc(n_procs, sizeof(uint32_t *));
    for (uint32_t i = 0; i < n_procs; ++i)
        st->allocation[i] = calloc(n_classes, sizeof(uint32_t));

    st->request = calloc(n_procs, sizeof(uint32_t *));
    for (uint32_t i = 0; i < n_procs; ++i)
        st->request[i] = calloc(n_classes, sizeof(uint32_t));

    st->n_procs = n_procs;          /* store it for the Banker check */
}

/* -----------------------------------------------------------------
   Helper: free the matrices.
   ----------------------------------------------------------------- */
static void free_state_matrices(SystemState *st, uint32_t n_procs)
{
    if (st->allocation) {
        for (uint32_t i = 0; i < n_procs; ++i)
            free(st->allocation[i]);
        free(st->allocation);
        st->allocation = NULL;
    }
    if (st->request) {
        for (uint32_t i = 0; i < n_procs; ++i)
            free(st->request[i]);
        free(st->request);
        st->request = NULL;
    }
}

/* -----------------------------------------------------------------
   Build a fresh demo SystemState (2 resources, 3 processes).  This
   function is called every time we switch policies.
   ----------------------------------------------------------------- */
static SystemState *build_demo_state(void)
{
    uint32_t instances[2] = {2, 3};          /* 2 printers, 3 disks */
    SystemState *st = state_create(2, instances);
    allocate_state_matrices(st, 3);         /* 3 processes */

    /* maximum demand (the need matrix) */
    uint32_t max0[2] = {1, 2};
    uint32_t max1[2] = {2, 1};
    uint32_t max2[2] = {1, 1};

    for (uint32_t r = 0; r < 2; ++r) {
        st->request[0][r] = max0[r];
        st->request[1][r] = max1[r];
        st->request[2][r] = max2[r];
    }
    return st;
}

/* -----------------------------------------------------------------
   Insert the four demo request events (identical to the console demo).
   ----------------------------------------------------------------- */
static void schedule_demo_events(Scheduler *sched)
{
    Event ev;

    ev.time = 1; ev.type = EV_REQUEST; ev.pid = 0; ev.class_id = 0; ev.amount = 1;
    scheduler_schedule_event(sched, &ev);

    ev.time = 2; ev.type = EV_REQUEST; ev.pid = 0; ev.class_id = 1; ev.amount = 2;
    scheduler_schedule_event(sched, &ev);

    ev.time = 3; ev.type = EV_REQUEST; ev.pid = 1; ev.class_id = 0; ev.amount = 2;
    scheduler_schedule_event(sched, &ev);

    ev.time = 4; ev.type = EV_REQUEST; ev.pid = 2; ev.class_id = 1; ev.amount = 1;
    scheduler_schedule_event(sched, &ev);
}

/* -----------------------------------------------------------------
   Produce a textual snapshot (the same format you printed on the
   console).  The caller must free the returned string.
   ----------------------------------------------------------------- */
static char *make_snapshot_text(void)
{
    const SystemState *st = scheduler_state(g_sched);
    uint64_t now = scheduler_current_time(g_sched);
    const uint32_t n_procs   = st->n_procs;
    const uint32_t n_classes = st->n_classes;

    size_t bufsize = (1 + n_procs) * 200 + 100;
    char *buf = malloc(bufsize);
    char *p   = buf;
    int n;

    n = snprintf(p, bufsize, "\n=== time %lu ====================================\n", now);
    p += n; bufsize -= n;

    n = snprintf(p, bufsize, "Available per class : ");
    p += n; bufsize -= n;
    for (uint32_t r = 0; r < n_classes; ++r) {
        n = snprintf(p, bufsize, "%u ", st->available[r]);
        p += n; bufsize -= n;
    }
    n = snprintf(p, bufsize, "\n");
    p += n; bufsize -= n;

    for (uint32_t pid = 0; pid < n_procs; ++pid) {
        n = snprintf(p, bufsize, "P%u  alloc:", pid);
        p += n; bufsize -= n;
        for (uint32_t r = 0; r < n_classes; ++r) {
            n = snprintf(p, bufsize, " %u", st->allocation[pid][r]);
            p += n; bufsize -= n;
        }
        n = snprintf(p, bufsize, "   need:");
        p += n; bufsize -= n;
        for (uint32_t r = 0; r < n_classes; ++r) {
            n = snprintf(p, bufsize, " %u", st->request[pid][r]);
            p += n; bufsize -= n;
        }
        n = snprintf(p, bufsize, "\n");
        p += n; bufsize -= n;
    }
    return buf;
}

/* -----------------------------------------------------------------
   Update the snapshot view.
   ----------------------------------------------------------------- */
static void gui_update_snapshot(void)
{
    char *txt = make_snapshot_text();
    GtkTextBuffer *buf = gtk_text_view_get_buffer(g_text_view);
    gtk_text_buffer_set_text(buf, txt, -1);
    free(txt);
}

/* -----------------------------------------------------------------
   Update the dead‑lock counter label.
   ----------------------------------------------------------------- */
static void gui_update_deadlock_label(void)
{
    uint64_t cnt = 0;
    if (g_policy && g_policy->private) {
        /* All three policies store the counter as the first field of
           their private struct, so we can read it generically. */
        cnt = *((uint64_t *)g_policy->private);
    }
    char label[64];
    snprintf(label, sizeof(label), "Dead‑locks / postpones: %lu", cnt);
    gtk_label_set_text(g_deadlock_label, label);
}

/* -----------------------------------------------------------------
   Public: step the simulation by one tick and refresh the UI.
   ----------------------------------------------------------------- */
void gui_step(void)
{
    if (!g_sched) return;
    uint64_t now = scheduler_current_time(g_sched);
    scheduler_run_until(g_sched, now + 1);   /* process all events ≤ now+1 */
    gui_update_snapshot();
    gui_update_deadlock_label();
}

/* -----------------------------------------------------------------
   Public: return the dead‑lock counter (used by the label updater).
   ----------------------------------------------------------------- */
uint64_t gui_get_deadlock_counter(void)
{
    if (g_policy && g_policy->private)
        return *((uint64_t *)g_policy->private);
    return 0;
}

/* -----------------------------------------------------------------
   Public: destroy the current scheduler/policy and create a new one
   with the supplied policy object.
   ----------------------------------------------------------------- */
void gui_set_policy(Policy *new_policy)
{
    /* -------------------------------------------------------------
       1) Stop the periodic timeout while we tear everything down.
       ------------------------------------------------------------- */
    if (g_timeout_id != 0) {
        g_source_remove(g_timeout_id);
        g_timeout_id = 0;
    }

    /* -------------------------------------------------------------
       2) Clean up the old scheduler / policy / state.
       ------------------------------------------------------------- */
    if (g_sched) {
        SystemState *old_st = scheduler_state(g_sched);
        scheduler_destroy(g_sched);               /* also calls old_policy->cleanup */
        free_state_matrices(old_st, old_st->n_procs);
        state_destroy(old_st);
        g_sched = NULL;
    }
    /* The old policy pointer is already freed by scheduler_destroy(),
       but we set the global to NULL for safety. */
    g_policy = NULL;

    /* -------------------------------------------------------------
       3) Store the new policy.
       ------------------------------------------------------------- */
    g_policy = new_policy;        /* the factory already zero‑initialised the counter */

    /* -------------------------------------------------------------
       4) Build a fresh demo system.
       ------------------------------------------------------------- */
    SystemState *st = build_demo_state();

    /* -------------------------------------------------------------
       5) Create the scheduler that uses the new policy.
       ------------------------------------------------------------- */
    g_sched = scheduler_create(st, g_policy);

    /* -------------------------------------------------------------
       6) Insert the demo events.
       ------------------------------------------------------------- */
    schedule_demo_events(g_sched);

    /* -------------------------------------------------------------
       7) Refresh the UI (snapshot + dead‑lock counter).
       ------------------------------------------------------------- */
    gui_update_snapshot();
    gui_update_deadlock_label();

    /* -------------------------------------------------------------
       8) Restart the periodic timeout (1‑second step).
       ------------------------------------------------------------- */
    g_timeout_id = g_timeout_add_seconds(1, (GSourceFunc)gui_step, NULL);
}

/* -----------------------------------------------------------------
   Callbacks for the three “Run …” buttons.
   ----------------------------------------------------------------- */
static void on_run_graph_detect(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    Policy *pol = detect_policy_create(3, 2);   /* 3 processes, 2 resource classes */
    gui_set_policy(pol);
}
static void on_run_banker(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    Policy *pol = banker_policy_create(3, 2);
    gui_set_policy(pol);
}
static void on_run_holdwait(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    Policy *pol = holdwait_policy_create(3, 2);
    gui_set_policy(pol);
}

/* -----------------------------------------------------------------
   Callback for the “Step” button (manual advance).
   ----------------------------------------------------------------- */
static void on_step_clicked(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_step();
}

/* -----------------------------------------------------------------
   Build the whole window.
   ----------------------------------------------------------------- */
GtkWidget *gui_create_window(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Dead‑Lock Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* ---------------------------------------------------------
       Layout: vertical box (buttons on top, text view below)
       --------------------------------------------------------- */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* ---- Buttons ------------------------------------------------ */
    GtkWidget *hbox_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, FALSE, 0);

    GtkWidget *btn_graph = gtk_button_new_with_label("Run Graph‑Detect");
    GtkWidget *btn_banker = gtk_button_new_with_label("Run Banker");
    GtkWidget *btn_hw = gtk_button_new_with_label("Run Hold‑and‑Wait");
    GtkWidget *btn_step = gtk_button_new_with_label("Step");

    gtk_box_pack_start(GTK_BOX(hbox_buttons), btn_graph, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_buttons), btn_banker, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_buttons), btn_hw, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_buttons), btn_step, FALSE, FALSE, 0);

    g_signal_connect(btn_graph, "clicked", G_CALLBACK(on_run_graph_detect), NULL);
    g_signal_connect(btn_banker, "clicked", G_CALLBACK(on_run_banker), NULL);
    g_signal_connect(btn_hw,    "clicked", G_CALLBACK(on_run_holdwait), NULL);
    g_signal_connect(btn_step,  "clicked", G_CALLBACK(on_step_clicked), NULL);

    /* ---- Dead‑lock counter label -------------------------------- */
    g_deadlock_label = GTK_LABEL(gtk_label_new("Dead‑locks / postpones: 0"));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(g_deadlock_label), FALSE, FALSE, 0);

    /* ---- Text view (snapshot) ----------------------------------- */
    g_text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(g_text_view, FALSE);
    gtk_text_view_set_cursor_visible(g_text_view, FALSE);
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(g_text_view));
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    /* ---- Start the 1‑second timeout (will be paused/re‑started
           automatically when we switch policies) ----------------- */
    g_timeout_id = g_timeout_add_seconds(1, (GSourceFunc)gui_step, NULL);

    gtk_widget_show_all(window);
    return window;
}