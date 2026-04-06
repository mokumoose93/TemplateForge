# TemplateForge — Technical Design Document

## Overview

TemplateForge is an interactive command-line application written in **C++17** that simulates a hierarchical Unix-like file system entirely in memory. Its primary purpose is to act as a smart project template generator, while also serving as an educational tool for operating-system concepts such as file systems, access control, process management, inter-process communication, threading, and signals.

---

## Architecture

TemplateForge follows a **Model-View-Controller (MVC)** architecture adapted for a command-line application.

```
┌─────────────────────────────────────────────────────────┐
│                         User                            │
│                  (types commands)                       │
└──────────────────────────┬──────────────────────────────┘
                           │ raw input string
                           ▼
┌──────────────────────────────────────────────────────────┐
│                     CONTROLLER                           │
│  parse()         — tokenises the input line              │
│  dispatch()      — routes to the correct subsystem       │
└───────┬──────────────────┬──────────────────┬────────────┘
        │                  │                  │
        ▼                  ▼                  ▼
┌────────────┐   ┌──────────────────┐  ┌────────────────┐
│   MODEL    │   │   TEMPLATE /     │  │    PROCESS     │
│            │   │   VARIABLE       │  │    SUBSYSTEM   │
│ FileSystem │   │   TemplateManager│  │ ProcessManager │
│ Model      │   │ VariableManager  │  │ Scheduler      │
│            │   │                  │  │ SignalRegistry  │
│ Nodes:     │   │                  │  │ Pipe  / Mutex  │
│  FileNode  │   │                  │  │                │
│  DirNode   │   │                  │  │                │
│  Symlink   │   │                  │  │                │
└────────────┘   └──────────────────┘  └────────────────┘
        │                  │                  │
        └──────────────────┴──────────────────┘
                           │ formatted strings
                           ▼
              ┌───────────────────────┐
              │         VIEW          │
              │   OutputFormatter     │
              │  (ANSI colour, tables)│
              └───────────────────────┘
                           │
                           ▼
                      Terminal / stdout
```

---

## Source Layout

```
templateforge/
├── Makefile
├── include/              ← Header files (interfaces)
│   ├── nodes.h
│   ├── filesystem.h
│   ├── template_manager.h
│   ├── variable_manager.h
│   ├── command_parser.h
│   ├── output_formatter.h
│   ├── pipe.h
│   ├── mutex_sim.h
│   ├── thread_scheduler.h
│   ├── signal_handler.h
│   └── process_manager.h
└── src/                  ← Implementation files
    ├── main.cpp
    ├── nodes.cpp
    ├── filesystem.cpp
    ├── template_manager.cpp
    ├── variable_manager.cpp
    ├── command_parser.cpp
    ├── output_formatter.cpp
    ├── pipe.cpp
    ├── mutex_sim.cpp
    ├── thread_scheduler.cpp
    ├── signal_handler.cpp
    └── process_manager.cpp
```

---

## Component Reference

### 1. Node Types (`nodes.h / nodes.cpp`)

Every element in the simulated file system is a `Node`. Three concrete types exist:

| Class | Represents | Key field |
|---|---|---|
| `FileNode` | A regular file | `std::string content` |
| `DirectoryNode` | A directory | `std::map<string, shared_ptr<Node>> children` |
| `SymlinkNode` | A symbolic link | `std::string target_path` |

All nodes inherit from `Node` (which inherits `std::enable_shared_from_this<Node>`) and carry an `AccessMetadata` struct with Unix-style permission bits:

```
owner_perms / group_perms / other_perms  (int bitmask: READ=4, WRITE=2, EXECUTE=1)
```

The `parent` pointer is stored as a `std::weak_ptr<Node>` to avoid reference cycles that would prevent nodes from being freed.

### 2. FileSystemModel (`filesystem.h / filesystem.cpp`)

The central in-memory tree. All operations work on `shared_ptr<Node>` handles.

**Key methods:**

| Method | Description |
|---|---|
| `pwd()` | Walk parent pointers from `cwd` to `root`, build `/path/string` |
| `cd(path)` | Resolve path, check EXECUTE permission, update `cwd` |
| `ls(path)` | Read children of target directory (requires READ permission) |
| `mkdir(path)` | Recursively create directories (p-flag semantics) |
| `touch(path)` | Create or overwrite a file |
| `rm(path, recursive)` | Remove node; non-empty dirs require `recursive=true` |
| `mv(src, dst)` | Detach from src parent, attach to dst (rename if dst is not a dir) |
| `cp(src, dst)` | Deep-copy subtree via `deep_copy()` |
| `ln(target, link)` | Create a `SymlinkNode` pointing to `target` |
| `chmod(path, octal)` | Change permission bits; owner or root only |
| `chown(path, owner)` | Change owner; root only |

**Path resolution** (`resolve_path`):
- Absolute paths (`/a/b`) start from root.
- Relative paths start from `cwd`.
- Segments `.` are skipped; `..` pops the last segment.
- Symlinks in intermediate path components are automatically dereferenced up to 40 hops (prevents infinite loops).

