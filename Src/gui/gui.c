/**
 * @file gui.c
 * @brief Implementierung des GTK-3-Frontends für den Deadlock-Simulator.
 *
 * Diese Datei implementiert die gesamte grafische Benutzeroberfläche.
 * Das Fenster besteht aus folgenden Bereichen:
 * - **Header**: Titel und Untertitel der Anwendung.
 * - **Button-Leiste**: Drei Policy-Buttons und ein Step-Button.
 * - **Statusleiste**: Aktive Policy, Verschiebungs-Zähler, aktueller Tick.
 * - **Hauptbereich**: Scrollbares Textfenster mit Systemzustand-Snapshot (links)
 *   und statische Legende (rechts).
 *
 * Die Simulation arbeitet mit einem prozeduralen Prozessmodell: Jeder Prozess
 * durchläuft die Zustände IDLE → RUNNING → DONE und wird dann neu gestartet.
 * Alle Ressourcenanforderungen und -freigaben werden als Ereignisse in den
 * Scheduler eingespeist.
 */

#include "../gui/gui.h"
#include "../core/state.h"
#include "../core/event.h"
#include "../core/types.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ==========================================================================
   Globale Zustandsvariablen
   ========================================================================== */

/** @brief Zeiger auf den aktiven Simulator-Scheduler. NULL, solange keine Policy gewählt. */
static Scheduler   *g_sched             = NULL;

/** @brief Zeiger auf die aktive Deadlock-Policy. NULL vor der ersten Policy-Auswahl. */
static Policy      *g_policy            = NULL;

/** @brief Referenz auf das TextView-Widget für den Systemzustand-Snapshot. */
static GtkTextView *g_text_view         = NULL;

/** @brief Referenz auf das Label, das den Verschiebungs-Zähler anzeigt. */
static GtkLabel    *g_deadlock_label    = NULL;

/** @brief Referenz auf das Label, das den Namen der aktiven Policy anzeigt. */
static GtkLabel    *g_policy_name_label = NULL;

/** @brief Referenz auf das Label, das den aktuellen Tick anzeigt. */
static GtkLabel    *g_tick_label        = NULL;

/** @brief Referenz auf den "GRAPH-ERKENNUNG"-Button (für Hervorhebung). */
static GtkWidget   *g_btn_graph         = NULL;

/** @brief Referenz auf den "BANKIER-ALGO"-Button (für Hervorhebung). */
static GtkWidget   *g_btn_banker        = NULL;

/** @brief Referenz auf den "HALTE-UND-WARTE"-Button (für Hervorhebung). */
static GtkWidget   *g_btn_hw            = NULL;

/** @brief GTK-Timer-ID für den automatischen Simulationsschritt (1 Sekunde). 0 = kein aktiver Timer. */
static guint        g_timeout_id        = 0;

/* ==========================================================================
   Vorwärts-Deklarationen
   ========================================================================== */

static void gui_update_tick_label(void);
static void gui_highlight_policy_btn(GtkWidget *active_btn, const char *name);

/* ==========================================================================
   Interne Hilfsfunktionen: Zustandsverwaltung
   ========================================================================== */

/**
 * @brief Allokiert die Allokations- und Bedarfsmatrizen im Systemzustand.
 *
 * Diese lokale Variante wird verwendet, da die GUI direkten Zugriff auf die
 * Felder von @c SystemState benötigt (ohne den Umweg über @c state_allocate_process_matrices()).
 * Beide Matrizen werden mit Nullen initialisiert. @c n_procs wird im State gespeichert,
 * damit der Bankier-Algorithmus später korrekt arbeitet.
 *
 * @param st       Zeiger auf den Systemzustand.
 * @param n_procs  Anzahl der simulierten Prozesse.
 */
