#pragma once
#include <string>
#include <map>
#include <functional>
#include <vector>
#include <set>

// ---------------------------------------------------------------------------
// Signal definitions
// ---------------------------------------------------------------------------
enum class Signal {
    SIGTERM,
    SIGKILL,
    SIGINT,
    SIGHUP,
    SIGUSR1,
    SIGUSR2,
    SIGCHLD,
    SIGSTOP,
    SIGCONT
};

Signal      signal_from_str(const std::string& s);   // throws std::invalid_argument
std::string signal_to_str(Signal s);

std::string default_action_for(Signal s);             // "terminate"|"stop"|"ignore"|"continue"
bool        is_uncatchable(Signal s);                 // SIGKILL, SIGSTOP

// ---------------------------------------------------------------------------
// SignalRegistry — maps (pid, signal) to custom handlers
// ---------------------------------------------------------------------------
using SignalHandler = std::function<std::string(int pid, Signal sig)>;

class SignalRegistry {
public:
    // Register a custom handler. Returns false for uncatchable signals.
    bool register_handler(int pid, Signal sig, SignalHandler handler);
    void unregister_handler(int pid, Signal sig);

    // Dispatch signal to pid, running custom handler or default action.
    std::string dispatch(int pid, Signal sig);

    const std::vector<std::string>& log() const { return log_; }

private:
    std::map<std::pair<int,Signal>, SignalHandler> handlers_;
    std::vector<std::string>                        log_;
};
