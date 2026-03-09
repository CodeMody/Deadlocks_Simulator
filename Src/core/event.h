/**
 * @file event.h
 * @brief Öffentliches Interface für den zeitgesteuerten Ereignis-Heap.
 *
 * Der @c EventHeap ist eine Min-Heap-basierte Prioritätswarteschlange,
 * die Ereignisse aufsteigend nach ihrem logischen Zeitstempel (@c time)
 * sortiert. Er wird vom @c Scheduler genutzt, um Ereignisse in chronologischer
 * Reihenfolge zu verarbeiten.
 */

#ifndef EVENT_H
#define EVENT_H

#include "types.h"
#include <stddef.h>

/**
 * @brief Opaker Zeiger auf die interne Heap-Struktur.
 *
 * Die Implementierungsdetails sind nur in @c event.c sichtbar
 * Externe Module arbeiten ausschließlich
 * über die nachfolgenden Funktionen.
 */
typedef struct EventHeap EventHeap;

/**
 * @brief Erstellt einen neuen, leeren Ereignis-Heap.
 *
 * Allokiert Speicher für die Heap-Struktur und das interne Daten-Array.
 * Die Anfangskapazität beträgt 16 Elemente und verdoppelt sich bei Bedarf.
 *
 * @return Zeiger auf den neu erstellten @c EventHeap.
 */
EventHeap *event_heap_create(void);

/**
 * @brief Gibt einen Ereignis-Heap und seinen gesamten Speicher frei.
 *
 * @param h  Zeiger auf den zu zerstörenden Heap. Ein NULL-Zeiger wird sicher ignoriert.
 */
void event_heap_destroy(EventHeap *h);

/**
 * @brief Fügt ein Ereignis in den Heap ein.
 *
 * Das Ereignis wird an der richtigen Position einsortiert, sodass
 * das zeitlich früheste Ereignis stets an der Wurzel liegt.
 * Bei voller Kapazität wird das interne Array automatisch verdoppelt.
 *
 * @param h   Zeiger auf den Heap.
 * @param ev  Zeiger auf das einzufügende Ereignis (wird per Wert kopiert).
 */
void event_push(EventHeap *h, const Event *ev);

/**
 * @brief Entfernt das zeitlich früheste Ereignis aus dem Heap.
 *
 * Das Wurzelelement (minimaler Zeitstempel) wird in @p out geschrieben
 * und aus dem Heap entfernt. Anschließend wird die Heap-Eigenschaft
 * durch Absinken des neuen Wurzelelements wiederhergestellt.
 *
 * @param h    Zeiger auf den Heap.
 * @param out  Ausgabeparameter; wird mit dem entfernten Ereignis befüllt.
 * @return     @c true bei Erfolg; @c false, wenn der Heap leer ist.
 */
bool event_pop(EventHeap *h, Event *out);

/**
 * @brief Liest das zeitlich früheste Ereignis, ohne es zu entfernen.
 *
 * @param h    Zeiger auf den Heap (const).
 * @param out  Ausgabeparameter; wird mit dem Ereignis an der Wurzel befüllt.
 * @return     @c true bei Erfolg; @c false, wenn der Heap leer ist.
 */
bool event_peek(const EventHeap *h, Event *out);

/**
 * @brief Gibt die aktuelle Anzahl der Ereignisse im Heap zurück.
 *
 * @param h  Zeiger auf den Heap (const).
 * @return   Anzahl der gespeicherten Ereignisse.
 */
size_t event_size(const EventHeap *h);

#endif /* EVENT_H */