static void allocate_state_matrices(SystemState *st, uint32_t n_procs)
{
    uint32_t n_classes = st->n_classes;

    st->allocation = calloc(n_procs, sizeof(uint32_t *));
    for (uint32_t i = 0; i < n_procs; ++i)
        st->allocation[i] = calloc(n_classes, sizeof(uint32_t));

    st->request = calloc(n_procs, sizeof(uint32_t *));
    for (uint32_t i = 0; i < n_procs; ++i)
        st->request[i] = calloc(n_classes, sizeof(uint32_t));

    st->n_procs = n_procs;
}

/**
 * @brief Gibt die Allokations- und Bedarfsmatrizen eines Systemzustands frei.
 *
 * @param st       Zeiger auf den Systemzustand.
 * @param n_procs  Anzahl der Zeilen (Prozesse) in den Matrizen.
 */
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

/* ==========================================================================
   Prozess-Simulator
   ========================================================================== */

/**
 * @brief Lebenszyklus-Zustände eines simulierten Prozesses.
 */
typedef enum {
    PROC_IDLE,    /**< Prozess besitzt keine Ressourcen und wartet auf seinen nächsten Start. */
    PROC_RUNNING, /**< Prozess hält Ressourcen und führt seine Arbeit aus. */
    PROC_DONE     /**< Prozess hat seine Arbeit abgeschlossen und gibt Ressourcen frei. */
} ProcLifeState;

/**
 * @brief Laufzeit-Zustand eines einzelnen simulierten Prozesses.
 */
typedef struct {
    ProcLifeState life;        /**< Aktueller Lebenszyklus-Zustand. */
    uint32_t      ticks_left;  /**< Verbleibende Ticks bis zur nächsten Zustandsänderung. */
} ProcSim;

/** @brief Array aller simulierten Prozesse. */
static ProcSim  *g_procs     = NULL;

/** @brief Anzahl der simulierten Prozesse. */
static uint32_t  g_n_procs   = 0;

/** @brief Anzahl der Ressourcenklassen (wird bei Policy-Wechsel gesetzt). */
static uint32_t  g_n_classes = 0;

/** @brief Maximaler Bedarf je Prozess und Ressourcenklasse (aus der initialen Request-Matrix). */
static uint32_t **g_max_need = NULL;

/**
 * @brief Gibt den Prozess-Simulator und seinen Speicher frei.
 *
 * Setzt alle globalen Zeiger auf NULL, um Doppel-Freigaben zu verhindern.
 */
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

/**
 * @brief Initialisiert den Prozess-Simulator anhand des gegebenen Systemzustands.
 *
 * Liest die initiale Request-Matrix als maximalen Bedarf ein und setzt alle
 * Prozesse in den IDLE-Zustand mit einem zufälligen Anfangs-Delay von 1-3 Ticks.
 *
 * @param st  Zeiger auf den initialisierten Systemzustand (mit Request-Matrix).
 */
static void procsim_init(const SystemState *st)
{
    procsim_destroy();
    g_n_procs   = st->n_procs;
    g_n_classes = st->n_classes;

    g_procs    = calloc(g_n_procs, sizeof(ProcSim));
    g_max_need = calloc(g_n_procs, sizeof(uint32_t *));
    for (uint32_t i = 0; i < g_n_procs; ++i) {
        g_max_need[i] = calloc(g_n_classes, sizeof(uint32_t));
        /* Maximaler Bedarf aus der initialen Request-Matrix übernehmen */
        for (uint32_t r = 0; r < g_n_classes; ++r)
            g_max_need[i][r] = st->request[i][r];
        /* Zufälliger Start-Delay, damit Prozesse nicht alle gleichzeitig starten */
        g_procs[i].life       = PROC_IDLE;
        g_procs[i].ticks_left = 1 + (rand() % 3);
    }
}

