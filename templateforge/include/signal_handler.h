#pragma once

#define MAX_SIGNAL_HANDLERS 256
#define MAX_SIGNAL_LOG      512

typedef enum {
    SIG_TERM, SIG_KILL, SIG_INT, SIG_HUP,
    SIG_USR1, SIG_USR2, SIG_CHLD, SIG_STOP, SIG_CONT
} Signal;

/* Converts "SIGTERM" / "TERM" (case-insensitive) -> enum.
   Returns -1 on unknown signal. */
int         signal_from_str   (const char* s);
const char* signal_to_str     (Signal s);
const char* default_action_for(Signal s);
int         is_uncatchable    (Signal s);

/* Handler callback: returns heap-allocated message string, caller frees. */
typedef char* (*SigHandlerFn)(int pid, Signal sig, void* ctx);

typedef struct {
    int          pid;
    Signal       sig;
    SigHandlerFn fn;
    void*        ctx;   /* non-owning */
} HandlerEntry;

/* Replaces C++ SignalRegistry. */
typedef struct {
    HandlerEntry entries[MAX_SIGNAL_HANDLERS];
    int          count;
    char*        log[MAX_SIGNAL_LOG]; /* owned strings */
    int          n_log;
} SignalReg;

void  sreg_init      (SignalReg* r);
void  sreg_free      (SignalReg* r);
/* Returns 0 if signal is uncatchable (not registered). */
int   sreg_register  (SignalReg* r, int pid, Signal sig, SigHandlerFn fn, void* ctx);
void  sreg_unregister(SignalReg* r, int pid, Signal sig);
/* Returns heap-allocated result string; caller frees. */
char* sreg_dispatch  (SignalReg* r, int pid, Signal sig);
