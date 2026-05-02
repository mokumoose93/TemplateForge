#include "thread_scheduler.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

std::string thread_state_str(ThreadState s) {
    switch (s) {
        case ThreadState::READY:      return "READY";
        case ThreadState::RUNNING:    return "RUNNING";
        case ThreadState::BLOCKED:    return "BLOCKED";
        case ThreadState::TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

std::string SimThread::status_line() const {
    std::ostringstream ss;
    ss << std::setw(4) << tid << "  "
       << std::setw(4) << owner_pid << "  "
       << std::left << std::setw(12) << thread_state_str(state) << "  "
       << name;
    return ss.str();
}

int Scheduler::create_thread(const std::string& name, int owner_pid) {
    int tid = next_tid_++;
    threads_[tid] = SimThread{tid, name, owner_pid, ThreadState::READY};
    return tid;
}

void Scheduler::terminate_thread(int tid) {
    auto it = threads_.find(tid);
    if (it == threads_.end())
        throw std::invalid_argument("No thread with TID " + std::to_string(tid));
    it->second.state = ThreadState::TERMINATED;
}

void Scheduler::block_thread(int tid, const std::string& /*reason*/) {
    auto it = threads_.find(tid);
    if (it == threads_.end())
        throw std::invalid_argument("No thread with TID " + std::to_string(tid));
    auto& t = it->second;
    if (t.state == ThreadState::READY || t.state == ThreadState::RUNNING)
        t.state = ThreadState::BLOCKED;
}

void Scheduler::unblock_thread(int tid) {
    auto it = threads_.find(tid);
    if (it == threads_.end())
        throw std::invalid_argument("No thread with TID " + std::to_string(tid));
    if (it->second.state == ThreadState::BLOCKED)
        it->second.state = ThreadState::READY;
}

void Scheduler::terminate_all_for_pid(int pid) {
    for (auto& [tid, t] : threads_) {
        if (t.owner_pid == pid && t.state != ThreadState::TERMINATED)
            t.state = ThreadState::TERMINATED;
    }
}

std::optional<int> Scheduler::schedule() {
    // Demote currently RUNNING thread
    for (auto& [tid, t] : threads_)
        if (t.state == ThreadState::RUNNING) t.state = ThreadState::READY;

    // Collect runnable threads
    std::vector<int> runnable;
    for (auto& [tid, t] : threads_)
        if (t.state == ThreadState::READY) runnable.push_back(tid);

    if (runnable.empty()) return std::nullopt;

    if (run_ptr_ >= static_cast<int>(runnable.size())) run_ptr_ = 0;
    int chosen_tid = runnable[run_ptr_];
    threads_[chosen_tid].state = ThreadState::RUNNING;
    run_ptr_ = (run_ptr_ + 1) % static_cast<int>(runnable.size());
    return chosen_tid;
}

std::optional<SimThread*> Scheduler::get_thread(int tid) {
    auto it = threads_.find(tid);
    if (it == threads_.end()) return std::nullopt;
    return &it->second;
}

std::vector<SimThread> Scheduler::list_threads() const {
    std::vector<SimThread> result;
    for (auto& [tid, t] : threads_) result.push_back(t);
    return result;
}

std::vector<SimThread> Scheduler::threads_for_pid(int pid) const {
    std::vector<SimThread> result;
    for (auto& [tid, t] : threads_)
        if (t.owner_pid == pid) result.push_back(t);
    return result;
}