/**
 * @brief Erzeugt prozedurale Ereignisse für alle Prozesse im nächsten Tick.
 *
 * Für jeden Prozess wird anhand seines Lebenszyklus-Zustands entschieden,
 * welche Ereignisse für @p next_tick eingeplant werden:
 * - **IDLE**: Plant @c EV_REQUEST-Ereignisse für alle Ressourcenklassen ein
 *   und wechselt in den RUNNING-Zustand.
 * - **RUNNING**: Plant @c EV_RELEASE-Ereignisse für alle gehaltenen Ressourcen ein
 *   und wechselt in den DONE-Zustand.
 * - **DONE**: Plant ein @c EV_TERMINATE-Ereignis ein, setzt die Request-Matrix
 *   zurück und wechselt wieder in den IDLE-Zustand.
 *
 * @param sched      Zeiger auf den aktiven Scheduler.
 * @param next_tick  Zeitstempel, für den die Ereignisse eingeplant werden.
 */
static void generate_proc_events(Scheduler *sched, uint64_t next_tick)
{
    const SystemState *st = scheduler_state(sched);

    for (uint32_t pid = 0; pid < g_n_procs; ++pid) {
        ProcSim *ps = &g_procs[pid];

        /* Tick herunterzählen; bei > 0 noch nicht aktiv */
        if (ps->ticks_left > 0) { ps->ticks_left--; continue; }

        Event ev;
        ev.retries = 0;

        switch (ps->life) {

            case PROC_IDLE: {
                /* Prozess startet: Anforderungen für alle Ressourcenklassen einplanen */
                bool any = false;
                for (uint32_t r = 0; r < g_n_classes; ++r) {
                    uint32_t max = g_max_need[pid][r];
                    if (max == 0) continue;
                    uint32_t amt = 1 + (rand() % max);  /* Zufällige Menge zwischen 1 und max */

                    ev.time     = next_tick;
                    ev.type     = EV_REQUEST;
                    ev.pid      = pid;
                    ev.class_id = r;
                    ev.amount   = amt;
                    scheduler_schedule_event(sched, &ev);

                    /* Request-Matrix aktualisieren, damit Policy den korrekten Bedarf kennt */
                    st->request[pid][r] = amt;
                    any = true;
                }
                if (any) {
                    ps->life       = PROC_RUNNING;
                    ps->ticks_left = 3 + (rand() % 5);  /* Arbeitsphase: 3–7 Ticks */
                }
                break;
            }

            case PROC_RUNNING: {
                /* Prozess fertig: alle gehaltenen Ressourcen freigeben */
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
                /* Prozess terminiert und wird für den nächsten Zyklus vorbereitet */
                ev.time     = next_tick;
                ev.type     = EV_TERMINATE;
                ev.pid      = pid;
                ev.class_id = 0;
                ev.amount   = 0;
                scheduler_schedule_event(sched, &ev);

                /* Request-Matrix auf maximalen Bedarf zurücksetzen */
                for (uint32_t r = 0; r < g_n_classes; ++r)
                    st->request[pid][r] = g_max_need[pid][r];

                ps->life       = PROC_IDLE;
                ps->ticks_left = 1 + (rand() % 3);  /* Kurze Pause vor dem nächsten Zyklus */
                break;
            }
        }
    }
}

/* ==========================================================================
   Demo-Szenario
   ========================================================================== */

/**
 * @brief Erstellt einen festen Demo-Systemzustand für die Simulation.
 *
 * Das Szenario umfasst 2 Ressourcenklassen (2 Drucker, 3 Festplatten)
 * und 3 Prozesse mit fest definierten maximalen Bedarfswerten. Die
 * Reproduzierbarkeit des Szenarios bleibt durch die festen Werte erhalten.
 *
 * @return Zeiger auf den neu erstellten und initialisierten @c SystemState.
 */
static SystemState *build_demo_state(void)
{
    uint32_t instances[2] = {2, 3};    /* Klasse 0: 2 Drucker, Klasse 1: 3 Festplatten */
    SystemState *st = state_create(2, instances);
    allocate_state_matrices(st, 3);    /* 3 simulierte Prozesse */

    /* Maximaler Bedarf je Prozess (Zeile) und Ressourcenklasse (Spalte) */
    uint32_t max0[2] = {1, 2};  /* P0: max. 1 Drucker, 2 Festplatten */
    uint32_t max1[2] = {2, 1};  /* P1: max. 2 Drucker, 1 Festplatte  */
    uint32_t max2[2] = {1, 1};  /* P2: max. 1 Drucker, 1 Festplatte  */

    for (uint32_t r = 0; r < 2; ++r) {
        st->request[0][r] = max0[r];
        st->request[1][r] = max1[r];
        st->request[2][r] = max2[r];
    }
    return st;
}

