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
#include <time.h>     /* für srand(time(NULL)) */

/* -----------------------------------------------------------------
   Alle globalen Variablen — müssen vor allen Funktionen stehen
   ----------------------------------------------------------------- */
static Scheduler   *g_sched             = NULL;
static Policy      *g_policy            = NULL;
static GtkTextView *g_text_view         = NULL;
static GtkLabel    *g_deadlock_label    = NULL;
static GtkLabel    *g_policy_name_label = NULL;
static GtkLabel    *g_tick_label        = NULL;
static GtkWidget   *g_btn_graph         = NULL;
static GtkWidget   *g_btn_banker        = NULL;
static GtkWidget   *g_btn_hw            = NULL;
static guint        g_timeout_id        = 0;

/* -----------------------------------------------------------------
   Vorwärts-Deklarationen — damit Funktionen die weiter unten
   definiert sind, schon vorher aufgerufen werden können
   ----------------------------------------------------------------- */
static void gui_update_tick_label(void);
static void gui_highlight_policy_btn(GtkWidget *active_btn, const char *name);

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
   Prozess-Lebenszyklus-Zustände für die prozedurale Generierung
   ----------------------------------------------------------------- */
typedef enum {
    PROC_IDLE,       /* Prozess hat keine Ressourcen und wartet */
    PROC_RUNNING,    /* Prozess hält Ressourcen und arbeitet     */
    PROC_DONE        /* Prozess ist fertig und wird neu gestartet */
} ProcLifeState;

/* Zustand jedes simulierten Prozesses */
typedef struct {
    ProcLifeState life;
    uint32_t      ticks_left;   /* verbleibende Ticks bevor nächste Aktion */
} ProcSim;

/* Globaler Prozesssimulator (wird bei Policy-Wechsel neu erstellt) */
static ProcSim  *g_procs     = NULL;
static uint32_t  g_n_procs   = 0;
static uint32_t  g_n_classes = 0;

/* Maximaler Bedarf pro Prozess/Klasse (wird aus dem State gelesen) */
static uint32_t **g_max_need = NULL;

static void procsim_destroy(void)
{
    free(g_procs);
    g_procs = NULL;
    if (g_max_need) {
        for (uint32_t i = 0; i < g_n_procs; ++i)
            free(g_max_need[i]);
        free(g_max_need);
        g_max_need = NULL;
    }
}

static void procsim_init(const SystemState *st)
{
    procsim_destroy();
    g_n_procs   = st->n_procs;
    g_n_classes = st->n_classes;

    g_procs = calloc(g_n_procs, sizeof(ProcSim));
    g_max_need = calloc(g_n_procs, sizeof(uint32_t *));
    for (uint32_t i = 0; i < g_n_procs; ++i) {
        g_max_need[i] = calloc(g_n_classes, sizeof(uint32_t));
        /* Maximaler Bedarf = initiale Request-Matrix */
        for (uint32_t r = 0; r < g_n_classes; ++r)
            g_max_need[i][r] = st->request[i][r];
        /* Alle Prozesse starten als IDLE, beginnen nach einem zufälligen Delay */
        g_procs[i].life       = PROC_IDLE;
        g_procs[i].ticks_left = 1 + (rand() % 3);
    }
}

/* -----------------------------------------------------------------
   Erzeugt prozedurale Events für den nächsten Tick.
   Aufgerufen von gui_step() bevor scheduler_run_until() läuft.
   ----------------------------------------------------------------- */
