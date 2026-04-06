#pragma once
#include "pipe.h"
#include "mutex_sim.h"
#include "thread_scheduler.h"
#include "signal_handler.h"
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <memory>

// ---------------------------------------------------------------------------
// Process state
// ---------------------------------------------------------------------------
enum class ProcessState { RUNNING, SLEEPING, STOPPED, ZOMBIE, TERMINATED };
std::string process_state_str(ProcessState s);

// ---------------------------------------------------------------------------
// Process — a simulated OS process
// ---------------------------------------------------------------------------
struct Process {
    int          pid;
    std::string  name;
    ProcessState state       {ProcessState::RUNNING};
    int          parent_pid  {-1};
    std::vector<int> children;
    int          exit_code   {0};

    std::string status_line() const;
};

// ---------------------------------------------------------------------------
// ProcessManager — central authority for processes, pipes, mutexes
// ---------------------------------------------------------------------------
class ProcessManager {
public:
    ProcessManager();

    // ----- Process lifecycle -------------------------------------------
    int  spawn(const std::string& name, int parent_pid = -1);
    void terminate(int pid);                          // sends SIGTERM
    std::optional<Process*> get_process(int pid);
    std::vector<Process>    list_processes()  const;

    // ----- Signal delivery ---------------------------------------------
    std::string send_signal(int pid, const std::string& signal_str);

    // ----- Pipes -------------------------------------------------------
    int  create_pipe();
    Pipe& get_pipe(int pipe_id);
    void pipe_write(int pipe_id, const std::string& data);
    std::optional<std::string> pipe_read(int pipe_id);
    std::vector<Pipe*> list_pipes();

    // ----- Mutexes -----------------------------------------------------
    int  create_mutex(const std::string& name);
    std::pair<bool,std::string> mutex_acquire(const std::string& name_or_id, int tid);
    std::pair<bool,std::string> mutex_release(const std::string& name_or_id, int tid);
    std::vector<MutexSim*> list_mutexes();

    // ----- Scheduler (public for direct thread commands) ---------------
    Scheduler scheduler;

private:
    std::map<int, Process>             processes_;
    int                                next_pid_   {2};

    std::map<int, Pipe>                pipes_;
    int                                next_pipe_  {1};

    std::map<std::string, MutexSim>    mutexes_;
    int                                next_mutex_ {1};

    SignalRegistry                     signals_;

    MutexSim& find_mutex(const std::string& name_or_id);
    void      do_terminate(int pid, int exit_code = 0);
};
