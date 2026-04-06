#pragma once
#include <string>
#include <map>
#include <vector>
#include <optional>

// ---------------------------------------------------------------------------
// Thread state
// ---------------------------------------------------------------------------
enum class ThreadState {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
};

std::string thread_state_str(ThreadState s);

// ---------------------------------------------------------------------------
// SimThread — a simulated user-space thread
// ---------------------------------------------------------------------------
struct SimThread {
    int         tid;
    std::string name;
    int         owner_pid;
    ThreadState state {ThreadState::READY};

    std::string status_line() const;
};

// ---------------------------------------------------------------------------
// Scheduler — cooperative round-robin scheduler over SimThreads
// ---------------------------------------------------------------------------
class Scheduler {
public:
    int  create_thread(const std::string& name, int owner_pid);
    void terminate_thread(int tid);
    void block_thread(int tid, const std::string& reason = "");
    void unblock_thread(int tid);
    void terminate_all_for_pid(int pid);

    // Advance one tick: demote current RUNNING -> READY, pick next READY.
    // Returns the newly RUNNING thread, or nullopt if nothing runnable.
    std::optional<int> schedule();

    std::optional<SimThread*> get_thread(int tid);
    std::vector<SimThread>    list_threads() const;
    std::vector<SimThread>    threads_for_pid(int pid) const;

private:
    std::map<int, SimThread> threads_;
    int next_tid_ {1};
    int run_ptr_  {0};
};