static void generate_proc_events(Scheduler *sched, uint64_t next_tick)
{
    const SystemState *st = scheduler_state(sched);

    for (uint32_t pid = 0; pid < g_n_procs; ++pid) {
        ProcSim *ps = &g_procs[pid];

        if (ps->ticks_left > 0) { ps->ticks_left--; continue; }

        Event ev;
        ev.retries = 0;

        switch (ps->life) {

            case PROC_IDLE: {
                /* Prozess startet: fordert für jede Ressourcenklasse
                   eine zufällige Menge zwischen 1 und max_need an */
                bool any = false;
                for (uint32_t r = 0; r < g_n_classes; ++r) {
                    uint32_t max = g_max_need[pid][r];
                    if (max == 0) continue;
                    uint32_t amt = 1 + (rand() % max);

                    ev.time     = next_tick;
                    ev.type     = EV_REQUEST;
                    ev.pid      = pid;
                    ev.class_id = r;
                    ev.amount   = amt;
                    scheduler_schedule_event(sched, &ev);

                    /* Request-Matrix aktualisieren damit Policy/Banker
                       den korrekten Bedarf kennt */
                    st->request[pid][r] = amt;
                    any = true;
                }
                if (any) {
                    ps->life       = PROC_RUNNING;
                    ps->ticks_left = 3 + (rand() % 5);  /* arbeitet 3-7 Ticks */
                }
                break;
            }

            case PROC_RUNNING: {
                /* Prozess ist fertig: gibt alle Ressourcen frei */
                for (uint32_t r = 0; r < g_n_classes; ++r) {
                    uint32_t held = st->allocation[pid][r];
                    if (held == 0) continue;
                    ev.time     = next_tick;
                    ev.type     = EV_RELEASE;
                    ev.pid      = pid;
                    ev.class_id = r;
                    ev.amount   = held;
                    scheduler_schedule_event(sched, &ev);
                }
                ps->life       = PROC_DONE;
                ps->ticks_left = 0;
                break;
            }

            case PROC_DONE: {
                /* Prozess terminiert und wird neu gestartet */
                ev.time    = next_tick;
                ev.type    = EV_TERMINATE;
                ev.pid     = pid;
                ev.class_id = 0;
                ev.amount   = 0;
                scheduler_schedule_event(sched, &ev);

                /* Request-Matrix auf maximalen Bedarf zurücksetzen */
                for (uint32_t r = 0; r < g_n_classes; ++r)
                    st->request[pid][r] = g_max_need[pid][r];

                ps->life       = PROC_IDLE;
                ps->ticks_left = 1 + (rand() % 3);   /* kurze Pause vor neuem Zyklus */
                break;
            }
        }
    }
}

/* -----------------------------------------------------------------
   Initialisiert den SystemState mit zufälligem Maximalbedarf
   ----------------------------------------------------------------- */
static SystemState *build_demo_state(void)
{
    uint32_t instances[2] = {2, 3};          /* 2 Drucker, 3 Festplatten */
    SystemState *st = state_create(2, instances);
    allocate_state_matrices(st, 3);          /* 3 Prozesse */

    /* Maximaler Bedarf — bleibt wie bisher fest für Reproduzierbarkeit */
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

    /* Buffer großzügig skalieren: pro Prozess eine Zeile mit je n_classes Feldern,
       plus Header-Zeilen. 32 Zeichen pro Feld ist großzügig bemessen. */
    size_t bufsize = 256                                /* Header */
                   + n_classes * 32                    /* "Available" Zeile */
                   + n_procs * (64 + n_classes * 32 * 2); /* Pro Prozess: alloc + need */
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
        cnt = *((uint64_t *)g_policy->private);
    }
    char label[64];
    snprintf(label, sizeof(label), "POSTPONED  %lu", cnt);
    gtk_label_set_text(g_deadlock_label, label);
}

/* -----------------------------------------------------------------
   Öffentliche Funktion — Signatur passend zu GSourceFunc:
   gboolean (*GSourceFunc)(gpointer).
   Gibt G_SOURCE_CONTINUE zurück, damit der periodische Timer weiterläuft.
   ----------------------------------------------------------------- */
gboolean gui_step(gpointer user_data)
{
    (void)user_data;
    if (!g_sched) return G_SOURCE_CONTINUE;
    uint64_t now = scheduler_current_time(g_sched);
    generate_proc_events(g_sched, now + 1);
    scheduler_run_until(g_sched, now + 1);
    gui_update_snapshot();
    gui_update_deadlock_label();
    gui_update_tick_label();
    return G_SOURCE_CONTINUE;
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
        state_destroy(old_st);   /* state_destroy already frees allocation/request matrices */
        g_sched = NULL;
    }
    procsim_destroy();   /* Prozess-Simulator-Zustand freigeben */
    g_policy = NULL;

    /* 3) Neue Policy setzen */
    g_policy = new_policy;

    /* 4) Neuen Demo-SystemState erstellen */
    SystemState *st = build_demo_state();

    /* 5) Neuen Scheduler erzeugen */
    g_sched = scheduler_create(st, g_policy);

    /* 6) Prozess-Simulator initialisieren (ersetzt die festen Demo-Events) */
    procsim_init(st);

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
    gui_highlight_policy_btn(g_btn_graph, "GRAPH DETECT");
    Policy *pol = detect_policy_create(3, 2);
    gui_set_policy(pol);
}

