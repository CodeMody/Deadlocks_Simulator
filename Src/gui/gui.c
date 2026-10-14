/**
 * @file gui.c
 * @brief Implementierung des GTK-3-Frontends für den Deadlock-Simulator.
 *
 * Diese Datei implementiert die gesamte grafische Benutzeroberfläche.
 * Das Fenster besteht aus folgenden Bereichen:
 * - **Toolbar**: Drei Policy-Buttons und ein Step-Button.
 * - **Statusleiste**: Aktive Policy, Verschiebungs-Zähler, aktueller Tick.
 * - **Hauptbereich**: Simulation-State-Anzeige (links) und Legende (rechts).
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
#define _GTK_H_INSIDE
#include <gtk/gtk.h>
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

/** @brief Referenz auf den "GRAPH DETECT"-Button (für Hervorhebung). */
static GtkWidget   *g_btn_graph         = NULL;

/** @brief Referenz auf den "BANKER'S ALGO"-Button (für Hervorhebung). */
static GtkWidget   *g_btn_banker        = NULL;

/** @brief Referenz auf den "HOLD & WAIT"-Button (für Hervorhebung). */
static GtkWidget   *g_btn_hw            = NULL;

/** @brief GTK-Timer-ID für den automatischen Simulationsschritt (1 Sekunde). 0 = kein aktiver Timer. */
static guint        g_timeout_id        = 0;

/** @brief Referenz auf die Legende-Swatch-Labels für Farbkorrektur. */
static GtkWidget   *g_swatch[8]         = { NULL };

/** @brief GTK-Text-Tags für Syntax-Highlighting im Snapshot. */
static GtkTextTag  *g_tag_gray          = NULL;
static GtkTextTag  *g_tag_white         = NULL;
static GtkTextTag  *g_tag_purple        = NULL;
static GtkTextTag  *g_tag_green         = NULL;
static GtkTextTag  *g_tag_red           = NULL;

/* ==========================================================================
   Vorwärts-Deklarationen
   ========================================================================== */

