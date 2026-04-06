#pragma once
#include <string>
#include <optional>

// ---------------------------------------------------------------------------
// MutexSim — simulated mutual-exclusion lock for SimThreads
// ---------------------------------------------------------------------------
class MutexSim {
public:
    MutexSim(int id, const std::string& name);

    // Returns {true, msg} on success or {false, msg} if already locked.
    std::pair<bool, std::string> acquire(int tid);
    std::pair<bool, std::string> release(int tid);

    bool                    is_locked()  const { return locked_; }
    std::optional<int>      owner_tid()  const;
    int                     id()         const { return id_; }
    const std::string&      name()       const { return name_; }
    std::string             status_str() const;

private:
    int         id_;
    std::string name_;
    bool        locked_ {false};
    int         owner_  {-1};
};