static void on_run_banker(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_banker, "BANKER'S ALGO");
    Policy *pol = banker_policy_create(3, 2);
    gui_set_policy(pol);
}

static void on_run_holdwait(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_hw, "HOLD & WAIT");
    Policy *pol = holdwait_policy_create(3, 2);
    gui_set_policy(pol);
}

/* -----------------------------------------------------------------
   Callback für den „Step“-Button (manueller Schritt)
   ----------------------------------------------------------------- */
static void on_step_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gui_step(user_data);   /* user_data wird ignoriert, Signatur passt aber zu GSourceFunc */
}

/* -----------------------------------------------------------------
   CSS für das dunkle Terminal-Design
   ----------------------------------------------------------------- */
static const char *GUI_CSS =
    /* Hauptfenster: tiefschwarz */
    "window {"
    "  background-color: #0d0f14;"
    "}"

    /* Titelleiste oben */
    "#header {"
    "  background-color: #0d0f14;"
    "  border-bottom: 2px solid #00e5ff;"
    "  padding: 16px 24px;"
    "}"
    "#title-label {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 22px;"
    "  font-weight: bold;"
    "  color: #00e5ff;"
    "  letter-spacing: 4px;"
    "}"
    "#subtitle-label {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 12px;"
    "  color: #546e7a;"
    "  letter-spacing: 2px;"
    "}"

    /* Policy-Buttons (aktive Auswahl) */
    "#btn-bar {"
    "  background-color: #111318;"
    "  padding: 14px 24px;"
    "  border-bottom: 1px solid #1e2530;"
    "}"
    ".policy-btn {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 15px;"
    "  font-weight: bold;"
    "  letter-spacing: 1px;"
    "  color: #90a4ae;"
    "  background-color: #1a1f2b;"
    "  border: 2px solid #2a3240;"
    "  border-radius: 4px;"
    "  padding: 10px 20px;"
    "  min-width: 200px;"
    "}"
    ".policy-btn:hover {"
    "  background-color: #1e2840;"
    "  border-color: #00e5ff;"
    "  color: #ffffff;"
    "}"
    ".policy-btn.active {"
    "  background-color: #003d4d;"
    "  border-color: #00e5ff;"
    "  color: #00e5ff;"
    "}"

    /* Step-Button */
    "#btn-step {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 15px;"
    "  font-weight: bold;"
    "  color: #ffd54f;"
    "  background-color: #1a1a00;"
    "  border: 2px solid #ffd54f;"
    "  border-radius: 4px;"
    "  padding: 10px 24px;"
    "}"
    "#btn-step:hover {"
    "  background-color: #332b00;"
    "  color: #fff176;"
    "  border-color: #fff176;"
    "}"

    /* Status-Leiste mit Zähler und Tick */
    "#status-bar {"
    "  background-color: #111318;"
    "  border-bottom: 1px solid #1e2530;"
    "  padding: 10px 24px;"
    "}"
    "#deadlock-label {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 16px;"
    "  color: #ff5252;"
    "  font-weight: bold;"
    "}"
    "#policy-name-label {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 14px;"
    "  color: #00e5ff;"
    "  font-weight: bold;"
    "}"
    "#tick-label {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 14px;"
    "  color: #546e7a;"
    "}"

    /* TextView: Monospace, große Schrift, dunkler Hintergrund */
    "textview {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 18px;"
    "  color: #e0f7fa;"
    "  background-color: #0a0c10;"
    "  padding: 20px;"
    "}"
    "textview text {"
    "  background-color: #0a0c10;"
    "  color: #e0f7fa;"
    "}"
    "scrolledwindow {"
    "  background-color: #0a0c10;"
    "  border: 1px solid #1e2530;"
    "  margin: 12px 16px 16px 16px;"
    "}"

    /* Legende / Info-Panel rechts */
    "#legend-box {"
    "  background-color: #111318;"
    "  border-left: 2px solid #1e2530;"
    "  padding: 20px 16px;"
    "  min-width: 220px;"
    "}"
    "#legend-title {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 11px;"
    "  color: #546e7a;"
    "  letter-spacing: 2px;"
    "  margin-bottom: 12px;"
    "}"
    ".legend-row {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 14px;"
    "  color: #b0bec5;"
    "  margin-bottom: 8px;"
    "}"
    ".legend-key {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 13px;"
    "  color: #00e5ff;"
    "  font-weight: bold;"
    "}"
    "#idle-indicator {"
    "  font-family: 'Courier New', monospace;"
    "  font-size: 13px;"
    "  color: #546e7a;"
    "  font-style: italic;"
    "  margin-top: 24px;"
    "}";

