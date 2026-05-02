#include "signal_handler.h"
#include <stdexcept>

Signal signal_from_str(const std::string& s) {
    std::string upper;
    for (char c : s) upper += static_cast<char>(toupper(c));
    if (upper.substr(0, 3) != "SIG") upper = "SIG" + upper;

    if (upper == "SIGTERM")  return Signal::SIGTERM;
    if (upper == "SIGKILL")  return Signal::SIGKILL;
    if (upper == "SIGINT")   return Signal::SIGINT;
    if (upper == "SIGHUP")   return Signal::SIGHUP;
    if (upper == "SIGUSR1")  return Signal::SIGUSR1;
    if (upper == "SIGUSR2")  return Signal::SIGUSR2;
    if (upper == "SIGCHLD")  return Signal::SIGCHLD;
    if (upper == "SIGSTOP")  return Signal::SIGSTOP;
    if (upper == "SIGCONT")  return Signal::SIGCONT;
    throw std::invalid_argument("Unknown signal: '" + s + "'");
}

std::string signal_to_str(Signal s) {
    switch (s) {
        case Signal::SIGTERM:  return "SIGTERM";
        case Signal::SIGKILL:  return "SIGKILL";
        case Signal::SIGINT:   return "SIGINT";
        case Signal::SIGHUP:   return "SIGHUP";
        case Signal::SIGUSR1:  return "SIGUSR1";
        case Signal::SIGUSR2:  return "SIGUSR2";
        case Signal::SIGCHLD:  return "SIGCHLD";
        case Signal::SIGSTOP:  return "SIGSTOP";
        case Signal::SIGCONT:  return "SIGCONT";
    }
    return "UNKNOWN";
}

std::string default_action_for(Signal s) {
    switch (s) {
        case Signal::SIGTERM:  return "terminate";
        case Signal::SIGKILL:  return "terminate";
        case Signal::SIGINT:   return "terminate";
        case Signal::SIGHUP:   return "terminate";
        case Signal::SIGUSR1:  return "ignore";
        case Signal::SIGUSR2:  return "ignore";
        case Signal::SIGCHLD:  return "ignore";
        case Signal::SIGSTOP:  return "stop";
        case Signal::SIGCONT:  return "continue";
    }
    return "ignore";
}

bool is_uncatchable(Signal s) {
    return s == Signal::SIGKILL || s == Signal::SIGSTOP;
}

bool SignalRegistry::register_handler(int pid, Signal sig, SignalHandler handler) {
    if (is_uncatchable(sig)) return false;
    handlers_[{pid, sig}] = std::move(handler);
    return true;
}

void SignalRegistry::unregister_handler(int pid, Signal sig) {
    handlers_.erase({pid, sig});
}

std::string SignalRegistry::dispatch(int pid, Signal sig) {
    std::string entry = signal_to_str(sig) + " -> PID " + std::to_string(pid);
    std::string msg;

    if (is_uncatchable(sig)) {
        msg = entry + ": " + default_action_for(sig) + " (uncatchable)";
        log_.push_back(msg);
        return msg;
    }

    auto it = handlers_.find({pid, sig});
    if (it != handlers_.end()) {
        std::string result = it->second(pid, sig);
        msg = entry + ": custom handler -> " + result;
    } else {
        msg = entry + ": default action '" + default_action_for(sig) + "'";
    }
    log_.push_back(msg);
    return msg;
}