/* ==========================================================================
   GUI-Aktualisierungsfunktionen
   ========================================================================== */

/**
 * @brief Erstellt einen textuellen Snapshot des aktuellen Systemzustands.
 *
 * Formatiert den Systemzustand als mehrzeiligen String mit Zeit-Angabe,
 * verfügbaren Ressourcen sowie den Allokations- und Bedarfswerten aller Prozesse.
 * Der Puffer wird dynamisch allokiert und muss vom Aufrufer freigegeben werden.
 *
 * @return Dynamisch allokierter String mit dem formatierten Systemzustand.
 *         Der Aufrufer ist für das Freigeben mit @c free() verantwortlich.
 */
static char *make_snapshot_text(void)
{
    const SystemState *st = scheduler_state(g_sched);
    uint64_t now          = scheduler_current_time(g_sched);
    const uint32_t n_procs   = st->n_procs;
    const uint32_t n_classes = st->n_classes;

    /* Puffergröße großzügig berechnen: Header + verfügbare Ressourcen + pro Prozess eine Zeile */
    size_t bufsize = 256
                   + n_classes * 32
                   + n_procs * (64 + n_classes * 32 * 2);
    char *buf = malloc(bufsize);
    char *p   = buf;
    int   n;

    n = snprintf(p, bufsize, "\n=== Takt %lu ====================================\n", now);
    p += n; bufsize -= n;

    n = snprintf(p, bufsize, "Verfügbar je Klasse : ");
    p += n; bufsize -= n;
    for (uint32_t r = 0; r < n_classes; ++r) {
        n = snprintf(p, bufsize, "%u ", st->available[r]);
        p += n; bufsize -= n;
    }
    n = snprintf(p, bufsize, "\n");
    p += n; bufsize -= n;

    for (uint32_t pid = 0; pid < n_procs; ++pid) {
        n = snprintf(p, bufsize, "P%u  zugeteilt:", pid);
        p += n; bufsize -= n;
        for (uint32_t r = 0; r < n_classes; ++r) {
            n = snprintf(p, bufsize, " %u", st->allocation[pid][r]);
            p += n; bufsize -= n;
        }
        n = snprintf(p, bufsize, "   bedarf:");
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

/**
 * @brief Aktualisiert das Systemzustand-Snapshot im TextView.
 */
static void gui_update_snapshot(void)
{
    char *txt = make_snapshot_text();
    GtkTextBuffer *buf = gtk_text_view_get_buffer(g_text_view);
    gtk_text_buffer_set_text(buf, txt, -1);
    free(txt);
}

/**
 * @brief Aktualisiert das Label mit dem aktuellen Verschiebungs-Zähler.
 *
 * Liest den Zähler aus dem ersten @c uint64_t-Feld des privaten Policy-Kontexts.
 */
static void gui_update_deadlock_label(void)
{
    uint64_t cnt = 0;
    if (g_policy && g_policy->private) {
        cnt = *((uint64_t *)g_policy->private);
    }
    char label[64];
    snprintf(label, sizeof(label), "VERSCHOBEN  %lu", cnt);
    gtk_label_set_text(g_deadlock_label, label);
}

/**
 * @brief Aktualisiert das Tick-Label in der Statusleiste.
 */
static void gui_update_tick_label(void)
{
    if (!g_tick_label || !g_sched) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "TAKT  %lu", scheduler_current_time(g_sched));
    gtk_label_set_text(g_tick_label, buf);
}

/* ==========================================================================
   Öffentliche API
   ========================================================================== */

/**
 * @brief Führt einen einzelnen Simulationsschritt aus und aktualisiert die GUI.
 *
 * Erzeugt Ereignisse für den nächsten Tick, lässt den Scheduler sie
 * verarbeiten und aktualisiert alle GUI-Elemente (Snapshot, Zähler, Tick).
 *
 * @param user_data  Nicht verwendet (GTK-Konvention für @c GSourceFunc).
 * @return           @c G_SOURCE_CONTINUE, damit der GTK-Timer weiterläuft.
 */
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

/**
 * @brief Gibt den aktuellen Verschiebungs-Zähler der aktiven Policy zurück.
 *
 * @return Anzahl verschobener Anfragen; 0, wenn keine Policy aktiv.
 */
uint64_t gui_get_deadlock_counter(void)
{
    if (g_policy && g_policy->private)
        return *((uint64_t *)g_policy->private);
    return 0;
}

/**
 * @brief Wechselt die aktive Policy und setzt die Simulation zurück.
 *
 * Stoppt den Timer, gibt alle bestehenden Ressourcen frei und startet
 * die Simulation mit der neuen Policy und einem frischen Systemzustand neu.
 *
 * @param new_policy  Zeiger auf die neu zu aktivierende Policy.
 */
void gui_set_policy(Policy *new_policy)
{
    /* 1. Periodischen Timer stoppen */
    if (g_timeout_id != 0) {
        g_source_remove(g_timeout_id);
        g_timeout_id = 0;
    }

    /* 2. Alten Scheduler, Systemzustand und Prozess-Simulator freigeben */
    if (g_sched) {
        SystemState *old_st = scheduler_state(g_sched);
        scheduler_destroy(g_sched);
        state_destroy(old_st);
        g_sched = NULL;
    }
    procsim_destroy();
    g_policy = NULL;

    /* 3. Neue Policy übernehmen */
    g_policy = new_policy;

    /* 4. Neuen Demo-Systemzustand erstellen */
    SystemState *st = build_demo_state();

    /* 5. Neuen Scheduler mit der gewählten Policy erstellen */
    g_sched = scheduler_create(st, g_policy);

    /* 6. Prozess-Simulator mit dem neuen Zustand initialisieren */
    procsim_init(st);

    /* 7. GUI-Elemente aktualisieren */
    gui_update_snapshot();
    gui_update_deadlock_label();

    /* 8. Automatischen Timer neu starten (1 Sekunde Intervall) */
    g_timeout_id = g_timeout_add_seconds(1, (GSourceFunc)gui_step, NULL);
}

/* ==========================================================================
   Button-Callbacks
   ========================================================================== */

/**
 * @brief Callback für den "GRAPH-ERKENNUNG"-Button.
 *
 * Hebt den Button optisch hervor und aktiviert die Graph-Erkennungs-Policy.
 *
 * @param button     Nicht verwendet.
 * @param user_data  Nicht verwendet.
 */
static void on_run_graph_detect(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_graph, "GRAPH-ERKENNUNG");
    Policy *pol = detect_policy_create(3, 2);
    gui_set_policy(pol);
}

