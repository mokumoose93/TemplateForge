#include "pipe.h"
#include <stdlib.h>
#include <string.h>

void pipe_init(Pipe* p, int id) {
    memset(p, 0, sizeof(Pipe));
    p->id = id;
}

void pipe_free(Pipe* p) {
    while (p->count > 0) { free(p->items[p->head]); p->head = (p->head + 1) % PIPE_CAP; p->count--; }
}

void pipe_write(Pipe* p, const char* data) {
    if (p->count >= PIPE_CAP) { free(p->items[p->head]); p->head = (p->head + 1) % PIPE_CAP; p->count--; }
    p->items[p->tail] = strdup(data ? data : "");
    p->tail = (p->tail + 1) % PIPE_CAP;
    p->count++;
}

char* pipe_read(Pipe* p) {
    if (p->count == 0) return NULL;
    char* s = p->items[p->head];
    p->items[p->head] = NULL;
    p->head = (p->head + 1) % PIPE_CAP;
    p->count--;
    return s; /* caller frees */
}

const char* pipe_peek(const Pipe* p) { return p->count ? p->items[p->head] : NULL; }
int pipe_empty(const Pipe* p) { return p->count == 0; }
int pipe_size (const Pipe* p) { return p->count; }
