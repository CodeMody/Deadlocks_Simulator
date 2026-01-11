#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>          /* für size_t */

typedef enum {
    EV_REQUEST,     /*Prozess fordert Ressource an*/
    EV_RELEASE,     /* Prozess gibt Ressource frei*/
    EV_CHECKPOINT,  /* Überprüfungsereignis (z.B, ZUstand testen)*/
    EV_TERMINATE    /*Prozess beendet sich*/
} EventType;

/* --------------------------------------------------------------
   Einzelnes Ereignis in der Event-Queue
   -------------------------------------------------------------- */
typedef struct {
    uint64_t   time;       /* logische Zeit (Ticks)                */
    EventType  type;       /* Ereignissart                       */
    uint32_t   pid;        /* Prozess-ID    */
    uint32_t   class_id;   /* betroffene Ressourcenklasse             */
    uint32_t   amount;     /* Anzahl der Instanzen      */
} Event;

struct Policy;

/* Funktionszeiger für Policy-Callbacks */
typedef bool  (*policy_request_f)(struct Policy *p,
                                  struct SystemState *st,
                                  const Event *ev);
typedef void  (*policy_tick_f)(struct Policy *p,
                               struct SystemState *st,
                               uint64_t now);
typedef void  (*policy_cleanup_f)(struct Policy *p);

/* --------------------------------------------------------------
   Hauptstruktur für den Ressourcen-Zustand des Systems
   -------------------------------------------------------------- */
typedef struct SystemState {
    uint32_t n_classes;      /* Anzahl verschiedener Ressourcenklassen*/
    uint32_t n_procs;        /* Anzahl der Prozesse*/
    uint32_t *instances;    /* Gesamtklasse je Klasse (E) */
    uint32_t *available;    /* Verfügbare Instanzen je Klasse   (A)*/
    uint32_t **allocation;  /* C-Matrix -> gehaltene Instanzen (Prozess x Klasse */
    uint32_t **request;     /* R-Matrix -> noch benötigte Instanzen (Prozess x Klasse) */
} SystemState;

#endif /* TYPES_H */