/**
 * @brief Callback für den "BANKIER-ALGO"-Button.
 *
 * Hebt den Button optisch hervor und aktiviert die Bankier-Policy.
 *
 * @param button     Nicht verwendet.
 * @param user_data  Nicht verwendet.
 */
static void on_run_banker(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_banker, "BANKIER-ALGO");
    Policy *pol = banker_policy_create(3, 2);
    gui_set_policy(pol);
}

/**
 * @brief Callback für den "HALTE-UND-WARTE"-Button.
 *
 * Hebt den Button optisch hervor und aktiviert die Hold-and-Wait-Policy.
 *
 * @param button     Nicht verwendet.
 * @param user_data  Nicht verwendet.
 */
static void on_run_holdwait(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_hw, "HALTE-UND-WARTE");
    Policy *pol = holdwait_policy_create(3, 2);
    gui_set_policy(pol);
}

/**
 * @brief Callback für den "STEP"-Button (manueller Einzelschritt).
 *
 * @param button     Nicht verwendet.
 * @param user_data  Wird unverändert an @c gui_step() weitergegeben.
 */
static void on_step_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gui_step(user_data);
}

/* ==========================================================================
   CSS-Theme
   ========================================================================== */

/**
 * @brief CSS-Zeichenkette für das dunkle Terminal-Design der GUI.
 *
 * Definiert Farben, Schriftarten und Abstände für alle GTK-Widgets.
 * Das Design verwendet eine dunkle Farbpalette mit Cyan-Akzenten
 * und einer Monospace-Schriftart für den Terminal-Look.
 */
