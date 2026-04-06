#include "process_manager.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

std::string process_state_str(ProcessState s) {
    switch (s) {
        case ProcessState::RUNNING:    return "RUNNING";
        case ProcessState::SLEEPING:   return "SLEEPING";
        case ProcessState::STOPPED:    return "STOPPED";
        case ProcessState::ZOMBIE:     return "ZOMBIE";
        case ProcessState::TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

std::string Process::status_line() const {
    std::ostringstream ss;
    std::string ppid_s = (parent_pid >= 0) ? std::to_string(parent_pid) : "-";
    ss << std::setw(4) << pid    << "  "
       << std::setw(4) << ppid_s << "  "
       << std::left << std::setw(12) << process_state_str(state) << "  "
       << name;
    return ss.str();
}

// ---------------------------------------------------------------------------
// Constructor  — create init (PID 1)
// ---------------------------------------------------------------------------
ProcessManager::ProcessManager() {
    Process init;
    init.pid   = 1;
    init.name  = "init";
    init.state = ProcessState::RUNNING;
    processes_[1] = init;
    next_pid_ = 2;

    // init gets a main thread
    scheduler.create_thread("main", 1);

    // Register a default SIGTERM handler for init
    signals_.register_handler(1, Signal::SIGTERM,
        [this](int pid, Signal) -> std::string {
            do_terminate(pid, 0);
            return "exited cleanly";
        });
}

// ---------------------------------------------------------------------------
// Process lifecycle
// ---------------------------------------------------------------------------
int ProcessManager::spawn(const std::string& name, int parent_pid) {
    if (parent_pid >= 0 && processes_.count(parent_pid) == 0)
        throw std::invalid_argument(
            "spawn: parent PID " + std::to_string(parent_pid) + " does not exist");

    int pid = next_pid_++;
    Process proc;
    proc.pid        = pid;
    proc.name       = name;
    proc.parent_pid = parent_pid;
    proc.state      = ProcessState::RUNNING;
    processes_[pid] = proc;

    if (parent_pid >= 0)
        processes_[parent_pid].children.push_back(pid);

    // Every new process gets a main thread
    scheduler.create_thread("main", pid);

    // Register a default SIGTERM handler
    signals_.register_handler(pid, Signal::SIGTERM,
        [this, pid](int p, Signal) -> std::string {
            do_terminate(p, 0);
            return "process " + std::to_string(pid) + " exited cleanly";
        });

    return pid;
}

void ProcessManager::do_terminate(int pid, int exit_code) {
    auto it = processes_.find(pid);
    if (it == processes_.end()) return;
    it->second.state     = ProcessState::ZOMBIE;
    it->second.exit_code = exit_code;
    scheduler.terminate_all_for_pid(pid);
}

void ProcessManager::terminate(int pid) {
    if (processes_.count(pid) == 0)
        throw std::invalid_argument("kill: no such process: PID " + std::to_string(pid));
    if (pid == 1)
        throw std::invalid_argument("kill: cannot terminate init (PID 1)");
    send_signal(pid, "SIGTERM");
}

std::optional<Process*> ProcessManager::get_process(int pid) {
    auto it = processes_.find(pid);
    if (it == processes_.end()) return std::nullopt;
    return &it->second;
}

std::vector<Process> ProcessManager::list_processes() const {
    std::vector<Process> result;
    for (auto& [pid, p] : processes_) result.push_back(p);
    return result;
}

// ---------------------------------------------------------------------------
// Signal delivery
// ---------------------------------------------------------------------------
std::string ProcessManager::send_signal(int pid, const std::string& signal_str) {
    if (processes_.count(pid) == 0)
        throw std::invalid_argument("signal: no such process: PID " + std::to_string(pid));

    auto& proc = processes_[pid];
    if (proc.state == ProcessState::ZOMBIE || proc.state == ProcessState::TERMINATED)
        return "PID " + std::to_string(pid) + " is already dead — signal not delivered";

    Signal sig = signal_from_str(signal_str);

    if (sig == Signal::SIGKILL) {
        do_terminate(pid, -9);
        return "PID " + std::to_string(pid) + " killed with SIGKILL";
    }
    if (sig == Signal::SIGSTOP) {
        proc.state = ProcessState::STOPPED;
        return "PID " + std::to_string(pid) + " stopped";
    }
    if (sig == Signal::SIGCONT) {
        if (proc.state == ProcessState::STOPPED)
            proc.state = ProcessState::RUNNING;
        return "PID " + std::to_string(pid) + " continued";
    }

    return signals_.dispatch(pid, sig);
}

// ---------------------------------------------------------------------------
// Pipes
// ---------------------------------------------------------------------------
int ProcessManager::create_pipe() {
    int id = next_pipe_++;
    pipes_.emplace(id, Pipe(id));
    return id;
}

Pipe& ProcessManager::get_pipe(int pipe_id) {
    auto it = pipes_.find(pipe_id);
    if (it == pipes_.end())
        throw std::invalid_argument("No pipe with ID " + std::to_string(pipe_id));
    return it->second;
}

void ProcessManager::pipe_write(int pipe_id, const std::string& data) {
    get_pipe(pipe_id).write(data);
}

std::optional<std::string> ProcessManager::pipe_read(int pipe_id) {
    return get_pipe(pipe_id).read();
}

std::vector<Pipe*> ProcessManager::list_pipes() {
    std::vector<Pipe*> result;
    for (auto& [id, p] : pipes_) result.push_back(&p);
    return result;
}

// ---------------------------------------------------------------------------
// Mutexes
// ---------------------------------------------------------------------------
int ProcessManager::create_mutex(const std::string& name) {
    if (mutexes_.count(name))
        throw std::invalid_argument("Mutex '" + name + "' already exists");
    int id = next_mutex_++;
    mutexes_.emplace(name, MutexSim(id, name));
    return id;
}

MutexSim& ProcessManager::find_mutex(const std::string& name_or_id) {
    // Try by name
    auto it = mutexes_.find(name_or_id);
    if (it != mutexes_.end()) return it->second;
    // Try by numeric ID
    try {
        int mid = std::stoi(name_or_id);
        for (auto& [n, m] : mutexes_)
            if (m.id() == mid) return m;
    } catch (...) {}
    throw std::invalid_argument("No mutex named '" + name_or_id + "'");
}

std::pair<bool,std::string> ProcessManager::mutex_acquire(
    const std::string& name_or_id, int tid) {
    return find_mutex(name_or_id).acquire(tid);
}

std::pair<bool,std::string> ProcessManager::mutex_release(
    const std::string& name_or_id, int tid) {
    return find_mutex(name_or_id).release(tid);
}

std::vector<MutexSim*> ProcessManager::list_mutexes() {
    std::vector<MutexSim*> result;
    for (auto& [n, m] : mutexes_) result.push_back(&m);
    return result;
}
