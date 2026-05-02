#pragma once

/* Replaces C++ MutexSim. */
typedef struct {
    int  id;
    char name[64];
    int  locked;
    int  owner_tid;  /* -1 when unlocked */
} MutexSim;

void mutex_init(MutexSim* m, int id, const char* name);

/* Both return 1=success/0=failure and write a message into msg (size msgsz). */
int  mutex_acquire(MutexSim* m, int tid, char* msg, int msgsz);
int  mutex_release(MutexSim* m, int tid, char* msg, int msgsz);

/* Writes status string into out (size outsz). */
void mutex_status_str(const MutexSim* m, char* out, int outsz);