/* Extra Labels und Button-Referenzen werden oben als globale Variablen gehalten */

/* -----------------------------------------------------------------
   Aktualisiert den Tick-Counter im Status-Bar
   ----------------------------------------------------------------- */
static void gui_update_tick_label(void)
{
    if (!g_tick_label || !g_sched) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "TICK  %lu", scheduler_current_time(g_sched));
    gtk_label_set_text(g_tick_label, buf);
}

/* -----------------------------------------------------------------
   Markiert den aktiven Policy-Button und setzt den Namen
   ----------------------------------------------------------------- */
static void gui_highlight_policy_btn(GtkWidget *active_btn, const char *name)
{
    GtkWidget *btns[3] = { g_btn_graph, g_btn_banker, g_btn_hw };
    for (int i = 0; i < 3; ++i) {
        if (!btns[i]) continue;
        GtkStyleContext *ctx = gtk_widget_get_style_context(btns[i]);
        if (btns[i] == active_btn)
            gtk_style_context_add_class(ctx, "active");
        else
            gtk_style_context_remove_class(ctx, "active");
    }
    if (g_policy_name_label && name) {
        char buf[64];
        snprintf(buf, sizeof(buf), "POLICY  %s", name);
        gtk_label_set_text(g_policy_name_label, buf);
    }
}

/* -----------------------------------------------------------------
   Erstellt das komplette Fenster
   ----------------------------------------------------------------- */