static const char *GUI_CSS =
    "window { background-color: #0d0f14; }"

    "#header { background-color: #0d0f14; border-bottom: 2px solid #00e5ff; padding: 16px 24px; }"
    "#title-label { font-family: 'Courier New', monospace; font-size: 22px; font-weight: bold; color: #00e5ff; letter-spacing: 4px; }"
    "#subtitle-label { font-family: 'Courier New', monospace; font-size: 12px; color: #546e7a; letter-spacing: 2px; }"

    "#btn-bar { background-color: #111318; padding: 14px 24px; border-bottom: 1px solid #1e2530; }"
    ".policy-btn { font-family: 'Courier New', monospace; font-size: 15px; font-weight: bold; letter-spacing: 1px; color: #FF0000; background-color: #1a1f2b; border: 2px solid #2a3240; border-radius: 4px; padding: 10px 20px; min-width: 200px; }"
    ".policy-btn:hover { background-color: #1e2840; border-color: #00e5ff; color: #ffffff; }"
    ".policy-btn.active { background-color: #003d4d; border-color: #00e5ff; color: #00e5ff; }"

    "#btn-step { font-family: 'Courier New', monospace; font-size: 15px; font-weight: bold; color: #ffd54f; background-color: #1a1a00; border: 2px solid #ffd54f; border-radius: 4px; padding: 10px 24px; }"
    "#btn-step:hover { background-color: #332b00; color: #fff176; border-color: #fff176; }"

    "#status-bar { background-color: #111318; border-bottom: 1px solid #1e2530; padding: 10px 24px; }"
    "#deadlock-label { font-family: 'Courier New', monospace; font-size: 16px; color: #ff5252; font-weight: bold; }"
    "#policy-name-label { font-family: 'Courier New', monospace; font-size: 14px; color: #00e5ff; font-weight: bold; }"
    "#tick-label { font-family: 'Courier New', monospace; font-size: 14px; color: #546e7a; }"

    "textview { font-family: 'Courier New', monospace; font-size: 18px; color: #e0f7fa; background-color: #0a0c10; padding: 20px; }"
    "textview text { background-color: #0a0c10; color: #e0f7fa; }"
    "scrolledwindow { background-color: #0a0c10; border: 1px solid #1e2530; margin: 12px 16px 16px 16px; }"

    "#legend-box { background-color: #111318; border-left: 2px solid #1e2530; padding: 20px 16px; min-width: 220px; }"
    "#legend-title { font-family: 'Courier New', monospace; font-size: 11px; color: #546e7a; letter-spacing: 2px; margin-bottom: 12px; }"
    ".legend-row { font-family: 'Courier New', monospace; font-size: 14px; color: #b0bec5; margin-bottom: 8px; }"
    ".legend-key { font-family: 'Courier New', monospace; font-size: 13px; color: #00e5ff; font-weight: bold; }"
    "#idle-indicator { font-family: 'Courier New', monospace; font-size: 13px; color: #546e7a; font-style: italic; margin-top: 24px; }";

/* ==========================================================================
   Hilfsfunktion: Policy-Button hervorheben
   ========================================================================== */

