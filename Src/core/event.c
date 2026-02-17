#define _XOPEN_SOURCE 600
#include "../core/event.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

struct EventHeap {
    Event *data;      /* Array von Events, 1-Basierte Indizierung (Index 0 bleibt leer) */
    size_t size;      /* Aktuelle Anzahl der gespeicherten Elemente */
    size_t capacity;  /* Gesamtkapazität des Arrays (einschließlich unbenutzem Index 0) */
};

#define LEFT(i)   ((i) << 1)        /* Linkes Kind = i * 2 */
#define RIGHT(i)  (((i) << 1) + 1)  /* Rechtes Kind = i * 2 +1 */
#define PARENT(i) ((i) >> 1)        /* Elternknoten = i / 2*/

/* Hilfsfunktion: Tauscht zwei Events */
static void heap_swap(Event *a, Event *b)
{
    Event tmp = *a;
    *a = *b;
    *b = tmp;
}

/* -------------------------------------------------------------- */
EventHeap *event_heap_create(void)
{
    EventHeap *h = calloc(1, sizeof(*h));
    if (!h) { fprintf(stderr, "event_heap_create: out of memory\n"); exit(EXIT_FAILURE); }
    h->capacity = 16;
    h->data = calloc(h->capacity, sizeof(Event));   /* Der Index 0 ist unbenutzt */
    if (!h->data) { fprintf(stderr, "event_heap_create: out of memory\n"); exit(EXIT_FAILURE); }
    return h;
}

void event_heap_destroy(EventHeap *h)
{
    if (!h) return; /* macht den Sicherheitscheck: NULL-Pointer abfangen */
    free(h->data);  /* Event-Array freigeben */
    free(h);        /* Heap-Struktur freigeben*/
}

/* -------------------------------------------------------------- */
static void heap_sift_up(EventHeap *h, size_t idx)
{
    while (idx > 1 && h->data[PARENT(idx)].time > h->data[idx].time) {
        heap_swap(&h->data[PARENT(idx)], &h->data[idx]);
        idx = PARENT(idx);
    }
}

static void heap_sift_down(EventHeap *h, size_t idx)
{
    while (LEFT(idx) <= h->size) {
        size_t smallest = LEFT(idx);
        if (RIGHT(idx) <= h->size &&
            h->data[RIGHT(idx)].time < h->data[smallest].time)
            smallest = RIGHT(idx);
        if (h->data[idx].time <= h->data[smallest].time) break;
        heap_swap(&h->data[idx], &h->data[smallest]);
        idx = smallest;
    }
}

/* -------------------------------------------------------------- */
void event_push(EventHeap *h, const Event *ev)
{
    if (h->size + 1 >= h->capacity) {
        h->capacity *= 2;
        Event *tmp = realloc(h->data, h->capacity * sizeof(Event));
        if (!tmp) { fprintf(stderr, "event_push: out of memory\n"); exit(EXIT_FAILURE); }
        h->data = tmp;
    }
    h->size++;
    h->data[h->size] = *ev;
    heap_sift_up(h, h->size);
}

/* -------------------------------------------------------------- */
bool event_pop(EventHeap *h, Event *out)
{
    if (h->size == 0) return false;
    *out = h->data[1];
    h->data[1] = h->data[h->size];
    h->size--;
    heap_sift_down(h, 1);
    return true;
}

/* -------------------------------------------------------------- */
bool event_peek(const EventHeap *h, Event *out)
{
    if (h->size == 0) return false;
    *out = h->data[1];
    return true;
}

/* -------------------------------------------------------------- */
size_t event_size(const EventHeap *h)
{
    return h->size;
}