GtkWidget *gui_create_window(void)
{
    srand((unsigned)time(NULL));

    /* ---- CSS laden ---- */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, GUI_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* ---- Hauptfenster ---- */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Deadlock Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 720);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* ---- Äußere vertikale Box ---- */
    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), root_vbox);

    /* ===== HEADER ===== */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(header, "header");
    gtk_box_pack_start(GTK_BOX(root_vbox), header, FALSE, FALSE, 0);

    GtkWidget *title_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(header), title_vbox, TRUE, TRUE, 0);

    GtkWidget *title_lbl = gtk_label_new("DEADLOCK SIMULATOR");
    gtk_widget_set_name(title_lbl, "title-label");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(title_vbox), title_lbl, FALSE, FALSE, 0);

    GtkWidget *sub_lbl = gtk_label_new("OPERATING SYSTEMS · RESOURCE MANAGEMENT");
    gtk_widget_set_name(sub_lbl, "subtitle-label");
    gtk_label_set_xalign(GTK_LABEL(sub_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(title_vbox), sub_lbl, FALSE, FALSE, 0);

    /* ===== BUTTON BAR ===== */
    GtkWidget *btn_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(btn_bar, "btn-bar");
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_bar, FALSE, FALSE, 0);

    g_btn_graph  = gtk_button_new_with_label("⬡  GRAPH DETECT");
    g_btn_banker = gtk_button_new_with_label("⬡  BANKER'S ALGO");
    g_btn_hw     = gtk_button_new_with_label("⬡  HOLD & WAIT");
    GtkWidget *btn_step = gtk_button_new_with_label("▶  STEP");

    GtkWidget *policy_btns[3] = { g_btn_graph, g_btn_banker, g_btn_hw };
    for (int i = 0; i < 3; ++i) {
        gtk_style_context_add_class(
            gtk_widget_get_style_context(policy_btns[i]), "policy-btn");
        gtk_box_pack_start(GTK_BOX(btn_bar), policy_btns[i], FALSE, FALSE, 0);
    }
    gtk_widget_set_name(btn_step, "btn-step");
    /* Step rechts ausrichten */
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(btn_bar), spacer, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_bar), btn_step, FALSE, FALSE, 0);

    g_signal_connect(g_btn_graph,  "clicked", G_CALLBACK(on_run_graph_detect), NULL);
    g_signal_connect(g_btn_banker, "clicked", G_CALLBACK(on_run_banker),       NULL);
    g_signal_connect(g_btn_hw,     "clicked", G_CALLBACK(on_run_holdwait),     NULL);
    g_signal_connect(btn_step,     "clicked", G_CALLBACK(on_step_clicked),     NULL);

    /* ===== STATUS BAR ===== */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_name(status_bar, "status-bar");
    gtk_box_pack_start(GTK_BOX(root_vbox), status_bar, FALSE, FALSE, 0);

    g_policy_name_label = GTK_LABEL(gtk_label_new("POLICY  —"));
    gtk_widget_set_name(GTK_WIDGET(g_policy_name_label), "policy-name-label");
    gtk_label_set_xalign(GTK_LABEL(g_policy_name_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_policy_name_label), FALSE, FALSE, 0);

    /* Trennzeichen */
    GtkWidget *sep_lbl = gtk_label_new("·");
    gtk_widget_set_name(sep_lbl, "tick-label");
    gtk_box_pack_start(GTK_BOX(status_bar), sep_lbl, FALSE, FALSE, 0);

    g_deadlock_label = GTK_LABEL(gtk_label_new("POSTPONED  0"));
    gtk_widget_set_name(GTK_WIDGET(g_deadlock_label), "deadlock-label");
    gtk_label_set_xalign(GTK_LABEL(g_deadlock_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_deadlock_label), FALSE, FALSE, 0);

    GtkWidget *sep_lbl2 = gtk_label_new("·");
    gtk_widget_set_name(sep_lbl2, "tick-label");
    gtk_box_pack_start(GTK_BOX(status_bar), sep_lbl2, FALSE, FALSE, 0);

    g_tick_label = GTK_LABEL(gtk_label_new("TICK  0"));
    gtk_widget_set_name(GTK_WIDGET(g_tick_label), "tick-label");
    gtk_label_set_xalign(GTK_LABEL(g_tick_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_tick_label), FALSE, FALSE, 0);

    /* ===== HAUPTBEREICH: TextView links, Legende rechts ===== */
    GtkWidget *content_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), content_hbox, TRUE, TRUE, 0);

    /* TextView in scrollbarem Fenster */
    g_text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(g_text_view, FALSE);
    gtk_text_view_set_cursor_visible(g_text_view, FALSE);
    gtk_text_view_set_left_margin(g_text_view, 12);
    gtk_text_view_set_top_margin(g_text_view, 12);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(g_text_view));
    gtk_box_pack_start(GTK_BOX(content_hbox), scrolled, TRUE, TRUE, 0);

    /* ===== LEGENDE rechts ===== */
    GtkWidget *legend_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(legend_box, "legend-box");
    gtk_box_pack_start(GTK_BOX(content_hbox), legend_box, FALSE, FALSE, 0);

    GtkWidget *legend_title = gtk_label_new("KEY");
    gtk_widget_set_name(legend_title, "legend-title");
    gtk_label_set_xalign(GTK_LABEL(legend_title), 0.0);
    gtk_box_pack_start(GTK_BOX(legend_box), legend_title, FALSE, FALSE, 0);

    /* Legende Einträge */
    const char *legend_entries[][2] = {
        { "alloc",    "resources held"    },
        { "need",     "still required"    },
        { "avail",    "free instances"    },
        { "POSTPONED","blocked requests"  },
        { "TICK",     "simulation clock"  },
    };
    for (int i = 0; i < 5; ++i) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_name(row, "legend-row");

        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "%s", legend_entries[i][0]);
        GtkWidget *key_lbl = gtk_label_new(key_buf);
        gtk_widget_set_name(key_lbl, "legend-key");
        gtk_label_set_xalign(GTK_LABEL(key_lbl), 0.0);

        char val_buf[64];
        snprintf(val_buf, sizeof(val_buf), "%s", legend_entries[i][1]);
        GtkWidget *val_lbl = gtk_label_new(val_buf);
        gtk_widget_set_name(val_lbl, "legend-row");
        gtk_label_set_xalign(GTK_LABEL(val_lbl), 0.0);

        gtk_box_pack_start(GTK_BOX(row), key_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), val_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(legend_box), row, FALSE, FALSE, 8);
    }

    /* Hinweistext unten in der Legende */
    GtkWidget *idle_lbl = gtk_label_new("Select a policy\nto begin.");
    gtk_widget_set_name(idle_lbl, "idle-indicator");
    gtk_label_set_xalign(GTK_LABEL(idle_lbl), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(idle_lbl), TRUE);
    gtk_box_pack_end(GTK_BOX(legend_box), idle_lbl, FALSE, FALSE, 0);

    /* ===== Timer starten ===== */
    g_timeout_id = g_timeout_add_seconds(1, (GSourceFunc)gui_step, NULL);

    gtk_widget_show_all(window);
    return window;
}