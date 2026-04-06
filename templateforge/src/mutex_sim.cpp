#include "mutex_sim.h"
#include <sstream>

MutexSim::MutexSim(int id, const std::string& name)
    : id_(id), name_(name) {}

std::pair<bool, std::string> MutexSim::acquire(int tid) {
    if (locked_) {
        if (owner_ == tid) {
            return {false, "Mutex '" + name_ + "': TID " + std::to_string(tid) +
                           " already holds this lock (deadlock avoided)"};
        }
        return {false, "Mutex '" + name_ + "': locked by TID " + std::to_string(owner_) +
                       " — TID " + std::to_string(tid) + " would block"};
    }
    locked_ = true;
    owner_  = tid;
    return {true, "Mutex '" + name_ + "' acquired by TID " + std::to_string(tid)};
}

std::pair<bool, std::string> MutexSim::release(int tid) {
    if (!locked_)
        return {false, "Mutex '" + name_ + "': not currently locked"};
    if (owner_ != tid)
        return {false, "Mutex '" + name_ + "': owned by TID " + std::to_string(owner_) +
                       ", not TID " + std::to_string(tid)};
    locked_ = false;
    owner_  = -1;
    return {true, "Mutex '" + name_ + "' released by TID " + std::to_string(tid)};
}

std::optional<int> MutexSim::owner_tid() const {
    if (!locked_) return std::nullopt;
    return owner_;
}

std::string MutexSim::status_str() const {
    if (locked_) return "LOCKED (owner: TID " + std::to_string(owner_) + ")";
    return "UNLOCKED";
}
