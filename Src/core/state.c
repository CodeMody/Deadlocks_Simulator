/**
 * @file state.c
 * @brief Implementierung der Systemzustandsverwaltung.
 *
 * Dieses Modul verwaltet den zentralen @c SystemState, der das
 * Ressourcensystem nach dem Bankier-Modell abbildet. Es stellt
 * Funktionen zur Allokation, Manipulation und Analyse des Zustands
 * bereit. Der Kern des Moduls ist die Implementierung des
 * Bankier-Algorithmus (@c state_is_safe()), der prüft, ob eine
 * sichere Abarbeitungsreihenfolge aller Prozesse existiert.
 */

#include "../core/state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * @brief Allokiert eine zweidimensionale Matrix aus uint32_t, mit 0 initialisiert.
 *
 * @param rows  Anzahl der Zeilen.
 * @param cols  Anzahl der Spalten.
 * @return      Zeiger auf die neu allokierte Matrix.
 */
static uint32_t **alloc_matrix(uint32_t rows, uint32_t cols)
{
    uint32_t **m = calloc(rows, sizeof(uint32_t *));
    if (!m) { fprintf(stderr, "alloc_matrix: out of memory\n"); exit(EXIT_FAILURE); }
    for (uint32_t i = 0; i < rows; ++i) {
        m[i] = calloc(cols, sizeof(uint32_t));
        if (!m[i]) { fprintf(stderr, "alloc_matrix: out of memory\n"); exit(EXIT_FAILURE); }
    }
    return m;
}

/**
 * @brief Gibt eine zweidimensionale Matrix und ihre Zeilen frei.
 *
 * @param m     Zeiger auf die freizugebende Matrix. NULL wird sicher ignoriert.
 * @param rows  Anzahl der Zeilen (wird benötigt, um alle Zeilenzeiger freizugeben).
 */
static void free_matrix(uint32_t **m, uint32_t rows)
{
    if (!m) return;
    for (uint32_t i = 0; i < rows; ++i)
        free(m[i]);
    free(m);
}

/**
 * @brief Erstellt einen neuen Systemzustand.
 *
 * Initialisiert die Ressourcenvektoren @c instances und @c available mit
 * den übergebenen Werten. Die Prozess-Matrizen (@c allocation, @c request)
 * werden erst durch @c state_allocate_process_matrices() angelegt.
 *
 * @param n_classes  Anzahl der Ressourcenklassen.
 * @param instances  Array der Gesamtinstanzen je Klasse.
 * @return           Zeiger auf den neu erstellten @c SystemState.
 */
SystemState *state_create(uint32_t n_classes,
                          const uint32_t *instances)
{
    SystemState *st = calloc(1, sizeof(*st));
    if (!st) { fprintf(stderr, "state_create: out of memory\n"); exit(EXIT_FAILURE); }
    st->n_classes = n_classes;
    st->n_procs   = 0;

    st->instances = calloc(n_classes, sizeof(uint32_t));
    st->available = calloc(n_classes, sizeof(uint32_t));
    if (!st->instances || !st->available) {
        fprintf(stderr, "state_create: out of memory\n"); exit(EXIT_FAILURE);
    }

    /* Gesamtinstanzen kopieren; zu Beginn sind alle Ressourcen verfügbar. */
    memcpy(st->instances, instances, n_classes * sizeof(uint32_t));
    memcpy(st->available, instances, n_classes * sizeof(uint32_t));

    st->allocation = NULL;
    st->request    = NULL;
    return st;
}

/**
 * @brief Legt die Allokations- und Bedarfsmatrizen für die Prozesse an.
 *
 * Muss aufgerufen werden, sobald die Prozessanzahl bekannt ist und
 * bevor irgendwelche Matrizeneinträge gesetzt werden. Beide Matrizen
 * werden mit Nullen initialisiert.
 *
 * @param st       Zeiger auf den Systemzustand.
 * @param n_procs  Anzahl der simulierten Prozesse.
 */
void state_allocate_process_matrices(SystemState *st, uint32_t n_procs)
{
    st->n_procs    = n_procs;
    st->allocation = alloc_matrix(n_procs, st->n_classes);
    st->request    = alloc_matrix(n_procs, st->n_classes);
}

/**
 * @brief Gibt den gesamten Systemzustand frei.
 *
 * Gibt alle dynamisch allokierten Felder des @c SystemState frei
 * (Ressourcenvektoren sowie Prozess-Matrizen). NULL wird ignoriert.
 *
 * @param st  Zeiger auf den freizugebenden @c SystemState.
 */
void state_destroy(SystemState *st)
{
    if (!st) return;
    free(st->instances);
    free(st->available);
    free_matrix(st->allocation, st->n_procs);
    free_matrix(st->request,    st->n_procs);
    free(st);
}

/**
 * @brief Setzt einen Eintrag in der Allokationsmatrix C.
 *
 * @param st        Zeiger auf den Systemzustand.
 * @param pid       Prozess-ID (Zeilenindex).
 * @param class_id  Ressourcenklasse (Spaltenindex).
 * @param amount    Anzahl der als gehalten einzutragenden Instanzen.
 */
void state_set_allocation(SystemState *st,
                          uint32_t pid,
                          uint32_t class_id,
                          uint32_t amount)
{
    if (pid >= st->n_procs || class_id >= st->n_classes) return;
    st->allocation[pid][class_id] = amount;
}

