/*=====================================================================
 * Diese Datei implementiert die grafische Oberfläche mit folgenden Aufgaben
 *      Stoppt den periodischen Timer bevor der alte Scheduler zerstört wird
 *      Zerstört Scheduler, Policy und SystemState sauber
 *      Erstellt einen neuen Scheduler mit der neu gewählten Policy
 *      Setzt den Deadlock-Counter zurück
 *      Startet den Timer neu
 *
 * Es sind Diese Architekturen vorhanden
 *      Buttons oben (Policy-Auswahl + Step)
 *      Label für den Deadlock-Counter
 *      TextView für den Systemzustad (scrollbar)
 *      Automatisches Update jede Sekunde
 *====================================================================*/

#include "../gui/gui.h"
#include "../core/state.h"
#include "../core/event.h"
#include "../core/types.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------
   Globale Objekte es gibt immer nur einen Aktiven Scheduler/Policy zu gleichen Zeit
   ----------------------------------------------------------------- */
static Scheduler   *g_sched          = NULL;   /* aktueller Scheduler   */
static Policy      *g_policy         = NULL;   /* aktuelle Policy      */
static GtkTextView *g_text_view      = NULL;   /* Anzeige des Systemzustands      */
static GtkLabel    *g_deadlock_label = NULL;   /* Deadlock-Zähler   */

/* -----------------------------------------------------------------
   Timeout-Source-ID wird gespeichert um den Timer beim
   Neuaufbau der Simulation stoppen zu können
   ----------------------------------------------------------------- */
static guint g_timeout_id = 0;

/* -----------------------------------------------------------------
   Das ist eine hilfsfunktion heißt Allokiert die Allocation und Request-Matrizen
   und speichert die Anzahl der Prozesse im SystemState
   (benötigt für den Safety-Check des Bankiers)
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

    st->n_procs = n_procs;          /* wird für den Banker-Check benötigt */
}

/* -----------------------------------------------------------------
   Gibt die Matrizen wieder frei
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
   Erstelllt einen neuen Demo-SystemState
   (2 Ressourcenklassen, 3 Prozesse)
   Diese Funktion wird bei jedem Policy-Wechsel aufgerufen
   ----------------------------------------------------------------- */