/**
 * @brief Hebt den aktiven Policy-Button hervor und setzt den Policy-Namen.
 *
 * Entfernt die CSS-Klasse "active" von allen Policy-Buttons und fügt sie
 * nur dem aktuell gewählten Button hinzu. Aktualisiert außerdem das
 * Policy-Namen-Label in der Statusleiste.
 *
 * @param active_btn  Zeiger auf den aktuell aktiven Button.
 * @param name        Anzuzeigender Policy-Name (z. B. "GRAPH-ERKENNUNG").
 */
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
        snprintf(buf, sizeof(buf), "STRATEGIE  %s", name);
        gtk_label_set_text(g_policy_name_label, buf);
    }
}

/* ==========================================================================
   Fenster-Erstellung
   ========================================================================== */

/**
 * @brief Erstellt das vollständige Hauptfenster der Anwendung.
 *
 * Aufbau des Fensters (von oben nach unten):
 * 1. **Header**: Titel "DEADLOCK SIMULATOR" und Untertitel "BETRIEB SYSTEME".
 * 2. **Button-Leiste**: Drei Policy-Buttons (links) und Step-Button (rechts).
 * 3. **Statusleiste**: Policy-Name · Verschiebungs-Zähler · Tick-Counter.
 * 4. **Hauptbereich**: TextView mit Systemzustand (links) | Legende (rechts).
 *
 * Nach dem Erstellen wird ein GTK-Timer gestartet, der @c gui_step()
 * einmal pro Sekunde aufruft, falls bereits eine Policy aktiv ist.
 *
 * @return Zeiger auf das erstellte Top-Level-GtkWidget.
 */
