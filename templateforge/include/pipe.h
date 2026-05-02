#pragma once

#define PIPE_CAP 256  /* max buffered messages */

/* Replaces C++ Pipe (std::deque<string>). */
typedef struct {
    int   id;
    char* items[PIPE_CAP]; /* owned strings, circular */
    int   head;
    int   tail;
    int   count;
} Pipe;

void  pipe_init (Pipe* p, int id);
void  pipe_free (Pipe* p);
void  pipe_write(Pipe* p, const char* data);
/* Returns heap-allocated string (caller frees) or NULL if empty. */
char* pipe_read (Pipe* p);
/* Returns non-owning pointer or NULL.  Do not free. */
const char* pipe_peek(const Pipe* p);
int   pipe_empty(const Pipe* p);
int   pipe_size (const Pipe* p);
