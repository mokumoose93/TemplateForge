#pragma once
#include "nodes.h"
#include "process_manager.h"
#include <string>
#include <vector>
#include <map>

// ---------------------------------------------------------------------------
// OutputFormatter — the View in the MVC design
// Converts model data into human-readable strings.
// ---------------------------------------------------------------------------
class OutputFormatter {
public:
    explicit OutputFormatter(bool use_color = true);

    // Navigation
    std::string format_pwd(const std::string& path) const;
    std::string format_prompt(const std::string& cwd, const std::string& user) const;

    // Filesystem
    std::string format_ls(const std::vector<std::shared_ptr<Node>>& nodes,
                          bool long_format = false) const;
    std::string format_tree(const std::shared_ptr<Node>& node,
                            const std::string& prefix  = "",
                            bool               is_last = true) const;

    // Variables
    std::string format_vars(const std::map<std::string,std::string>& vars) const;

    // Processes / threads / pipes / mutexes
    std::string format_processes(const std::vector<Process>& procs)   const;
    std::string format_threads  (const std::vector<SimThread>& threads) const;
    std::string format_pipes    (const std::vector<Pipe*>& pipes)      const;
    std::string format_mutexes  (const std::vector<MutexSim*>& mtxs)  const;

    // Feedback
    std::string ok (const std::string& msg) const;   // green
    std::string err(const std::string& msg) const;   // red
    std::string inf(const std::string& msg) const;   // yellow

    bool use_color;

private:
    std::string col(const std::string& text, const std::string& code) const;

    // ANSI codes
    static constexpr const char* RESET   = "\033[0m";
    static constexpr const char* BOLD    = "\033[1m";
    static constexpr const char* RED     = "\033[31m";
    static constexpr const char* GREEN   = "\033[32m";
    static constexpr const char* YELLOW  = "\033[33m";
    static constexpr const char* BLUE    = "\033[34m";
    static constexpr const char* CYAN    = "\033[36m";
};
