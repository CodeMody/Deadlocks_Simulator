#define _XOPEN_SOURCE 600          /* für das usleep() falls es gebracuht wird  */
#include "../core/state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* -----------------------------------------------------------------
   2D-Matrix anlegen und mit 0 initialiseren
   ----------------------------------------------------------------- */
static uint32_t **alloc_matrix(uint32_t rows, uint32_t cols)
{
    uint32_t **m = calloc(rows, sizeof(uint32_t *));
    for (uint32_t i = 0; i < rows; ++i)
        m[i] = calloc(cols, sizeof(uint32_t));
    return m;
}

/* -----------------------------------------------------------------
   2D-Matrix wird freigegeben
   ----------------------------------------------------------------- */
static void free_matrix(uint32_t **m, uint32_t rows)
{
    if (!m) return;
    for (uint32_t i = 0; i < rows; ++i)
        free(m[i]);
    free(m);
}

/* --------------------------------------------------------------
   Öffentliche API
   -------------------------------------------------------------- */
SystemState *state_create(uint32_t n_classes,
                          const uint32_t *instances)
{
    SystemState *st = calloc(1, sizeof(*st));
    st->n_classes = n_classes;
    st->n_procs   = 0;

    st->instances = calloc(n_classes, sizeof(uint32_t));
    st->available = calloc(n_classes, sizeof(uint32_t));

    memcpy(st->instances, instances, n_classes * sizeof(uint32_t));
    memcpy(st->available, instances, n_classes * sizeof(uint32_t));

    st->allocation = NULL;
    st->request    = NULL;
    return st;
}

/* -------------------------------------------------------------
    Speicher für Prozess-Matrizen angelegt
   -------------------------------------------------------------- */
void state_allocate_process_matrices(SystemState *st, uint32_t n_procs)
{
    st->n_procs   = n_procs;               /* store it for later use */
    st->allocation = alloc_matrix(n_procs, st->n_classes);
    st->request    = alloc_matrix(n_procs, st->n_classes);
}

/* --------------------------------------------------------------
   Alles wird freigegeben
   -------------------------------------------------------------- */
void state_destroy(SystemState *st)
{
    if (!st) return;
    free(st->instances);
    free(st->available);
    free_matrix(st->allocation, st->n_procs);
    free_matrix(st->request,    st->n_procs);
    free(st);
}

/* --------------------------------------------------------------
   Setzt eine Zuweisung
   -------------------------------------------------------------- */
void state_set_allocation(SystemState *st,
                          uint32_t pid,
                          uint32_t class_id,
                          uint32_t amount)
{
    if (pid >= st->n_procs || class_id >= st->n_classes) return;
    st->allocation[pid][class_id] = amount;
}

/* Setzt eine Anforderung auf*/
void state_set_request(SystemState *st,
                       uint32_t pid,
                       uint32_t class_id,
                       uint32_t amount)
{
    if (pid >= st->n_procs || class_id >= st->n_classes) return;
    st->request[pid][class_id] = amount;
}

/* --------------------------------------------------------------
   Prüft, ob Bedarf ≤ verfügbar (Hilfsfunktion)
   -------------------------------------------------------------- */
static bool need_leq_work(const uint32_t *need,
                          const uint32_t *work,
                          uint32_t n_classes)
{
    for (uint32_t i = 0; i < n_classes; ++i)
        if (need[i] > work[i]) return false;
    return true;
}

/*Sicherheitsprüfung nach dem Bankier-Algorithmus*/
bool state_is_safe(const SystemState *st)
{
    uint32_t n_p = st->n_procs;
    uint32_t n_r = st->n_classes;

    uint32_t *work = calloc(n_r, sizeof(uint32_t));
    memcpy(work, st->available, n_r * sizeof(uint32_t));

    bool *finished = calloc(n_p, sizeof(bool));

    bool progress;
    do {
        progress = false;
        for (uint32_t p = 0; p < n_p; ++p) {
            if (finished[p]) continue;
            if (need_leq_work(st->request[p], work, n_r)) {
                for (uint32_t r = 0; r < n_r; ++r)
                    work[r] += st->allocation[p][r];
                finished[p] = true;
                progress = true;
            }
        }
    } while (progress);

    bool safe = true;
    for (uint32_t p = 0; p < n_p; ++p)
        if (!finished[p]) { safe = false; break; }

    free(work);
    free(finished);
    return safe;
}

/* --------------------------------------------------------------
   Ausgabe des aktuellen Systemzustands
   -------------------------------------------------------------- */
void state_print(const SystemState *st)
{
    printf("=== System State =============================\n");
    printf("Resources (total)   : ");
    for (uint32_t i = 0; i < st->n_classes; ++i)
        printf("%u ", st->instances[i]);
    printf("\nResources (free)    : ");
    for (uint32_t i = 0; i < st->n_classes; ++i)
        printf("%u ", st->available[i]);
    printf("\nAllocation matrix C (proc × class):\n");
    for (uint32_t p = 0; p < st->n_procs; ++p) {
        printf(" P%u : ", p);
        for (uint32_t r = 0; r < st->n_classes; ++r)
            printf("%u ", st->allocation[p][r]);
        printf("\n");
    }
    printf("\nRequest (need) matrix R (proc × class):\n");
    for (uint32_t p = 0; p < st->n_procs; ++p) {
        printf(" P%u : ", p);
        for (uint32_t r = 0; r < st->n_classes; ++r)
            printf("%u ", st->request[p][r]);
        printf("\n");
    }
    printf("==============================================\n");
}