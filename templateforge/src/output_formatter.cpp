#include "output_formatter.h"
#include <sstream>
#include <iomanip>

OutputFormatter::OutputFormatter(bool use_color) : use_color(use_color) {}

std::string OutputFormatter::col(const std::string& text, const std::string& code) const {
    if (!use_color) return text;
    return code + text + RESET;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_pwd(const std::string& path) const {
    return col(path, BOLD);
}

std::string OutputFormatter::format_prompt(
    const std::string& cwd, const std::string& user) const
{
    std::string u = col(user, (user == "root") ? RED : GREEN);
    std::string p = col(cwd,  CYAN);
    return u + ":" + p + "$ ";
}

// ---------------------------------------------------------------------------
// ls
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_ls(
    const std::vector<std::shared_ptr<Node>>& nodes, bool long_format) const
{
    if (nodes.empty()) return "(empty)";

    if (!long_format) {
        std::string line;
        for (auto& n : nodes) {
            if (!line.empty()) line += "  ";
            if (n->is_dir())
                line += col(n->name + "/", std::string(BLUE) + BOLD);
            else if (n->is_symlink())
                line += col(n->name + "@", CYAN);
            else
                line += n->name;
        }
        return line;
    }

    // Long format
    std::ostringstream ss;
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto& n = nodes[i];
        std::string type_char(1, n->type_label());
        std::string perms   = n->access.permission_string();
        std::string owner   = n->access.owner;

        ss << type_char << perms << "  " << std::left << std::setw(12) << owner << "  ";

        if (n->is_dir()) {
            ss << col(n->name + "/", std::string(BLUE) + BOLD);
        } else if (n->is_symlink()) {
            auto sl = std::dynamic_pointer_cast<SymlinkNode>(n);
            ss << col(n->name, CYAN) << " -> " << col(sl->target_path, YELLOW);
        } else {
            ss << n->name;
        }
        if (i + 1 < nodes.size()) ss << "\n";
    }
    return ss.str();
}

// ---------------------------------------------------------------------------
// tree
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_tree(
    const std::shared_ptr<Node>& node,
    const std::string& prefix,
    bool is_last) const
{
    std::ostringstream ss;
    std::string connector = is_last ? "└── " : "├── ";

    std::string label;
    if (node->is_dir())
        label = col(node->name + "/", std::string(BLUE) + BOLD);
    else if (node->is_symlink()) {
        auto sl = std::dynamic_pointer_cast<SymlinkNode>(node);
        label = col(node->name, CYAN) + " -> " + sl->target_path;
    } else {
        label = node->name;
    }

    if (prefix.empty()) {
        ss << label;
    } else {
        ss << prefix << connector << label;
    }

    if (node->is_dir()) {
        auto dir = std::dynamic_pointer_cast<DirectoryNode>(node);
        auto children = dir->sorted_children();
        std::string child_prefix = prefix + (is_last ? "    " : "│   ");
        for (size_t i = 0; i < children.size(); ++i) {
            bool last_child = (i == children.size() - 1);
            ss << "\n" << format_tree(children[i], child_prefix, last_child);
        }
    }
    return ss.str();
}

// ---------------------------------------------------------------------------
// Variables
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_vars(
    const std::map<std::string, std::string>& vars) const
{
    if (vars.empty())
        return "No variables set. Use 'setvar NAME value' to define one.";
    std::ostringstream ss;
    ss << col("Variables:", BOLD) << "\n";
    for (auto& [name, value] : vars)
        ss << "  " << col("{{" + name + "}}", YELLOW) << "  =  \"" << value << "\"\n";
    std::string result = ss.str();
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// ---------------------------------------------------------------------------
// Processes
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_processes(const std::vector<Process>& procs) const {
    if (procs.empty()) return "No processes.";
    std::ostringstream ss;
    ss << col(std::string(
        std::string("   PID  ") + " PPID  " + "STATE         " + "NAME"), BOLD) << "\n";
    ss << std::string(44, '-') << "\n";
    for (auto& p : procs) ss << p.status_line() << "\n";
    std::string result = ss.str();
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// ---------------------------------------------------------------------------
// Threads
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_threads(const std::vector<SimThread>& threads) const {
    if (threads.empty()) return "No threads.";
    std::ostringstream ss;
    ss << col("   TID   PID  STATE         NAME", BOLD) << "\n";
    ss << std::string(44, '-') << "\n";
    for (auto& t : threads) ss << t.status_line() << "\n";
    std::string result = ss.str();
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// ---------------------------------------------------------------------------
// Pipes
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_pipes(const std::vector<Pipe*>& pipes) const {
    if (pipes.empty()) return "No pipes. Use 'mkpipe' to create one.";
    std::ostringstream ss;
    ss << col("  ID  BUFFERED", BOLD) << "\n";
    ss << std::string(16, '-') << "\n";
    for (auto* p : pipes)
        ss << std::setw(4) << p->id() << "  " << std::setw(8) << p->size() << "\n";
    std::string result = ss.str();
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// ---------------------------------------------------------------------------
// Mutexes
// ---------------------------------------------------------------------------
std::string OutputFormatter::format_mutexes(const std::vector<MutexSim*>& mtxs) const {
    if (mtxs.empty()) return "No mutexes. Use 'newmutex <name>' to create one.";
    std::ostringstream ss;
    ss << col(std::string("  ID  ") + std::string(std::string("NAME") + "                STATUS"), BOLD) << "\n";
    ss << std::string(50, '-') << "\n";
    for (auto* m : mtxs)
        ss << std::setw(4) << m->id() << "  "
           << std::left << std::setw(20) << m->name() << "  "
           << m->status_str() << "\n";
    std::string result = ss.str();
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// ---------------------------------------------------------------------------
// Feedback
// ---------------------------------------------------------------------------
std::string OutputFormatter::ok (const std::string& msg) const { return col(msg, GREEN);  }
std::string OutputFormatter::err(const std::string& msg) const { return col("Error: " + msg, RED); }
std::string OutputFormatter::inf(const std::string& msg) const { return col(msg, YELLOW); }