static void gui_update_tick_label(void);
static void gui_highlight_policy_btn(GtkWidget *active_btn, const char *name);
static void gui_update_legend_swatches(void);
static void tag_snapshot(GtkTextBuffer *buf);

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
    state_allocate_process_matrices(st, 3);    /* 3 simulierte Prozesse */

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
 * @brief Erstellt einen formatierten Snapshot mit Syntax-Highlighting.
 *
 * Verwendet Pango-Markup, um die Anzeige wie im Mockup zu gestalten:
 * - Gray (#64748b): Labels ("Available per class", "P0", "alloc:", "need:")
 * - White (#e2e8f0): Values ("1  2", "1  0")
 * - Purple (#818cf8): Highlighted values (available resources)
 * - Green (#34d399): Safety check result
 *
 * @return Dynamisch allozierter Pango-Markup-String.
 */
static char *make_snapshot_text(void)
{
    const SystemState *st = scheduler_state(g_sched);
    uint64_t now          = scheduler_current_time(g_sched);
    const uint32_t n_procs   = st->n_procs;
    const uint32_t n_classes = st->n_classes;
    const uint32_t n_classes2 = n_classes * 2 + 2;  /* space + number + space per class */

    /* Puffergröße: Header + available + per-process + safety */
    size_t bufsize = 128
                   + n_classes2 * 20
                   + n_procs * (80 + n_classes2 * 2)
                   + 64;
    char *buf = malloc(bufsize);
    char *p   = buf;
    int   n;

    /* Header */
    n = snprintf(p, bufsize, "\n=== Tick %lu ====================================\n", now);
    p += n; bufsize -= n;

    /* Available per class */
    n = snprintf(p, bufsize, "Available per class : ");
    p += n; bufsize -= n;
    for (uint32_t r = 0; r < n_classes; ++r) {
        n = snprintf(p, bufsize, "%-5u", st->available[r]);
        p += n; bufsize -= n;
    }
    n = snprintf(p, bufsize, "\n");
    p += n; bufsize -= n;

    /* Per-process allocation and need */
    for (uint32_t pid = 0; pid < n_procs; ++pid) {
        n = snprintf(p, bufsize, "P%u  ", pid);
        p += n; bufsize -= n;

        n = snprintf(p, bufsize, "alloc:");
        p += n; bufsize -= n;
        for (uint32_t r = 0; r < n_classes; ++r) {
            n = snprintf(p, bufsize, "%-5u", st->allocation[pid][r]);
            p += n; bufsize -= n;
        }

        n = snprintf(p, bufsize, "  need:");
        p += n; bufsize -= n;
        for (uint32_t r = 0; r < n_classes; ++r) {
            n = snprintf(p, bufsize, "%-5u", st->request[pid][r]);
            p += n; bufsize -= n;
        }
        n = snprintf(p, bufsize, "\n");
        p += n; bufsize -= n;
    }

    /* Safety check */
    bool safe = state_is_safe(st);
    if (safe) {
        n = snprintf(p, bufsize, "\n  ✓ SAFE\n");
        p += n; bufsize -= n;
    } else {
        n = snprintf(p, bufsize, "\n  ✗ UNSAFE\n");
        p += n; bufsize -= n;
    }

    return buf;
}

/**
 * @brief Legt Syntax-Highlighting-Tags auf den Text-Snapshot.
 *
 * Parst die formatierte Ausgabe und markiert Labels (grau), Werte (weiß),
 * Highlights (lila) und Sicherheitsergebnis (grün/rot).
 */
static void tag_snapshot(GtkTextBuffer *buf)
{
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buf, &start);
    gtk_text_buffer_get_end_iter(buf, &end);

    gchar *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    gchar *line = strtok(text, "\n");
    int offset = 0;

    while (line) {
        if (strstr(line, "Available per class")) {
            /* Werte nach ": " als lila markieren */
            char *p = strstr(line, ": ");
            if (p) {
                p += 2;
                while (*p) {
                    if (*p >= '0' && *p <= '9') {
                        int len = 0;
                        while (p[len] >= '0' && p[len] <= '9') len++;
                        GtkTextIter it1, it2;
                        gtk_text_buffer_get_iter_at_offset(buf, &it1, offset + (int)(p - line));
                        gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + (int)(p - line) + len);
                        gtk_text_buffer_apply_tag(buf, g_tag_purple, &it1, &it2);
                        p += len;
                    }
                    if (*p == ' ') p++; else break;
                }
            }
        } else if (line[0] == 'P' && line[1] >= '0' && line[1] <= '9') {
            /* Prozesszeile: P-ID grau, alloc-Werte weiß, need-Werte lila */
            int pid_len = 1;
            while (line[pid_len] >= '0' && line[pid_len] <= '9') pid_len++;
            {
                GtkTextIter it1, it2;
                gtk_text_buffer_get_iter_at_offset(buf, &it1, offset);
                gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + pid_len);
                gtk_text_buffer_apply_tag(buf, g_tag_gray, &it1, &it2);
            }

            char *alloc = strstr(line, "alloc:");
            if (alloc) {
                GtkTextIter it1, it2;
                gtk_text_buffer_get_iter_at_offset(buf, &it1, offset + (int)(alloc - line));
                gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + (int)(alloc - line) + 6);
                gtk_text_buffer_apply_tag(buf, g_tag_gray, &it1, &it2);

                char *v = alloc + 6;
                while (*v == ' ') v++;
                while (*v && *v != '\r' && *v != '\n') {
                    int num_len = 0;
                    for (; v[num_len] >= '0' && v[num_len] <= '9'; num_len++);
                    if (num_len > 0) {
                        GtkTextIter it1, it2;
                        gtk_text_buffer_get_iter_at_offset(buf, &it1, offset + (int)(v - line));
                        gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + (int)(v - line) + num_len);
                        gtk_text_buffer_apply_tag(buf, g_tag_white, &it1, &it2);
                        v += num_len;
                        while (*v == ' ') v++;
                    } else { v++; }
                }
            }

            char *need = strstr(line, "need:");
            if (need) {
                GtkTextIter it1, it2;
                gtk_text_buffer_get_iter_at_offset(buf, &it1, offset + (int)(need - line));
                gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + (int)(need - line) + 5);
                gtk_text_buffer_apply_tag(buf, g_tag_gray, &it1, &it2);

                char *v = need + 5;
                while (*v == ' ') v++;
                while (*v && *v != '\r' && *v != '\n') {
                    int num_len = 0;
                    for (; v[num_len] >= '0' && v[num_len] <= '9'; num_len++);
                    if (num_len > 0) {
                        GtkTextIter it1, it2;
                        gtk_text_buffer_get_iter_at_offset(buf, &it1, offset + (int)(v - line));
                        gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + (int)(v - line) + num_len);
                        gtk_text_buffer_apply_tag(buf, g_tag_purple, &it1, &it2);
                        v += num_len;
                        while (*v == ' ') v++;
                    } else { v++; }
                }
            }
        } else if (strstr(line, "✓ SAFE")) {
            int pos = (int)(strstr(line, "✓ SAFE") - line);
            GtkTextIter it1, it2;
            gtk_text_buffer_get_iter_at_offset(buf, &it1, offset + pos);
            gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + pos + 6);
            gtk_text_buffer_apply_tag(buf, g_tag_green, &it1, &it2);
        } else if (strstr(line, "✗ UNSAFE")) {
            int pos = (int)(strstr(line, "✗ UNSAFE") - line);
            GtkTextIter it1, it2;
            gtk_text_buffer_get_iter_at_offset(buf, &it1, offset + pos);
            gtk_text_buffer_get_iter_at_offset(buf, &it2, offset + pos + 8);
            gtk_text_buffer_apply_tag(buf, g_tag_red, &it1, &it2);
        }

        offset += (int)strlen(line) + 1;
        line = strtok(NULL, "\n");
    }
    g_free(text);
}