static SystemState *build_demo_state(void)
{
    uint32_t instances[2] = {2, 3};          /* 2 Drucker, 3 Festplatten*/
    SystemState *st = state_create(2, instances);
    allocate_state_matrices(st, 3);         /* 3 Prozesse  */

    /* Maximale Anforderungen (Need-Matrix)*/
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
   Fügt die vier Demo-Request-Events ein
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
    Erzeugt einen textuellen Snapshot des Systemzustands
    (gleiches Format wie die Konsolenausgabe).
    Der Rückgabestring muss vom Aufrufer freigegeben werden.
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
   Aktualisiert die Snapshot_Anzeige
   ----------------------------------------------------------------- */
static void gui_update_snapshot(void)
{
    char *txt = make_snapshot_text();
    GtkTextBuffer *buf = gtk_text_view_get_buffer(g_text_view);
    gtk_text_buffer_set_text(buf, txt, -1);
    free(txt);
}

/* -----------------------------------------------------------------
   Aktualisiert das deadlock-Zähler-Label
   ----------------------------------------------------------------- */
static void gui_update_deadlock_label(void)
{
    uint64_t cnt = 0;
    if (g_policy && g_policy->private) {
        /* Alle drei Policies speichern den Zähler
           als erstes Feld ihrer privaten Struktur */
        cnt = *((uint64_t *)g_policy->private);
    }
    char label[64];
    snprintf(label, sizeof(label), "Dead‑locks / postpones: %lu", cnt);
    gtk_label_set_text(g_deadlock_label, label);
}

/* -----------------------------------------------------------------
   Ist eine Öffentliche Funktion führt die Simulation um einen Tick weiter
   und aktualisiert die GUI
   ----------------------------------------------------------------- */
void gui_step(void)
{
    if (!g_sched) return;
    uint64_t now = scheduler_current_time(g_sched);
    scheduler_run_until(g_sched, now + 1);   /* Prozesse der Events ≤ now+1 */
    gui_update_snapshot();
    gui_update_deadlock_label();
}

/* -----------------------------------------------------------------
   Öffentliche Funktion gibt den Deadlock-Zähler zurück
   ----------------------------------------------------------------- */
uint64_t gui_get_deadlock_counter(void)
{
    if (g_policy && g_policy->private)
        return *((uint64_t *)g_policy->private);
    return 0;
}

/* -----------------------------------------------------------------
   Öffentliche Funktion zerstört den aktuellen Scheduler/Policy
   und erstellt einen neuen mit der übergebenen Policy
   ----------------------------------------------------------------- */
void gui_set_policy(Policy *new_policy)
{
    /* 1) Periodischen Timer stoppen */
    if (g_timeout_id != 0) {
        g_source_remove(g_timeout_id);
        g_timeout_id = 0;
    }

    /* 2) Alten Scheduler / State / Policy aufräumen */
    if (g_sched) {
        SystemState *old_st = scheduler_state(g_sched);
        scheduler_destroy(g_sched);
        free_state_matrices(old_st, old_st->n_procs);
        state_destroy(old_st);
        g_sched = NULL;
    }
    g_policy = NULL;

    /* 3) Neue Policy setzen */
    g_policy = new_policy;

    /* 4) Neuen Demo-SystemState erstellen */
    SystemState *st = build_demo_state();

    /* 5) Neuen Scheduler erzeugen */
    g_sched = scheduler_create(st, g_policy);

    /* 6) Demo-Events einplanen */
    schedule_demo_events(g_sched);

    /* 7) GUI aktualisieren */
    gui_update_snapshot();
    gui_update_deadlock_label();

    /* 8) Periodischen Timer neu starten */
    g_timeout_id = g_timeout_add_seconds(1,
        (GSourceFunc)gui_step, NULL);
}

/* -----------------------------------------------------------------
   Callbacks für die drei „Run …“-Buttons
   ----------------------------------------------------------------- */
static void on_run_graph_detect(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    Policy *pol = detect_policy_create(3, 2);
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
   Callback für den „Step“-Button (manueller Schritt)
   ----------------------------------------------------------------- */
static void on_step_clicked(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_step();
}

/* -----------------------------------------------------------------
   Erstellt das komplette Fenster
   ----------------------------------------------------------------- */
GtkWidget *gui_create_window(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Deadlock-Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    /* Layout Vertikale Box
       (Buttons oben, Textansicht darunter) */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* Buttons */
    GtkWidget *hbox_buttons =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox),
                       hbox_buttons, FALSE, FALSE, 0);

    GtkWidget *btn_graph =
        gtk_button_new_with_label("Run Graph-Detect");
    GtkWidget *btn_banker =
        gtk_button_new_with_label("Run Banker");
    GtkWidget *btn_hw =
        gtk_button_new_with_label("Run Hold-and-Wait");
    GtkWidget *btn_step =
        gtk_button_new_with_label("Step");

    gtk_box_pack_start(GTK_BOX(hbox_buttons),
                       btn_graph, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_buttons),
                       btn_banker, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_buttons),
                       btn_hw, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_buttons),
                       btn_step, FALSE, FALSE, 0);

    g_signal_connect(btn_graph, "clicked",
                     G_CALLBACK(on_run_graph_detect), NULL);
    g_signal_connect(btn_banker, "clicked",
                     G_CALLBACK(on_run_banker), NULL);
    g_signal_connect(btn_hw, "clicked",
                     G_CALLBACK(on_run_holdwait), NULL);
    g_signal_connect(btn_step, "clicked",
                     G_CALLBACK(on_step_clicked), NULL);

    /* Deadlock-Zähler */
    g_deadlock_label =
        GTK_LABEL(gtk_label_new("Deadlocks / Verschiebungen: 0"));
    gtk_box_pack_start(GTK_BOX(vbox),
                       GTK_WIDGET(g_deadlock_label),
                       FALSE, FALSE, 0);

    /* Textansicht */
    g_text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(g_text_view, FALSE);
    gtk_text_view_set_cursor_visible(g_text_view, FALSE);

    GtkWidget *scrolled =
        gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled),
                      GTK_WIDGET(g_text_view));
    gtk_box_pack_start(GTK_BOX(vbox),
                       scrolled, TRUE, TRUE, 0);

    /* Start des 1-Sekunden-Timers */
    g_timeout_id =
        g_timeout_add_seconds(1,
            (GSourceFunc)gui_step, NULL);

    gtk_widget_show_all(window);
    return window;
}