#ifndef EVENT_H
#define EVENT_H

#include "types.h"
#include <stddef.h>

typedef struct EventHeap EventHeap;

/* Public API */
EventHeap *event_heap_create(void);
void       event_heap_destroy(EventHeap *h);

void       event_push(EventHeap *h, const Event *ev);
bool       event_pop(EventHeap *h, Event *out);   /* false → empty */
bool       event_peek(const EventHeap *h, Event *out);
size_t     event_size(const EventHeap *h);

#endif /* EVENT_H */