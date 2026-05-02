#include "signal_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

int signal_from_str(const char* s) {
    if (!s) return -1;
    char up[32]; int i = 0;
    for (; s[i] && i < 31; i++) up[i] = (char)toupper((unsigned char)s[i]);
    up[i] = '\0';
    /* accept with or without "SIG" prefix */
    const char* n = (strncmp(up, "SIG", 3) == 0) ? up : up;
    char buf[32]; snprintf(buf, sizeof(buf), "%s%s", strncmp(up,"SIG",3)==0?"":"SIG", up);
    if (strcmp(buf,"SIGTERM")==0) return SIG_TERM;
    if (strcmp(buf,"SIGKILL")==0) return SIG_KILL;
    if (strcmp(buf,"SIGINT") ==0) return SIG_INT;
    if (strcmp(buf,"SIGHUP") ==0) return SIG_HUP;
    if (strcmp(buf,"SIGUSR1")==0) return SIG_USR1;
    if (strcmp(buf,"SIGUSR2")==0) return SIG_USR2;
    if (strcmp(buf,"SIGCHLD")==0) return SIG_CHLD;
    if (strcmp(buf,"SIGSTOP")==0) return SIG_STOP;
    if (strcmp(buf,"SIGCONT")==0) return SIG_CONT;
    (void)n;
    return -1;
}

const char* signal_to_str(Signal s) {
    switch (s) {
        case SIG_TERM: return "SIGTERM"; case SIG_KILL: return "SIGKILL";
        case SIG_INT:  return "SIGINT";  case SIG_HUP:  return "SIGHUP";
        case SIG_USR1: return "SIGUSR1"; case SIG_USR2: return "SIGUSR2";
        case SIG_CHLD: return "SIGCHLD"; case SIG_STOP: return "SIGSTOP";
        case SIG_CONT: return "SIGCONT";
    }
    return "UNKNOWN";
}

const char* default_action_for(Signal s) {
    switch (s) {
        case SIG_TERM: case SIG_KILL: case SIG_INT: case SIG_HUP: return "terminate";
        case SIG_USR1: case SIG_USR2: case SIG_CHLD: return "ignore";
        case SIG_STOP: return "stop";
        case SIG_CONT: return "continue";
    }
    return "ignore";
}

int is_uncatchable(Signal s) { return s == SIG_KILL || s == SIG_STOP; }

void sreg_init(SignalReg* r) { memset(r, 0, sizeof(SignalReg)); }

void sreg_free(SignalReg* r) {
    for (int i = 0; i < r->n_log; i++) free(r->log[i]);
}

int sreg_register(SignalReg* r, int pid, Signal sig, SigHandlerFn fn, void* ctx) {
    if (is_uncatchable(sig)) return 0;
    /* overwrite if (pid,sig) already registered */
    for (int i = 0; i < r->count; i++) {
        if (r->entries[i].pid == pid && r->entries[i].sig == sig) {
            r->entries[i].fn = fn; r->entries[i].ctx = ctx; return 1;
        }
    }
    if (r->count >= MAX_SIGNAL_HANDLERS) return 0;
    r->entries[r->count++] = (HandlerEntry){pid, sig, fn, ctx};
    return 1;
}

void sreg_unregister(SignalReg* r, int pid, Signal sig) {
    for (int i = 0; i < r->count; i++) {
        if (r->entries[i].pid == pid && r->entries[i].sig == sig) {
            memmove(&r->entries[i], &r->entries[i+1], (r->count-i-1)*sizeof(HandlerEntry));
            r->count--; return;
        }
    }
}

char* sreg_dispatch(SignalReg* r, int pid, Signal sig) {
    char entry[128];
    snprintf(entry, sizeof(entry), "%s -> PID %d", signal_to_str(sig), pid);
    char msg[512];
    if (is_uncatchable(sig)) {
        snprintf(msg, sizeof(msg), "%s: %s (uncatchable)", entry, default_action_for(sig));
    } else {
        SigHandlerFn fn = NULL; void* ctx = NULL;
        for (int i = 0; i < r->count; i++)
            if (r->entries[i].pid == pid && r->entries[i].sig == sig)
                { fn = r->entries[i].fn; ctx = r->entries[i].ctx; break; }
        if (fn) {
            char* res = fn(pid, sig, ctx);
            snprintf(msg, sizeof(msg), "%s: custom handler -> %s", entry, res ? res : "");
            free(res);
        } else {
            snprintf(msg, sizeof(msg), "%s: default action '%s'", entry, default_action_for(sig));
        }
    }
    if (r->n_log < MAX_SIGNAL_LOG) r->log[r->n_log++] = strdup(msg);
    return strdup(msg);
}
