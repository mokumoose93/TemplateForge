#include "mutex_sim.h"
#include <string.h>
#include <stdio.h>

void mutex_init(MutexSim* m, int id, const char* name) {
    m->id = id; m->locked = 0; m->owner_tid = -1;
    snprintf(m->name, sizeof(m->name), "%s", name ? name : "");
}

int mutex_acquire(MutexSim* m, int tid, char* msg, int msgsz) {
    if (m->locked) {
        if (m->owner_tid == tid)
            snprintf(msg, msgsz, "Mutex '%s': TID %d already holds this lock (deadlock avoided)", m->name, tid);
        else
            snprintf(msg, msgsz, "Mutex '%s': locked by TID %d — TID %d would block", m->name, m->owner_tid, tid);
        return 0;
    }
    m->locked = 1; m->owner_tid = tid;
    snprintf(msg, msgsz, "Mutex '%s' acquired by TID %d", m->name, tid);
    return 1;
}

int mutex_release(MutexSim* m, int tid, char* msg, int msgsz) {
    if (!m->locked)        { snprintf(msg, msgsz, "Mutex '%s': not currently locked", m->name); return 0; }
    if (m->owner_tid != tid) { snprintf(msg, msgsz, "Mutex '%s': owned by TID %d, not TID %d", m->name, m->owner_tid, tid); return 0; }
    m->locked = 0; m->owner_tid = -1;
    snprintf(msg, msgsz, "Mutex '%s' released by TID %d", m->name, tid);
    return 1;
}

void mutex_status_str(const MutexSim* m, char* out, int outsz) {
    if (m->locked) snprintf(out, outsz, "LOCKED (owner: TID %d)", m->owner_tid);
    else           snprintf(out, outsz, "UNLOCKED");
}
