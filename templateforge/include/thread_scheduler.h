#pragma once

#define MAX_THREADS 256

typedef enum { TS_READY, TS_RUNNING, TS_BLOCKED, TS_TERMINATED } ThreadState;

/* Replaces C++ SimThread struct. */
typedef struct {
    int         tid;
    char        name[64];
    int         owner_pid;
    ThreadState state;
} SimThread;

const char* thread_state_str(ThreadState s);
/* Writes formatted status line into out. */
void        thread_status_line(const SimThread* t, char* out, int outsz);

/* Replaces C++ Scheduler class. */
typedef struct {
    SimThread threads[MAX_THREADS];
    int       count;
    int       next_tid;
    int       run_ptr;
} Scheduler;

void  sched_init               (Scheduler* sc);
int   sched_create_thread      (Scheduler* sc, const char* name, int owner_pid);
int   sched_terminate_thread   (Scheduler* sc, int tid, char* eb, int esz);
int   sched_block_thread       (Scheduler* sc, int tid, char* eb, int esz);
int   sched_unblock_thread     (Scheduler* sc, int tid, char* eb, int esz);
void  sched_terminate_all_for  (Scheduler* sc, int pid);
/* Returns chosen TID, or -1 if no runnable thread. */
int   sched_schedule           (Scheduler* sc);
/* Returns pointer into internal array or NULL. */
SimThread* sched_get_thread    (Scheduler* sc, int tid);
