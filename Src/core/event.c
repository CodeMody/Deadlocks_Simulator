/**
 * @file event.c
 * @brief Implementierung des Min-Heap-basierten Ereignis-Heaps.
 *
 * Der @c EventHeap ist eine dynamisch wachsende Prioritätswarteschlange,
 * die Ereignisse aufsteigend nach ihrem logischen Zeitstempel (@c time)
 * sortiert. Er verwendet 1-basierte Indizierung, sodass der Index 0
 * unbenutzt bleibt und die Eltern-Kind-Beziehungen durch einfache
 * Bit-Operationen berechnet werden können.
 *
 * Zur Berechnung der Heap-Positionen dienen folgende Makros:
 * - @c LEFT(i)   = linkes Kind  = i × 2
 * - @c RIGHT(i)  = rechtes Kind = i × 2 + 1
 * - @c PARENT(i) = Elternknoten = i / 2
 */

#define _XOPEN_SOURCE 600
#include "../core/event.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * @brief Interne Struktur des Ereignis-Heaps.
 *
 * Das Daten-Array (@c data) wird dynamisch verwaltet. Die Kapazität
 * verdoppelt sich bei Bedarf. Index 0 bleibt aus Gründen der einfacheren
 * Eltern-Kind-Berechnung unbenutzt.
 */
struct EventHeap {
    Event  *data;     /**< Daten-Array; gültige Elemente liegen auf den Indizes 1..size. */
    size_t  size;     /**< Aktuelle Anzahl der gespeicherten Ereignisse. */
    size_t  capacity; /**< Gesamtkapazität des Arrays (einschließlich des unbenutzten Index 0). */
};

/** @brief Berechnet den Index des linken Kindknotens. */
#define LEFT(i)   ((i) << 1)
/** @brief Berechnet den Index des rechten Kindknotens. */
#define RIGHT(i)  (((i) << 1) + 1)
/** @brief Berechnet den Index des Elternknotens. */
#define PARENT(i) ((i) >> 1)

/**
 * @brief Tauscht zwei Ereignisse im Array.
 *
 * @param a  Zeiger auf das erste Ereignis.
 * @param b  Zeiger auf das zweite Ereignis.
 */
static void heap_swap(Event *a, Event *b)
{
    Event tmp = *a;
    *a = *b;
    *b = tmp;
}

/**
 * @brief Erstellt einen neuen, leeren Ereignis-Heap.
 *
 * Allokiert Speicher für die Heap-Struktur und ein initiales Daten-Array
 * der Kapazität 16 (Index 0 bleibt unbenutzt).
 *
 * @return Zeiger auf den neu erstellten @c EventHeap.
 */
EventHeap *event_heap_create(void)
{
    EventHeap *h = calloc(1, sizeof(*h));
    if (!h) { fprintf(stderr, "event_heap_create: out of memory\n"); exit(EXIT_FAILURE); }
    h->capacity = 16;
    /* Index 0 wird nie verwendet; calloc initialisiert alle Felder mit 0. */
    h->data = calloc(h->capacity, sizeof(Event));
    if (!h->data) { fprintf(stderr, "event_heap_create: out of memory\n"); exit(EXIT_FAILURE); }
    return h;
}

/**
 * @brief Gibt einen Ereignis-Heap und seinen gesamten Speicher frei.
 *
 * @param h  Zeiger auf den zu zerstörenden Heap. NULL wird sicher ignoriert.
 */
void event_heap_destroy(EventHeap *h)
{
    if (!h) return;
    free(h->data);
    free(h);
}

/**
 * @brief Stellt die Heap-Eigenschaft durch Aufsteigen wieder her.
 *
 * Verschiebt das Element an Position @p idx solange nach oben (Richtung Wurzel),
 * bis sein Zeitstempel kleiner-gleich dem seines Elternknotens ist.
 *
 * @param h    Zeiger auf den Heap.
 * @param idx  Startindex des aufzusteigenden Elements (1-basiert).
 */
static void heap_sift_up(EventHeap *h, size_t idx)
{
    while (idx > 1 && h->data[PARENT(idx)].time > h->data[idx].time) {
        heap_swap(&h->data[PARENT(idx)], &h->data[idx]);
        idx = PARENT(idx);
    }
}

/**
 * @brief Stellt die Heap-Eigenschaft durch Absinken wieder her.
 *
 * Verschiebt das Element an Position @p idx solange nach unten (Richtung Blätter),
 * bis sein Zeitstempel kleiner-gleich dem seiner Kindknoten ist.
 *
 * @param h    Zeiger auf den Heap.
 * @param idx  Startindex des absinkenden Elements (1-basiert).
 */
static void heap_sift_down(EventHeap *h, size_t idx)
{
    while (LEFT(idx) <= h->size) {
        size_t smallest = LEFT(idx);
        /* Wähle das Kind mit dem kleineren Zeitstempel */
        if (RIGHT(idx) <= h->size &&
            h->data[RIGHT(idx)].time < h->data[smallest].time)
            smallest = RIGHT(idx);
        if (h->data[idx].time <= h->data[smallest].time) break;
        heap_swap(&h->data[idx], &h->data[smallest]);
        idx = smallest;
    }
}

/**
 * @brief Fügt ein Ereignis in den Heap ein.
 *
 * Falls die Kapazität erschöpft ist, wird das interne Array auf die doppelte
 * Größe vergrößert. Das neue Ereignis wird am Ende eingefügt und anschließend
 * durch @c heap_sift_up() an die richtige Position verschoben.
 *
 * @param h   Zeiger auf den Heap.
 * @param ev  Zeiger auf das einzufügende Ereignis (wird per Wert kopiert).
 */
void event_push(EventHeap *h, const Event *ev)
{
    /* Kapazität prüfen und bei Bedarf verdoppeln */
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

/**
 * @brief Entfernt das zeitlich früheste Ereignis aus dem Heap.
 *
 * Das Wurzelelement (Index 1, kleinster Zeitstempel) wird in @p out
 * geschrieben. Das letzte Element rückt an die Wurzel und sinkt durch
 * @c heap_sift_down() an die richtige Position.
 *
 * @param h    Zeiger auf den Heap.
 * @param out  Ausgabeparameter; wird mit dem entfernten Ereignis befüllt.
 * @return     @c true bei Erfolg; @c false, wenn der Heap leer ist.
 */
bool event_pop(EventHeap *h, Event *out)
{
    if (h->size == 0) return false;
    *out = h->data[1];
    /* Letztes Element an die Wurzel verschieben und Heap reparieren */
    h->data[1] = h->data[h->size];
    h->size--;
    heap_sift_down(h, 1);
    return true;
}

/**
 * @brief Liest das zeitlich früheste Ereignis, ohne es zu entfernen.
 *
 * @param h    Zeiger auf den Heap (const).
 * @param out  Ausgabeparameter; wird mit dem Wurzelelement befüllt.
 * @return     @c true bei Erfolg; @c false, wenn der Heap leer ist.
 */
bool event_peek(const EventHeap *h, Event *out)
{
    if (h->size == 0) return false;
    *out = h->data[1];
    return true;
}

/**
 * @brief Gibt die aktuelle Anzahl der Ereignisse im Heap zurück.
 *
 * @param h  Zeiger auf den Heap (const).
 * @return   Anzahl der gespeicherten Ereignisse.
 */
size_t event_size(const EventHeap *h)
{
    return h->size;
}