GtkWidget *gui_create_window(void)
{
    srand((unsigned)time(NULL));

    /* CSS-Theme laden und für den gesamten Bildschirm aktivieren */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, GUI_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* Hauptfenster */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Deadlock-Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 720);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), root_vbox);

    /* ── Header ── */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(header, "header");
    gtk_box_pack_start(GTK_BOX(root_vbox), header, FALSE, FALSE, 0);

    GtkWidget *title_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(header), title_vbox, TRUE, TRUE, 0);

    GtkWidget *title_lbl = gtk_label_new("DEADLOCK-SIMULATOR");
    gtk_widget_set_name(title_lbl, "title-label");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(title_vbox), title_lbl, FALSE, FALSE, 0);

    GtkWidget *sub_lbl = gtk_label_new("BETRIEBSSYSTEME");
    gtk_widget_set_name(sub_lbl, "subtitle-label");
    gtk_label_set_xalign(GTK_LABEL(sub_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(title_vbox), sub_lbl, FALSE, FALSE, 0);

    /* ── Button-Leiste ── */
    GtkWidget *btn_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(btn_bar, "btn-bar");
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_bar, FALSE, FALSE, 0);

    g_btn_graph  = gtk_button_new_with_label("⬡  GRAPH-ERKENNUNG");
    g_btn_banker = gtk_button_new_with_label("⬡  BANKIER-ALGO");
    g_btn_hw     = gtk_button_new_with_label("⬡  HALTE-UND-WARTE");
    GtkWidget *btn_step = gtk_button_new_with_label("▶  SCHRITT");

    GtkWidget *policy_btns[3] = { g_btn_graph, g_btn_banker, g_btn_hw };
    for (int i = 0; i < 3; ++i) {
        gtk_style_context_add_class(
            gtk_widget_get_style_context(policy_btns[i]), "policy-btn");
        gtk_box_pack_start(GTK_BOX(btn_bar), policy_btns[i], FALSE, FALSE, 0);
    }
    gtk_widget_set_name(btn_step, "btn-step");
    /* Step-Button rechtsbündig durch Spacer */
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(btn_bar), spacer, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_bar), btn_step, FALSE, FALSE, 0);

    g_signal_connect(g_btn_graph,  "clicked", G_CALLBACK(on_run_graph_detect), NULL);
    g_signal_connect(g_btn_banker, "clicked", G_CALLBACK(on_run_banker),       NULL);
    g_signal_connect(g_btn_hw,     "clicked", G_CALLBACK(on_run_holdwait),     NULL);
    g_signal_connect(btn_step,     "clicked", G_CALLBACK(on_step_clicked),     NULL);

    /* ── Statusleiste ── */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_name(status_bar, "status-bar");
    gtk_box_pack_start(GTK_BOX(root_vbox), status_bar, FALSE, FALSE, 0);

    g_policy_name_label = GTK_LABEL(gtk_label_new("STRATEGIE  —"));
    gtk_widget_set_name(GTK_WIDGET(g_policy_name_label), "policy-name-label");
    gtk_label_set_xalign(GTK_LABEL(g_policy_name_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_policy_name_label), FALSE, FALSE, 0);

    GtkWidget *sep_lbl = gtk_label_new("·");
    gtk_widget_set_name(sep_lbl, "tick-label");
    gtk_box_pack_start(GTK_BOX(status_bar), sep_lbl, FALSE, FALSE, 0);

    g_deadlock_label = GTK_LABEL(gtk_label_new("VERSCHOBEN  0"));
    gtk_widget_set_name(GTK_WIDGET(g_deadlock_label), "deadlock-label");
    gtk_label_set_xalign(GTK_LABEL(g_deadlock_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_deadlock_label), FALSE, FALSE, 0);

    GtkWidget *sep_lbl2 = gtk_label_new("·");
    gtk_widget_set_name(sep_lbl2, "tick-label");
    gtk_box_pack_start(GTK_BOX(status_bar), sep_lbl2, FALSE, FALSE, 0);

    g_tick_label = GTK_LABEL(gtk_label_new("TAKT  0"));
    gtk_widget_set_name(GTK_WIDGET(g_tick_label), "tick-label");
    gtk_label_set_xalign(GTK_LABEL(g_tick_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_tick_label), FALSE, FALSE, 0);

    /* ── Hauptbereich: Snapshot links, Legende rechts ── */
    GtkWidget *content_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), content_hbox, TRUE, TRUE, 0);

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

    /* ── Legende ── */
    GtkWidget *legend_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(legend_box, "legend-box");
    gtk_box_pack_start(GTK_BOX(content_hbox), legend_box, FALSE, FALSE, 0);

    GtkWidget *legend_title = gtk_label_new("LEGENDE");
    gtk_widget_set_name(legend_title, "legend-title");
    gtk_label_set_xalign(GTK_LABEL(legend_title), 0.0);
    gtk_box_pack_start(GTK_BOX(legend_box), legend_title, FALSE, FALSE, 0);

    const char *legend_entries[][2] = {
        { "zugeteilt",   "gehaltene Ressourcen"   },
        { "bedarf",      "noch benötigt"          },
        { "verfügbar",   "freie Instanzen"        },
        { "VERSCHOBEN",  "blockierte Anfragen"    },
        { "TAKT",        "Simulationsuhr"         },
    };
    for (int i = 0; i < 5; ++i) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_name(row, "legend-row");

        GtkWidget *key_lbl = gtk_label_new(legend_entries[i][0]);
        gtk_widget_set_name(key_lbl, "legend-key");
        gtk_label_set_xalign(GTK_LABEL(key_lbl), 0.0);

        GtkWidget *val_lbl = gtk_label_new(legend_entries[i][1]);
        gtk_widget_set_name(val_lbl, "legend-row");
        gtk_label_set_xalign(GTK_LABEL(val_lbl), 0.0);

        gtk_box_pack_start(GTK_BOX(row), key_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), val_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(legend_box), row, FALSE, FALSE, 8);
    }

    GtkWidget *idle_lbl = gtk_label_new("Wähle eine Strategie\nzum Starten.");
    gtk_widget_set_name(idle_lbl, "idle-indicator");
    gtk_label_set_xalign(GTK_LABEL(idle_lbl), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(idle_lbl), TRUE);
    gtk_box_pack_end(GTK_BOX(legend_box), idle_lbl, FALSE, FALSE, 0);

    /* Automatischen Timer starten */
    g_timeout_id = g_timeout_add_seconds(1, (GSourceFunc)gui_step, NULL);

    gtk_widget_show_all(window);
    return window;
}
