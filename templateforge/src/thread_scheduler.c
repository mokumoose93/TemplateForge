#include "thread_scheduler.h"
#include <string.h>
#include <stdio.h>

const char* thread_state_str(ThreadState s) {
    switch (s) {
        case TS_READY:      return "READY";
        case TS_RUNNING:    return "RUNNING";
        case TS_BLOCKED:    return "BLOCKED";
        case TS_TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

void thread_status_line(const SimThread* t, char* out, int outsz) {
    snprintf(out, outsz, "%4d  %4d  %-12s  %s",
             t->tid, t->owner_pid, thread_state_str(t->state), t->name);
}

void sched_init(Scheduler* sc) {
    memset(sc, 0, sizeof(Scheduler));
    sc->next_tid = 1;
}

int sched_create_thread(Scheduler* sc, const char* name, int owner_pid) {
    if (sc->count >= MAX_THREADS) return -1;
    SimThread* t = &sc->threads[sc->count++];
    t->tid       = sc->next_tid++;
    t->owner_pid = owner_pid;
    t->state     = TS_READY;
    snprintf(t->name, sizeof(t->name), "%s", name ? name : "");
    return t->tid;
}

SimThread* sched_get_thread(Scheduler* sc, int tid) {
    for (int i = 0; i < sc->count; i++)
        if (sc->threads[i].tid == tid) return &sc->threads[i];
    return NULL;
}

int sched_terminate_thread(Scheduler* sc, int tid, char* eb, int esz) {
    SimThread* t = sched_get_thread(sc, tid);
    if (!t) { snprintf(eb, esz, "No thread with TID %d", tid); return -1; }
    t->state = TS_TERMINATED; return 0;
}

int sched_block_thread(Scheduler* sc, int tid, char* eb, int esz) {
    SimThread* t = sched_get_thread(sc, tid);
    if (!t) { snprintf(eb, esz, "No thread with TID %d", tid); return -1; }
    if (t->state == TS_READY || t->state == TS_RUNNING) t->state = TS_BLOCKED;
    return 0;
}

int sched_unblock_thread(Scheduler* sc, int tid, char* eb, int esz) {
    SimThread* t = sched_get_thread(sc, tid);
    if (!t) { snprintf(eb, esz, "No thread with TID %d", tid); return -1; }
    if (t->state == TS_BLOCKED) t->state = TS_READY;
    return 0;
}

void sched_terminate_all_for(Scheduler* sc, int pid) {
    for (int i = 0; i < sc->count; i++)
        if (sc->threads[i].owner_pid == pid && sc->threads[i].state != TS_TERMINATED)
            sc->threads[i].state = TS_TERMINATED;
}

int sched_schedule(Scheduler* sc) {
    /* Demote current RUNNING -> READY */
    for (int i = 0; i < sc->count; i++)
        if (sc->threads[i].state == TS_RUNNING) sc->threads[i].state = TS_READY;

    /* Collect runnable TIDs */
    int runnable[MAX_THREADS]; int nr = 0;
    for (int i = 0; i < sc->count; i++)
        if (sc->threads[i].state == TS_READY) runnable[nr++] = sc->threads[i].tid;
    if (nr == 0) return -1;

    if (sc->run_ptr >= nr) sc->run_ptr = 0;
    int chosen = runnable[sc->run_ptr];
    sched_get_thread(sc, chosen)->state = TS_RUNNING;
    sc->run_ptr = (sc->run_ptr + 1) % nr;
    return chosen;
}