/**
 * @brief Aktualisiert das Systemzustand-Snapshot im TextView.
 */
static void gui_update_snapshot(void)
{
    char *txt = make_snapshot_text();

    GtkTextBuffer *buf = gtk_text_view_get_buffer(g_text_view);
    gtk_text_buffer_set_text(buf, txt, -1);
    tag_snapshot(buf);
    free(txt);
}

/**
 * @brief Aktualisiert das Label mit dem aktuellen Verschiebungs-Zähler.
 *
 * Liest den Zähler aus dem ersten @c uint64_t-Feld des privaten Policy-Kontexts.
 */
static void gui_update_deadlock_label(void)
{
    uint64_t cnt = policy_get_deadlock_count(g_policy);
    char label[64];
    snprintf(label, sizeof(label), "POSTPONED  %lu", cnt);
    gtk_label_set_text(g_deadlock_label, label);
}

/**
 * @brief Aktualisiert das Tick-Label in der Statusleiste.
 */
static void gui_update_tick_label(void)
{
    if (!g_tick_label || !g_sched) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "TICK  %lu", scheduler_current_time(g_sched));
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
    return policy_get_deadlock_count(g_policy);
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
    gui_update_tick_label();

    /* 8. Automatischen Timer neu starten (1 Sekunde Intervall) */
    g_timeout_id = g_timeout_add_seconds(1, (GSourceFunc)gui_step, NULL);
}

/* ==========================================================================
   Button-Callbacks
   ========================================================================== */

/**
 * @brief Draw-Callback für die Legende-Swatch-Farbkästchen.
 */
static gboolean on_swatch_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    (void)user_data;
    const gchar *color_str = g_object_get_data(G_OBJECT(widget), "swatch-color");
    if (!color_str) return FALSE;

    GdkRGBA rgba;
    gdk_rgba_parse(&rgba, color_str);
    cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, rgba.alpha);
    cairo_rectangle(cr, 0, 0, gtk_widget_get_allocated_width(widget),
                    gtk_widget_get_allocated_height(widget));
    cairo_fill(cr);
    return TRUE;
}

/**
 * @brief Callback für den "GRAPH DETECT"-Button.
 *
 * Hebt den Button optisch hervor und aktiviert die Graph-Erkennungs-Policy.
 *
 * @param button     Nicht verwendet.
 * @param user_data  Nicht verwendet.
 */
static void on_run_graph_detect(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_graph, "Graph Detect");
    Policy *pol = detect_policy_create(3, 2);
    gui_set_policy(pol);
}

/**
 * @brief Callback für den "BANKER'S ALGO"-Button.
 *
 * Hebt den Button optisch hervor und aktiviert die Bankier-Policy.
 *
 * @param button     Nicht verwendet.
 * @param user_data  Nicht verwendet.
 */
static void on_run_banker(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_banker, "Banker's Algo");
    Policy *pol = banker_policy_create(3, 2);
    gui_set_policy(pol);
}