**Permission checking** follows Unix semantics:
- `root` bypasses all checks.
- Otherwise, if `current_user == node.access.owner` → use `owner_perms`; else use `other_perms`.

### 3. TemplateManager (`template_manager.h / template_manager.cpp`)

Stores named `TemplateDefinition` objects, each containing a list of `TemplateEntry` records.

**Defining a template:**

```
define webapp
  src/
  src/components/
  src/main.cpp::// {{PROJECT}} entry point
  README.md::# {{PROJECT}}
end
```

Paths ending with `/` become directories; others become files. The `::` separator provides inline file content. All paths and content may contain `{{VAR_NAME}}` placeholders.

**Building a template:**

```
build webapp /projects/myapp
```

The `build` method iterates over all entries, substitutes variables via `VariableManager::substitute()`, then calls `FileSystemModel::mkdir()` or `FileSystemModel::touch()` for each entry.

### 4. VariableManager (`variable_manager.h / variable_manager.cpp`)

A simple `std::map<string,string>` store with `std::regex`-powered substitution.

```cpp
vm.set("PROJECT", "myapp");
vm.substitute("src/{{PROJECT}}/main.cpp");  // -> "src/myapp/main.cpp"
```

Variable names must match `[A-Za-z_][A-Za-z0-9_]*`. Unresolved placeholders are left unchanged.

### 5. Process Manager (`process_manager.h / process_manager.cpp`)

Manages simulated OS processes. At startup, PID 1 (`init`) is bootstrapped with a default SIGTERM handler and a main thread.

**Process lifecycle:**
1. `spawn(name, parent_pid)` — allocates next PID, registers a SIGTERM handler, creates a main thread.
2. `terminate(pid)` — sends SIGTERM (calls the registered handler).
3. `do_terminate(pid, exit_code)` — sets state to ZOMBIE, kills all threads for that PID.

**Process states:** `RUNNING → STOPPED → RUNNING` (via SIGSTOP/SIGCONT), `RUNNING → ZOMBIE` (on termination).

### 6. Pipe / IPC (`pipe.h / pipe.cpp`)

A `Pipe` is a FIFO buffer backed by `std::deque<std::string>`. Processes communicate by ID:

```
mkpipe           → creates pipe, prints ID
pwrite 1 hello   → enqueues "hello" to pipe 1
pread 1          → dequeues and prints "hello"
```

The REPL also supports a basic `cmd1 | cmd2` syntax: the left command's output is split by line and written into a transient pipe.

### 7. Signal Handler (`signal_handler.h / signal_handler.cpp`)

`SignalRegistry` maps `(pid, Signal)` pairs to `std::function<std::string(int,Signal)>` handlers.

Supported signals and their defaults:

| Signal | Default action |
|---|---|
| SIGTERM | terminate |
| SIGKILL | terminate (uncatchable) |
| SIGINT | terminate |
| SIGHUP | terminate |
| SIGUSR1 | ignore |
| SIGUSR2 | ignore |
| SIGCHLD | ignore |
| SIGSTOP | stop (uncatchable) |
| SIGCONT | continue |

`SIGKILL` and `SIGSTOP` are always handled by `ProcessManager` directly and cannot be overridden.

### 8. Thread Scheduler (`thread_scheduler.h / thread_scheduler.cpp`)

`SimThread` objects represent user-space threads with four states: `READY`, `RUNNING`, `BLOCKED`, `TERMINATED`.

`Scheduler` implements **cooperative round-robin** scheduling:
1. On each `schedule()` call, the current `RUNNING` thread is demoted to `READY`.
2. All `READY` threads are collected.
3. The thread at `run_ptr_` is promoted to `RUNNING`; `run_ptr_` advances.

The `schedule` shell command advances the scheduler by one tick, showing which thread becomes active.

### 9. MutexSim (`mutex_sim.h / mutex_sim.cpp`)

Simulates a binary mutex. `acquire(tid)` sets `locked_=true` and `owner_=tid` if free; otherwise returns a descriptive error (simulating a blocking scenario). `release(tid)` clears the lock only if the caller owns it, preventing invalid releases.

This models critical-section protection — e.g. two threads cannot write to the same file simultaneously.

### 10. OutputFormatter (`output_formatter.h / output_formatter.cpp`)

Formats all model data into human-readable strings. Uses ANSI escape codes (with Windows 10 `SetConsoleMode` support):
- **Blue bold**: directories
- **Cyan**: symlinks
- **Green**: success messages
- **Red**: error messages
- **Yellow**: variable names, hints

Disable with `--no-color` flag.

### 11. Command Parser (`command_parser.h / command_parser.cpp`)

**`parse(raw)`** tokenises a string respecting single (`'`) and double (`"`) quoted strings. Returns `std::vector<std::string>`.

**`dispatch(tokens, ctx)`** uses a series of `if` comparisons on `tokens[0]` to route to the correct subsystem. All exceptions thrown by the model layer are caught and converted to `view.err(...)` strings. Three special return values signal the REPL:

| Return value | REPL action |
|---|---|
| `"::EXIT"` | Terminate the loop |
| `"::CLEAR"` | Clear the terminal |
| `"::DEFINE:<name>"` | Enter interactive template definition mode |

---

## How to Build

Requires: **g++ with C++17 support** (GCC 7+ or Clang 5+)

```bash
cd templateforge
make          # compiles all src/*.cpp -> build/*.o -> templateforge
make run      # build and launch immediately
make clean    # remove build artefacts
```

On Windows (without WSL), you can use:
- **MSYS2 / MinGW-w64**: `pacman -S mingw-w64-ucrt-x86_64-gcc` then `make`
- **Visual Studio** with CMakeLists (see Backlog)

---

## How to Use

### Starting the Shell

```
./templateforge              # default user "user"
./templateforge --user root  # start as root (bypasses all permissions)
./templateforge --no-color   # disable ANSI colours
```

The shell starts at `/home/user` with a pre-seeded filesystem.

### Shell Prompt

```
user:/home/user$
```

Format: `<current_user>:<cwd>$`. Root user displays in red.

### Example Session

```bash
# --- Filesystem basics ---
user:/home/user$ mkdir projects
user:/home/user$ cd projects
user:/home/user/projects$ touch hello.txt
user:/home/user/projects$ write hello.txt Hello, TemplateForge!
user:/home/user/projects$ cat hello.txt
Hello, TemplateForge!
user:/home/user/projects$ ls -l
-rw-r--r--  user          hello.txt

# --- Symbolic links ---
user:/home/user/projects$ ln hello.txt greet.lnk
user:/home/user/projects$ cat greet.lnk
Hello, TemplateForge!

# --- Access control ---
user:/home/user/projects$ chmod 644 hello.txt
user:/home/user/projects$ su root
root:/home/user/projects$ chown user hello.txt

# --- Variables and Templates ---
user:/home/user$ setvar AUTHOR Alice
user:/home/user$ setvar PROJECT webapp
user:/home/user$ vars
Variables:
  {{AUTHOR}}  =  "Alice"
  {{PROJECT}} =  "webapp"

user:/home/user$ define myproject
  define> src/
  define> src/main.cpp::// {{PROJECT}} by {{AUTHOR}}
  define> README.md::# {{PROJECT}}
  define> end
Template 'myproject' defined ('myproject': 1 directory, 2 files)

user:/home/user$ build myproject /home/user/projects/new
  + /home/user/projects/new/src
  + /home/user/projects/new/src/main.cpp
  + /home/user/projects/new/README.md

user:/home/user$ tree /home/user/projects/new
new/
├── README.md
└── src/
    └── main.cpp

user:/home/user$ cat /home/user/projects/new/src/main.cpp
// webapp by Alice

# --- Processes ---
user:/home/user$ ps
 PID  PPID  STATE         NAME
--------------------------------------------
   1     -  RUNNING       init

user:/home/user$ spawn worker 1
Process 'worker' spawned with PID 2

user:/home/user$ signal 2 SIGUSR1
PID 2: default action 'ignore'

user:/home/user$ kill 2

# --- IPC ---
user:/home/user$ mkpipe
Pipe created with ID 1
user:/home/user$ pwrite 1 "data from process A"
user:/home/user$ pread 1
data from process A

# --- Threads ---
user:/home/user$ spawn server 1
user:/home/user$ newthread 3 request-handler
Thread 'request-handler' (TID 3) created

user:/home/user$ newmutex file_lock
Mutex 'file_lock' created (ID 1)

user:/home/user$ acquire file_lock 1
Mutex 'file_lock' acquired by TID 1

user:/home/user$ acquire file_lock 2
Error: Mutex 'file_lock': locked by TID 1 — TID 2 would block

user:/home/user$ release file_lock 1
Mutex 'file_lock' released by TID 1

# --- Help ---
user:/home/user$ help templates
user:/home/user$ help processes

user:/home/user$ exit
Goodbye.
```

---

## Design Decisions

### Why `shared_ptr` for the tree?

Nodes are shared between the tree (parent/child references) and the `cwd` pointer in `FileSystemModel`. Reference-counted smart pointers eliminate manual memory management and prevent dangling pointer bugs when nodes are moved or copied.

### Why `std::map` for directory children?

`std::map` keeps children in sorted alphabetical order, which makes `ls` and `tree` output consistent without an extra sort step. The overhead is negligible for a simulation.

### Why simulated threads instead of real OS threads?

Real threads would introduce concurrency bugs and make the simulation non-deterministic. The user-space scheduler gives full control over scheduling decisions and makes the educational concepts explicit — the user can see exactly when a context switch happens by calling `schedule`.

### Why exceptions for filesystem errors?

The `FileSystemModel` throws `FileSystemError` (a `std::runtime_error` subclass) on every invalid operation. The dispatcher catches all exceptions and converts them to formatted error strings. This separates error-detection logic (in the model) from error-presentation logic (in the view), keeping both clean.
