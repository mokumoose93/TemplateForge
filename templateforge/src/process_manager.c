#include "process_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* process_state_str(ProcessState s) {
    switch (s) {
        case PS_RUNNING:    return "RUNNING";
        case PS_SLEEPING:   return "SLEEPING";
        case PS_STOPPED:    return "STOPPED";
        case PS_ZOMBIE:     return "ZOMBIE";
        case PS_TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

void process_status_line(const Process* p, char* out, int outsz) {
    char ppid[16];
    if (p->parent_pid >= 0) snprintf(ppid, sizeof(ppid), "%d", p->parent_pid);
    else snprintf(ppid, sizeof(ppid), "-");
    snprintf(out, outsz, "%4d  %4s  %-12s  %s",
             p->pid, ppid, process_state_str(p->state), p->name);
}

/* ---- Internal: SIGTERM default handler ---- */
static char* sigterm_handler(int pid, Signal sig, void* ctx) {
    ProcManager* pm = (ProcManager*)ctx;
    (void)sig;
    /* mark zombie */
    Process* p = pm_get_process(pm, pid);
    if (p) { p->state = PS_ZOMBIE; p->exit_code = 0; }
    sched_terminate_all_for(&pm->sched, pid);
    char* msg = malloc(64);
    snprintf(msg, 64, "process %d exited cleanly", pid);
    return msg;
}

void pm_init(ProcManager* pm) {
    memset(pm, 0, sizeof(ProcManager));
    pm->next_pid   = 2;
    pm->next_pipe  = 1;
    pm->next_mutex = 1;
    sched_init(&pm->sched);
    sreg_init(&pm->signals);

    /* create init (PID 1) */
    Process* init   = &pm->procs[pm->n_procs++];
    init->pid        = 1;
    init->parent_pid = -1;
    init->state      = PS_RUNNING;
    init->exit_code  = 0;
    snprintf(init->name, sizeof(init->name), "init");
    sched_create_thread(&pm->sched, "main", 1);
    sreg_register(&pm->signals, 1, SIG_TERM, sigterm_handler, pm);
}

void pm_free(ProcManager* pm) {
    for (int i = 0; i < pm->n_pipes; i++) pipe_free(&pm->pipes[i]);
    sreg_free(&pm->signals);
}

Process* pm_get_process(ProcManager* pm, int pid) {
    for (int i = 0; i < pm->n_procs; i++)
        if (pm->procs[i].pid == pid) return &pm->procs[i];
    return NULL;
}

static void do_terminate(ProcManager* pm, int pid, int exit_code) {
    Process* p = pm_get_process(pm, pid);
    if (!p) return;
    p->state     = PS_ZOMBIE;
    p->exit_code = exit_code;
    sched_terminate_all_for(&pm->sched, pid);
}

int pm_spawn(ProcManager* pm, const char* name, int parent_pid, char* eb, int esz) {
    if (parent_pid >= 0 && !pm_get_process(pm, parent_pid)) {
        snprintf(eb, esz, "spawn: parent PID %d does not exist", parent_pid); return -1;
    }
    if (pm->n_procs >= MAX_PROCS) { snprintf(eb, esz, "spawn: process table full"); return -1; }
    int pid = pm->next_pid++;
    Process* p = &pm->procs[pm->n_procs++];
    memset(p, 0, sizeof(Process));
    p->pid        = pid;
    p->parent_pid = parent_pid;
    p->state      = PS_RUNNING;
    snprintf(p->name, sizeof(p->name), "%s", name ? name : "process");
    if (parent_pid >= 0) {
        Process* par = pm_get_process(pm, parent_pid);
        if (par && par->n_children < MAX_CHILDREN)
            par->children[par->n_children++] = pid;
    }
    sched_create_thread(&pm->sched, "main", pid);
    sreg_register(&pm->signals, pid, SIG_TERM, sigterm_handler, pm);
    return pid;
}

int pm_terminate(ProcManager* pm, int pid, char* eb, int esz) {
    if (!pm_get_process(pm, pid)) { snprintf(eb, esz, "kill: no such process: PID %d", pid); return -1; }
    if (pid == 1)                  { snprintf(eb, esz, "kill: cannot terminate init (PID 1)"); return -1; }
    char* msg = pm_send_signal(pm, pid, "SIGTERM", eb, esz);
    free(msg);
    return 0;
}

char* pm_send_signal(ProcManager* pm, int pid, const char* sig_str, char* eb, int esz) {
    Process* p = pm_get_process(pm, pid);
    if (!p) { snprintf(eb, esz, "signal: no such process: PID %d", pid); return NULL; }
    if (p->state == PS_ZOMBIE || p->state == PS_TERMINATED) {
        char* msg = malloc(64); snprintf(msg, 64, "PID %d is already dead", pid); return msg;
    }
    int sig = signal_from_str(sig_str);
    if (sig < 0) { snprintf(eb, esz, "Unknown signal: '%s'", sig_str); return NULL; }
    Signal s = (Signal)sig;
    if (s == SIG_KILL) { do_terminate(pm, pid, -9); char* m = malloc(64); snprintf(m, 64, "PID %d killed with SIGKILL", pid); return m; }
    if (s == SIG_STOP) { p->state = PS_STOPPED;  char* m = malloc(64); snprintf(m, 64, "PID %d stopped", pid);    return m; }
    if (s == SIG_CONT) {
        if (p->state == PS_STOPPED) p->state = PS_RUNNING;
        char* m = malloc(64); snprintf(m, 64, "PID %d continued", pid); return m;
    }
    return sreg_dispatch(&pm->signals, pid, s);
}

/* ---- Pipes ---- */
int pm_create_pipe(ProcManager* pm, char* eb, int esz) {
    if (pm->n_pipes >= MAX_PIPES) { snprintf(eb, esz, "mkpipe: pipe table full"); return -1; }
    int id = pm->next_pipe++;
    pipe_init(&pm->pipes[pm->n_pipes++], id);
    return id;
}

Pipe* pm_get_pipe(ProcManager* pm, int pipe_id, char* eb, int esz) {
    for (int i = 0; i < pm->n_pipes; i++)
        if (pm->pipes[i].id == pipe_id) return &pm->pipes[i];
    snprintf(eb, esz, "No pipe with ID %d", pipe_id); return NULL;
}

void pm_pipe_write(ProcManager* pm, int pipe_id, const char* data, char* eb, int esz) {
    Pipe* p = pm_get_pipe(pm, pipe_id, eb, esz);
    if (p) pipe_write(p, data);
}

char* pm_pipe_read(ProcManager* pm, int pipe_id, char* eb, int esz) {
    Pipe* p = pm_get_pipe(pm, pipe_id, eb, esz);
    return p ? pipe_read(p) : NULL;
}

/* ---- Mutexes ---- */
int pm_create_mutex(ProcManager* pm, const char* name, char* eb, int esz) {
    for (int i = 0; i < pm->n_mutexes; i++)
        if (strcmp(pm->mutexes[i].name, name) == 0)
            { snprintf(eb, esz, "Mutex '%s' already exists", name); return -1; }
    if (pm->n_mutexes >= MAX_MUTEXES) { snprintf(eb, esz, "newmutex: mutex table full"); return -1; }
    int id = pm->next_mutex++;
    mutex_init(&pm->mutexes[pm->n_mutexes++], id, name);
    return id;
}

MutexSim* pm_find_mutex(ProcManager* pm, const char* name_or_id, char* eb, int esz) {
    for (int i = 0; i < pm->n_mutexes; i++)
        if (strcmp(pm->mutexes[i].name, name_or_id) == 0) return &pm->mutexes[i];
    /* try numeric id */
    int mid = 0; int is_num = 1;
    for (int i = 0; name_or_id[i]; i++) if (name_or_id[i] < '0' || name_or_id[i] > '9') { is_num = 0; break; }
    if (is_num) { mid = atoi(name_or_id); for (int i = 0; i < pm->n_mutexes; i++) if (pm->mutexes[i].id == mid) return &pm->mutexes[i]; }
    snprintf(eb, esz, "No mutex named '%s'", name_or_id); return NULL;
}