/**
 * @brief Callback für den "HOLD & WAIT"-Button.
 *
 * Hebt den Button optisch hervor und aktiviert die Hold-and-Wait-Policy.
 *
 * @param button     Nicht verwendet.
 * @param user_data  Nicht verwendet.
 */
static void on_run_holdwait(GtkButton *button, gpointer user_data)
{
    (void)button; (void)user_data;
    gui_highlight_policy_btn(g_btn_hw, "Hold & Wait");
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
   CSS-Theme — Modern Dark (exakt nach Mockup)
   ========================================================================== */

/**
 * @brief CSS-Zeichenkette für das moderne dunkle Design der GUI.
 *
 * Exakt nach Mockup: Inter-Schriftart, dunkle Panels (#1a1d27),
 * lila Gradient-Buttons für die aktive Policy, abgerundete Ecken,
 * dezente Trennlinien, Process-State-Badges.
 */
static const char *GUI_CSS =
    "window { background-color: #1a1d27; }"

    "#toolbar { background-color: #161822; padding: 12px 20px; border-bottom: 1px solid #2a2d3a; }"
    ".policy-btn { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 13px; font-weight: 500; color: #c4c9d4; background-color: #1e2130; border: 1px solid #3b3f52; border-radius: 8px; padding: 8px 16px; min-width: 120px; }"
    ".policy-btn:hover { background-color: #25293a; border-color: #4a4f6a; }"
    ".policy-btn.active { background: linear-gradient(135deg, #6366f1, #8b5cf6); border-color: transparent; color: #fff; box-shadow: 0 4px 15px rgba(99, 102, 241, 0.3); }"

    "#btn-step { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 13px; font-weight: 600; color: #fbbf24; background-color: #25293a; border: 1px solid #4a4f6a; border-radius: 8px; padding: 8px 20px; }"
    "#btn-step:hover { background-color: #2d3148; border-color: #fbbf24; }"

    "#status-bar { background-color: #13151e; padding: 8px 20px; border-bottom: 1px solid #2a2d3a; }"
    "#deadlock-label { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 12px; color: #f87171; font-weight: 600; }"
    "#policy-name-label { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 12px; color: #818cf8; font-weight: 600; }"
    "#tick-label { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 12px; color: #64748b; }"
    "#status-sep { background-color: #2a2d3a; }"

    "#sim-area { background-color: #161822; padding: 16px 20px; }"
    "#sim-title { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 11px; font-weight: 600; letter-spacing: 1px; color: #94a3b8; }"
    "#sim-tick { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 12px; color: #64748b; background-color: #1e2130; padding: 3px 10px; border-radius: 20px; border: 1px solid #2a2d3a; }"

    "textview { font-family: 'JetBrains Mono', 'Fira Code', 'Courier New', monospace; font-size: 25px; color: #cbd5e1; background-color: #0f1117; padding: 12px; border-radius: 8px; border: 1px solid #2a2d3a; }"
    "textview text { background-color: #0f1117; color: #e2e8f0; }"
    "scrolledwindow { background-color: #161822; border: 1px solid #2a2d3a; border-radius: 8px; margin: 12px 0 0 0; }"

    "#legend-box { background-color: #161822; padding: 16px 20px; border-left: 1px solid #2a2d3a; min-width: 240px; }"
    "#legend-title { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 11px; font-weight: 600; letter-spacing: 1px; color: #64748b; margin-bottom: 12px; }"
    ".legend-row { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 12px; color: #94a3b8; margin-bottom: 8px; }"
    ".legend-key { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 12px; color: #e2e8f0; font-weight: 600; }"
    "#idle-indicator { font-family: 'Inter', 'Segoe UI', sans-serif; font-size: 12px; color: #475569; font-style: italic; margin-top: 20px; }"
    ".legend-swatch { background-color: #475569; border-radius: 4px; }";

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
 * @param name        Anzuzeigender Policy-Name (z. B. "Graph Detect").
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
        snprintf(buf, sizeof(buf), "STRATEGY  %s", name);
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
 * 1. **Toolbar**: Drei Policy-Buttons (links) und Step-Button (rechts).
 * 2. **Statusleiste**: Policy-Name · Verschiebungs-Zähler · Tick-Counter.
 * 3. **Hauptbereich**: Simulation-State-Anzeige (links) | Legende (rechts).
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
    gtk_window_set_title(GTK_WINDOW(window), "Deadlock Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 720);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), root_vbox);

    /* ── Toolbar ── */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(toolbar, "toolbar");
    gtk_box_pack_start(GTK_BOX(root_vbox), toolbar, FALSE, FALSE, 0);

    g_btn_graph  = gtk_button_new_with_label("Graph Detect");
    g_btn_banker = gtk_button_new_with_label("Banker's Algo");
    g_btn_hw     = gtk_button_new_with_label("Hold & Wait");
    GtkWidget *btn_step = gtk_button_new_with_label("Step");

    GtkWidget *policy_btns[3] = { g_btn_graph, g_btn_banker, g_btn_hw };
    for (int i = 0; i < 3; ++i) {
        gtk_style_context_add_class(
            gtk_widget_get_style_context(policy_btns[i]), "policy-btn");
        gtk_box_pack_start(GTK_BOX(toolbar), policy_btns[i], FALSE, FALSE, 0);
    }

    /* Step-Button rechtsbündig durch Spacer */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(toolbar), spacer, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), btn_step, FALSE, FALSE, 0);

    g_signal_connect(g_btn_graph,  "clicked", G_CALLBACK(on_run_graph_detect), NULL);
    g_signal_connect(g_btn_banker, "clicked", G_CALLBACK(on_run_banker),       NULL);
    g_signal_connect(g_btn_hw,     "clicked", G_CALLBACK(on_run_holdwait),     NULL);
    g_signal_connect(btn_step,     "clicked", G_CALLBACK(on_step_clicked),     NULL);

    /* ── Statusleiste ── */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_name(status_bar, "status-bar");
    gtk_box_pack_start(GTK_BOX(root_vbox), status_bar, FALSE, FALSE, 0);

    g_policy_name_label = GTK_LABEL(gtk_label_new("STRATEGY  —"));
    gtk_widget_set_name(GTK_WIDGET(g_policy_name_label), "policy-name-label");
    gtk_label_set_xalign(GTK_LABEL(g_policy_name_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_policy_name_label), FALSE, FALSE, 0);

    GtkWidget *sep_lbl = gtk_label_new("");
    gtk_widget_set_name(sep_lbl, "status-sep");
    gtk_box_pack_start(GTK_BOX(status_bar), sep_lbl, FALSE, FALSE, 0);

    g_deadlock_label = GTK_LABEL(gtk_label_new("POSTPONED  0"));
    gtk_widget_set_name(GTK_WIDGET(g_deadlock_label), "deadlock-label");
    gtk_label_set_xalign(GTK_LABEL(g_deadlock_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_deadlock_label), FALSE, FALSE, 0);

    GtkWidget *sep_lbl2 = gtk_label_new("");
    gtk_widget_set_name(sep_lbl2, "status-sep");
    gtk_box_pack_start(GTK_BOX(status_bar), sep_lbl2, FALSE, FALSE, 0);

    g_tick_label = GTK_LABEL(gtk_label_new("TICK  0"));
    gtk_widget_set_name(GTK_WIDGET(g_tick_label), "tick-label");
    gtk_label_set_xalign(GTK_LABEL(g_tick_label), 0.0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(g_tick_label), FALSE, FALSE, 0);

    /* ── Hauptbereich: Simulation links, Legende rechts ── */
    GtkWidget *content_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), content_hbox, TRUE, TRUE, 0);

    /* Simulation area */
    GtkWidget *sim_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_name(sim_area, "sim-area");
    gtk_box_pack_start(GTK_BOX(content_hbox), sim_area, TRUE, TRUE, 0);

    GtkWidget *sim_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *sim_title_lbl = gtk_label_new("SIMULATION STATE");
    gtk_widget_set_name(sim_title_lbl, "sim-title");
    gtk_box_pack_start(GTK_BOX(sim_header), sim_title_lbl, FALSE, FALSE, 0);

    GtkWidget *sim_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sim_spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(sim_header), sim_spacer, TRUE, TRUE, 0);

    GtkWidget *sim_tick_lbl = gtk_label_new("Tick 0");
    gtk_widget_set_name(sim_tick_lbl, "sim-tick");
    gtk_box_pack_start(GTK_BOX(sim_header), sim_tick_lbl, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(sim_area), sim_header, FALSE, FALSE, 0);

    /* TextView mit Scrollbar */
    g_text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(g_text_view, FALSE);
    gtk_text_view_set_cursor_visible(g_text_view, FALSE);
    gtk_text_view_set_left_margin(g_text_view, 12);
    gtk_text_view_set_top_margin(g_text_view, 12);

    /* Syntax-Highlighting-Tags erstellen */
    GtkTextBuffer *buf = gtk_text_view_get_buffer(g_text_view);
    g_tag_gray  = gtk_text_buffer_create_tag(buf, "gray",  "foreground", "#64748b", NULL);
    g_tag_white = gtk_text_buffer_create_tag(buf, "white", "foreground", "#e2e8f0", NULL);
    g_tag_purple= gtk_text_buffer_create_tag(buf, "purple","foreground", "#818cf8", NULL);
    g_tag_green = gtk_text_buffer_create_tag(buf, "green", "foreground", "#34d399", NULL);
    g_tag_red   = gtk_text_buffer_create_tag(buf, "red",   "foreground", "#f87171", NULL);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(g_text_view));
    gtk_box_pack_start(GTK_BOX(sim_area), scrolled, TRUE, TRUE, 0);

    /* Legende */
    GtkWidget *legend_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(legend_box, "legend-box");
    gtk_box_pack_start(GTK_BOX(content_hbox), legend_box, FALSE, FALSE, 0);

    GtkWidget *legend_title = gtk_label_new("LEGEND");
    gtk_widget_set_name(legend_title, "legend-title");
    gtk_label_set_xalign(GTK_LABEL(legend_title), 0.0);
    gtk_box_pack_start(GTK_BOX(legend_box), legend_title, FALSE, FALSE, 0);

    /* Farbige Swatches als kleine Boxen */
    const char *legend_entries[][2] = {
        { "Available",   "Free instances"       },
        { "Need",        "Still required"       },
        { "Safe",        "State is safe"        },
        { "Unsafe",      "Deadlock risk"        },
        { "Postponed",   "Blocked requests"     },
        { "IDLE",        "Waiting to start"     },
        { "RUNNING",     "Working with resources"},
        { "DONE",        "Finished, releasing"  },
    };
    const char *swatch_colors[8] = {
        "#22d3ee",   /* Available — cyan */
        "#818cf8",   /* Need — purple */
        "#34d399",   /* Safe — green */
        "#f87171",   /* Unsafe — red */
        "#fbbf24",   /* Postponed — amber */
        "#475569",   /* IDLE — gray */
        "#34d399",   /* RUNNING — green */
        "#fbbf24",   /* DONE — amber */
    };
    for (int i = 0; i < 8; ++i) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_name(row, "legend-row");

        /* Farbkästchen — GtkDrawingArea mit draw-Callback */
        GtkWidget *swatch = gtk_drawing_area_new();
        gtk_widget_set_size_request(swatch, 14, 14);
        g_swatch[i] = swatch;
        g_object_set_data(G_OBJECT(swatch), "swatch-color", (gpointer)swatch_colors[i]);
        g_signal_connect(swatch, "draw", G_CALLBACK(on_swatch_draw), NULL);
        gtk_box_pack_start(GTK_BOX(row), swatch, FALSE, FALSE, 0);

        GtkWidget *val_lbl = gtk_label_new(legend_entries[i][0]);
        gtk_widget_set_name(val_lbl, "legend-key");
        gtk_box_pack_start(GTK_BOX(row), val_lbl, FALSE, FALSE, 0);

        GtkWidget *key_lbl = gtk_label_new(legend_entries[i][1]);
        gtk_box_pack_start(GTK_BOX(row), key_lbl, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(legend_box), row, FALSE, FALSE, 6);
    }

    GtkWidget *idle_lbl = gtk_label_new("Click a policy\nbutton to start.");
    gtk_widget_set_name(idle_lbl, "idle-indicator");
    gtk_label_set_xalign(GTK_LABEL(idle_lbl), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(idle_lbl), TRUE);
    gtk_box_pack_end(GTK_BOX(legend_box), idle_lbl, FALSE, FALSE, 0);

    /* Automatischen Timer starten */
    g_timeout_id = g_timeout_add_seconds(1, (GSourceFunc)gui_step, NULL);

    gtk_widget_show_all(window);
    return window;
}
