#pragma once
#include "pipe.h"
#include "mutex_sim.h"
#include "thread_scheduler.h"
#include "signal_handler.h"

#define MAX_PROCS    256
#define MAX_PIPES     64
#define MAX_MUTEXES   64
#define MAX_CHILDREN  64

typedef enum { PS_RUNNING, PS_SLEEPING, PS_STOPPED, PS_ZOMBIE, PS_TERMINATED } ProcessState;
const char* process_state_str(ProcessState s);

/* Replaces C++ Process struct. */
typedef struct {
    int          pid;
    char         name[64];
    ProcessState state;
    int          parent_pid;
    int          children[MAX_CHILDREN];
    int          n_children;
    int          exit_code;
} Process;

/* Writes "PID  PPID  STATE  NAME" line into out. */
void process_status_line(const Process* p, char* out, int outsz);

/* Replaces C++ ProcessManager — aggregates all OS-sim subsystems. */
typedef struct {
    Process   procs[MAX_PROCS];
    int       n_procs;
    int       next_pid;

    Pipe      pipes[MAX_PIPES];
    int       n_pipes;
    int       next_pipe;

    MutexSim  mutexes[MAX_MUTEXES];
    int       n_mutexes;
    int       next_mutex;

    Scheduler sched;
    SignalReg signals;
} ProcManager;

void     pm_init(ProcManager* pm);
void     pm_free(ProcManager* pm);

/* Process lifecycle */
int      pm_spawn      (ProcManager* pm, const char* name, int parent_pid,
                        char* eb, int esz);
int      pm_terminate  (ProcManager* pm, int pid, char* eb, int esz);
Process* pm_get_process(ProcManager* pm, int pid);

/* Returns heap string; caller frees. */
char*    pm_send_signal(ProcManager* pm, int pid, const char* sig_str,
                        char* eb, int esz);

/* Pipes */
int      pm_create_pipe(ProcManager* pm, char* eb, int esz);
Pipe*    pm_get_pipe   (ProcManager* pm, int pipe_id, char* eb, int esz);
void     pm_pipe_write (ProcManager* pm, int pipe_id, const char* data, char* eb, int esz);
/* Returns heap string or NULL; caller frees. */
char*    pm_pipe_read  (ProcManager* pm, int pipe_id, char* eb, int esz);

/* Mutexes */
int       pm_create_mutex (ProcManager* pm, const char* name, char* eb, int esz);
MutexSim* pm_find_mutex   (ProcManager* pm, const char* name_or_id, char* eb, int esz);