/**
 * @brief Setzt einen Eintrag in der Bedarfsmatrix R.
 *
 * @param st        Zeiger auf den Systemzustand.
 * @param pid       Prozess-ID (Zeilenindex).
 * @param class_id  Ressourcenklasse (Spaltenindex).
 * @param amount    Anzahl der noch benötigten Instanzen.
 */
void state_set_request(SystemState *st,
                       uint32_t pid,
                       uint32_t class_id,
                       uint32_t amount)
{
    if (pid >= st->n_procs || class_id >= st->n_classes) return;
    st->request[pid][class_id] = amount;
}

/**
 * @brief Prüft, ob der Bedarf eines Prozesses vollständig durch den Arbeitsvektor gedeckt ist.
 *
 * Hilfsfunktion des Bankier-Algorithmus: Gibt @c true zurück, wenn für alle
 * Ressourcenklassen @c r gilt: need[r] <= work[r].
 *
 * @param need       Bedarfsvektor eines Prozesses.
 * @param work       Aktueller Arbeitsvektor (verfügbare Ressourcen).
 * @param n_classes  Anzahl der Ressourcenklassen.
 * @return           @c true, wenn der Bedarf gedeckt ist; sonst @c false.
 */
static bool need_leq_work(const uint32_t *need,
                          const uint32_t *work,
                          uint32_t n_classes)
{
    for (uint32_t i = 0; i < n_classes; ++i)
        if (need[i] > work[i]) return false;
    return true;
}

/**
 * @brief Prüft mit dem Bankier-Algorithmus, ob der aktuelle Zustand sicher ist.
 *
 * Initialisiert den Arbeitsvektor @c work mit den aktuell verfügbaren
 * Ressourcen. In einer Schleife werden alle noch nicht abgeschlossenen
 * Prozesse gesucht, deren Bedarf durch @c work gedeckt werden kann.
 * Ein solcher Prozess wird als "fertig" markiert und seine Ressourcen
 * werden zu @c work addiert. Die Schleife wird wiederholt, bis kein
 * Fortschritt mehr möglich ist. Konnten alle Prozesse abgeschlossen
 * werden, ist der Zustand sicher.
 *
 * @param st  Zeiger auf den zu prüfenden Systemzustand (const).
 * @return    @c true, wenn ein sicherer Ablaufplan existiert; sonst @c false.
 */
bool state_is_safe(const SystemState *st)
{
    uint32_t n_p = st->n_procs;
    uint32_t n_r = st->n_classes;

    /* Arbeitsvektor mit verfügbaren Ressourcen initialisieren */
    uint32_t *work = calloc(n_r, sizeof(uint32_t));
    if (!work) { fprintf(stderr, "state_is_safe: out of memory\n"); exit(EXIT_FAILURE); }
    memcpy(work, st->available, n_r * sizeof(uint32_t));

    /* finished[p] = true, wenn Prozess p abgeschlossen werden konnte */
    bool *finished = calloc(n_p, sizeof(bool));
    if (!finished) {
        fprintf(stderr, "state_is_safe: out of memory\n");
        free(work);
        exit(EXIT_FAILURE);
    }

    bool progress;
    do {
        progress = false;
        for (uint32_t p = 0; p < n_p; ++p) {
            if (finished[p]) continue;
            /* Prüfen, ob der Bedarf von Prozess p durch work gedeckt ist */
            if (need_leq_work(st->request[p], work, n_r)) {
                /* Prozess abgeschlossen: Ressourcen zum Arbeitsvektor addieren */
                for (uint32_t r = 0; r < n_r; ++r)
                    work[r] += st->allocation[p][r];
                finished[p] = true;
                progress = true;
            }
        }
    } while (progress);

    /* Sicherheitsprüfung: Konnten alle Prozesse abgeschlossen werden? */
    bool safe = true;
    for (uint32_t p = 0; p < n_p; ++p)
        if (!finished[p]) { safe = false; break; }

    free(work);
    free(finished);
    return safe;
}

/**
 * @brief Gibt eine formatierte Übersicht des Systemzustands auf stdout aus.
 *
 * Zeigt Gesamtressourcen (E), verfügbare Ressourcen (A),
 * die Allokationsmatrix C und die Bedarfsmatrix R an.
 *
 * @param st  Zeiger auf den auszugebenden Systemzustand (const).
 */
void state_print(const SystemState *st)
{
    printf("=== System State =============================\n");
    printf("Resources (total)   : ");
    for (uint32_t i = 0; i < st->n_classes; ++i)
        printf("%u ", st->instances[i]);
    printf("\nResources (free)    : ");
    for (uint32_t i = 0; i < st->n_classes; ++i)
        printf("%u ", st->available[i]);
    printf("\nAllocation matrix C (proc x class):\n");
    for (uint32_t p = 0; p < st->n_procs; ++p) {
        printf(" P%u : ", p);
        for (uint32_t r = 0; r < st->n_classes; ++r)
            printf("%u ", st->allocation[p][r]);
        printf("\n");
    }
    printf("\nRequest (need) matrix R (proc x class):\n");
    for (uint32_t p = 0; p < st->n_procs; ++p) {
        printf(" P%u : ", p);
        for (uint32_t r = 0; r < st->n_classes; ++r)
            printf("%u ", st->request[p][r]);
        printf("\n");
    }
    printf("==============================================\n");
}
