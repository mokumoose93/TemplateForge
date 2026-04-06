#include "command_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// parse() — tokenise respecting single and double quotes
// ---------------------------------------------------------------------------
std::vector<std::string> parse(const std::string& raw) {
    std::vector<std::string> tokens;
    std::string current;
    char in_quote = 0;

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (in_quote) {
            if (c == in_quote) in_quote = 0;
            else               current += c;
        } else if (c == '\'' || c == '"') {
            in_quote = c;
        } else if (c == ' ' || c == '\t') {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool has_flag(const std::vector<std::string>& args, const std::string& flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

static std::vector<std::string> non_flag_args(const std::vector<std::string>& args) {
    std::vector<std::string> result;
    for (auto& a : args) if (a.empty() || a[0] != '-') result.push_back(a);
    return result;
}

static std::string join(const std::vector<std::string>& v,
                        size_t start = 0, const std::string& sep = " ") {
    std::string result;
    for (size_t i = start; i < v.size(); ++i) {
        if (i > start) result += sep;
        result += v[i];
    }
    return result;
}

// ---------------------------------------------------------------------------
// dispatch()
// ---------------------------------------------------------------------------
std::string dispatch(const std::vector<std::string>& tokens, AppContext& ctx) {
    if (tokens.empty()) return "";
    const std::string& cmd = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    auto& fs   = ctx.fs;
    auto& tm   = ctx.tm;
    auto& vm   = ctx.vm;
    auto& pm   = ctx.pm;
    auto& view = ctx.view;

    try {
        // ---- Navigation ------------------------------------------------
        if (cmd == "pwd")
            return view.format_pwd(fs.pwd());

        if (cmd == "cd") {
            if (args.empty()) return view.err("cd: missing operand");
            fs.cd(args[0]);
            return "";
        }

        if (cmd == "ls") {
            bool lng = has_flag(args, "-l");
            auto paths = non_flag_args(args);
            std::string path = paths.empty() ? "" : paths[0];
            return view.format_ls(fs.ls(path), lng);
        }

        if (cmd == "tree") {
            std::string path = args.empty() ? "" : args[0];
            auto node = path.empty()
                ? std::static_pointer_cast<Node>(fs.cwd)
                : fs.get_node(path);
            if (!node) return view.err("tree: no such path: '" + path + "'");
            return view.format_tree(node);
        }

        // ---- File / directory manipulation -----------------------------
        if (cmd == "mkdir") {
            if (args.empty()) return view.err("mkdir: missing operand");
            std::string out;
            for (auto& p : args) {
                fs.mkdir(p);
                out += view.ok("Directory '" + p + "' created") + "\n";
            }
            if (!out.empty() && out.back() == '\n') out.pop_back();
            return out;
        }

        if (cmd == "touch") {
            if (args.empty()) return view.err("touch: missing operand");
            std::string out;
            for (auto& p : args) {
                fs.touch(p);
                out += view.ok("File '" + p + "' created/updated") + "\n";
            }
            if (!out.empty() && out.back() == '\n') out.pop_back();
            return out;
        }

        if (cmd == "rm") {
            if (args.empty()) return view.err("rm: missing operand");
            bool rec = has_flag(args, "-r") || has_flag(args, "-rf");
            auto paths = non_flag_args(args);
            std::string out;
            for (auto& p : paths) {
                fs.rm(p, rec);
                out += view.ok("Removed '" + p + "'") + "\n";
            }
            if (!out.empty() && out.back() == '\n') out.pop_back();
            return out;
        }

        if (cmd == "mv") {
            if (args.size() < 2) return view.err("mv: missing destination operand");
            fs.mv(args[0], args[1]);
            return view.ok("Moved '" + args[0] + "' -> '" + args[1] + "'");
        }

        if (cmd == "cp") {
            if (args.size() < 2) return view.err("cp: missing destination operand");
            fs.cp(args[0], args[1]);
            return view.ok("Copied '" + args[0] + "' -> '" + args[1] + "'");
        }

        if (cmd == "cat") {
            if (args.empty()) return view.err("cat: missing operand");
            return fs.cat(args[0]);
        }

        if (cmd == "write") {
            if (args.size() < 2) return view.err("write: usage: write <path> <content>");
            std::string content = join(args, 1);
            auto node = fs.get_node(args[0]);
            if (!node) fs.touch(args[0], content);
            else       fs.write_file(args[0], content);
            return view.ok("Written to '" + args[0] + "'");
        }

        if (cmd == "ln") {
            if (args.size() < 2) return view.err("ln: usage: ln <target> <link_name>");
            fs.ln(args[0], args[1]);
            return view.ok("Symlink '" + args[1] + "' -> '" + args[0] + "'");
        }

        // ---- Access control --------------------------------------------
        if (cmd == "chmod") {
            if (args.size() < 2) return view.err("chmod: usage: chmod <octal> <path>");
            fs.chmod(args[1], args[0]);
            return view.ok("Permissions of '" + args[1] + "' set to " + args[0]);
        }

        if (cmd == "chown") {
            if (args.size() < 2) return view.err("chown: usage: chown <owner> <path>");
            fs.chown(args[1], args[0]);
            return view.ok("Owner of '" + args[1] + "' changed to '" + args[0] + "'");
        }

        if (cmd == "whoami") return fs.current_user;

        if (cmd == "su") {
            if (args.empty()) return view.err("su: missing username");
            fs.current_user = args[0];
            return view.ok("Switched to user '" + args[0] + "'");
        }

        // ---- Variable management --------------------------------------
        if (cmd == "setvar") {
            if (args.size() < 2) return view.err("setvar: usage: setvar <NAME> <value>");
            std::string val = join(args, 1);
            vm.set(args[0], val);
            return view.ok("$" + args[0] + " = \"" + val + "\"");
        }

        if (cmd == "unsetvar") {
            if (args.empty()) return view.err("unsetvar: missing variable name");
            if (vm.unset(args[0])) return view.ok("Variable '" + args[0] + "' removed");
            return view.err("unsetvar: '" + args[0] + "' is not set");
        }

        if (cmd == "vars") return view.format_vars(vm.all());

        // ---- Template management --------------------------------------
        if (cmd == "define") {
            if (args.empty()) return view.err("define: usage: define <template_name>");
            return "::DEFINE:" + args[0];   // signal REPL to enter define mode
        }

        if (cmd == "templates") {
            auto list = tm.list();
            if (list.empty()) return "No templates defined. Use 'define <name>' to create one.";
            std::string out;
            for (auto* t : list) out += "  " + t->summary() + "\n";
            if (!out.empty() && out.back() == '\n') out.pop_back();
            return out;
        }

        if (cmd == "rmtemplate") {
            if (args.empty()) return view.err("rmtemplate: missing template name");
            if (tm.remove(args[0])) return view.ok("Template '" + args[0] + "' removed");
            return view.err("Template '" + args[0] + "' not found");
        }

        if (cmd == "build") {
            if (args.size() < 2) return view.err("build: usage: build <template> <path>");
            auto created = tm.build(args[0], args[1], fs, vm);
            std::string out = view.ok("Built template '" + args[0] + "' at '" + args[1] + "'") + "\n";
            for (auto& p : created) out += "  + " + p + "\n";
            if (!out.empty() && out.back() == '\n') out.pop_back();
            return out;
        }

        // ---- Process management --------------------------------------
        if (cmd == "ps") return view.format_processes(pm.list_processes());

        if (cmd == "spawn") {
            std::string name = args.empty() ? "process" : args[0];
            int ppid = (args.size() > 1) ? std::stoi(args[1]) : -1;
            int pid  = pm.spawn(name, ppid);
            return view.ok("Process '" + name + "' spawned with PID " + std::to_string(pid));
        }

        if (cmd == "kill") {
            if (args.empty()) return view.err("kill: missing PID");
            pm.terminate(std::stoi(args[0]));
            return view.ok("Process " + args[0] + " terminated");
        }

        if (cmd == "signal") {
            if (args.size() < 2) return view.err("signal: usage: signal <pid> <SIGNAL>");
            std::string result = pm.send_signal(std::stoi(args[0]), args[1]);
            return view.ok(result);
        }

        // ---- Pipes ---------------------------------------------------
        if (cmd == "mkpipe") {
            int id = pm.create_pipe();
            return view.ok("Pipe created with ID " + std::to_string(id));
        }

        if (cmd == "pipes") return view.format_pipes(pm.list_pipes());

        if (cmd == "pwrite") {
            if (args.size() < 2) return view.err("pwrite: usage: pwrite <id> <data>");
            pm.pipe_write(std::stoi(args[0]), join(args, 1));
            return view.ok("Wrote to pipe " + args[0]);
        }

        if (cmd == "pread") {
            if (args.empty()) return view.err("pread: missing pipe_id");
            auto data = pm.pipe_read(std::stoi(args[0]));
            return data ? *data : "(pipe is empty)";
        }

        // ---- Threads -------------------------------------------------
        if (cmd == "threads") return view.format_threads(pm.scheduler.list_threads());

        if (cmd == "newthread") {
            if (args.size() < 2) return view.err("newthread: usage: newthread <pid> <name>");
            int tid = pm.scheduler.create_thread(args[1], std::stoi(args[0]));
            return view.ok("Thread '" + args[1] + "' (TID " + std::to_string(tid) + ") created");
        }

        if (cmd == "killthread") {
            if (args.empty()) return view.err("killthread: missing TID");
            pm.scheduler.terminate_thread(std::stoi(args[0]));
            return view.ok("Thread " + args[0] + " terminated");
        }

        if (cmd == "schedule") {
            auto next = pm.scheduler.schedule();
            if (!next) return view.inf("No runnable threads.");
            return view.ok("Scheduler: TID " + std::to_string(*next) + " is now RUNNING");
        }

        // ---- Mutexes -------------------------------------------------
        if (cmd == "mutexes") return view.format_mutexes(pm.list_mutexes());

        if (cmd == "newmutex") {
            if (args.empty()) return view.err("newmutex: missing mutex name");
            int mid = pm.create_mutex(args[0]);
            return view.ok("Mutex '" + args[0] + "' created (ID " + std::to_string(mid) + ")");
        }

        if (cmd == "acquire") {
            if (args.empty()) return view.err("acquire: missing mutex name/id");
            int tid = (args.size() > 1) ? std::stoi(args[1]) : 0;
            auto [ok, msg] = pm.mutex_acquire(args[0], tid);
            return ok ? view.ok(msg) : view.err(msg);
        }

        if (cmd == "release") {
            if (args.empty()) return view.err("release: missing mutex name/id");
            int tid = (args.size() > 1) ? std::stoi(args[1]) : 0;
            auto [ok, msg] = pm.mutex_release(args[0], tid);
            return ok ? view.ok(msg) : view.err(msg);
        }

        // ---- Misc ----------------------------------------------------
        if (cmd == "help")  return help_text(args.empty() ? "" : args[0]);
        if (cmd == "clear") return "::CLEAR";
        if (cmd == "exit" || cmd == "quit") return "::EXIT";

        return view.err("'" + cmd + "': command not found  (type 'help' for commands)");

    } catch (const std::invalid_argument& e) {
        return view.err(e.what());
    } catch (const std::out_of_range& e) {
        return view.err(std::string("argument out of range: ") + e.what());
    } catch (const std::exception& e) {
        return view.err(e.what());
    }
}

// ---------------------------------------------------------------------------
// help_text()
// ---------------------------------------------------------------------------
std::string help_text(const std::string& topic) {
    static const std::vector<std::pair<std::string,std::string>> sections = {
    {"navigation",
R"(Navigation
  pwd                        Print working directory
  ls [-l] [path]             List directory contents
  cd <path>                  Change directory
  tree [path]                Show directory tree)"},

    {"files",
R"(File & Directory Commands
  mkdir <path> [...]         Create directory (parents auto-created)
  touch <path> [...]         Create or update a file
  rm [-r] <path> [...]       Remove file or directory (-r for recursive)
  mv <src> <dst>             Move or rename
  cp <src> <dst>             Copy (recursive for directories)
  cat <path>                 Display file content
  write <path> <text>        Write text to a file
  ln <target> <link>         Create a symbolic link)"},

    {"access",
R"(Access Control
  whoami                     Show current user
  su <user>                  Switch user
  chmod <octal> <path>       Change permissions  (e.g. chmod 755 /bin)
  chown <owner> <path>       Change owner (root only))"},

    {"variables",
R"(Variables
  setvar <NAME> <value>      Define a template variable
  unsetvar <NAME>            Remove a variable
  vars                       List all variables)"},

    {"templates",
R"(Templates
  define <name>              Start interactive template definition
  templates                  List all defined templates
  rmtemplate <name>          Remove a template
  build <name> <path>        Instantiate a template at a path)"},

    {"processes",
R"(Processes
  ps                         List all processes
  spawn <name> [ppid]        Spawn a new process
  kill <pid>                 Terminate a process (SIGTERM)
  signal <pid> <SIG>         Send a signal to a process
                             Signals: SIGTERM SIGKILL SIGINT SIGHUP SIGUSR1 SIGUSR2)"},

    {"ipc",
R"(IPC / Pipes
  mkpipe                     Create a new FIFO pipe
  pipes                      List all pipes
  pwrite <id> <data>         Write data to a pipe
  pread <id>                 Read one item from a pipe)"},

    {"threads",
R"(Threads
  threads                    List all simulated threads
  newthread <pid> <name>     Create a thread under a process
  killthread <tid>           Terminate a thread
  schedule                   Advance scheduler by one tick)"},

    {"mutexes",
R"(Mutexes
  mutexes                    List all mutexes
  newmutex <name>            Create a named mutex
  acquire <name> [tid]       Acquire a mutex
  release <name> [tid]       Release a mutex)"},
    };

    if (!topic.empty()) {
        for (auto& [key, text] : sections)
            if (key == topic) return text;
        return "No help topic '" + topic + "'. Topics: navigation files access variables templates processes ipc threads mutexes";
    }

    std::string out = "TemplateForge — Simulated File System Shell\n";
    out += std::string(44, '=') + "\n\n";
    for (auto& [key, text] : sections) out += text + "\n\n";
    return out;
}
