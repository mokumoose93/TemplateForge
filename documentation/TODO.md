# TemplateForge — Development Plan

## Phase 1: Core Filesystem (Foundation)
- [x] Define node types: `FileNode`, `DirectoryNode`, `SymlinkNode`
- [x] Implement `AccessMetadata` with Unix-style permission bits (rwx)
- [x] Implement `FileSystemModel` with in-memory tree using `shared_ptr`
- [x] Navigation commands: `pwd`, `cd`, `ls`, `tree`
- [x] File/directory CRUD: `mkdir`, `touch`, `rm`, `mv`, `cp`
- [x] File I/O: `cat`, `write`
- [x] Symbolic link creation: `ln`
- [x] Symbolic link resolution (dereference chains, broken link detection)
- [x] Permission enforcement (read/write/execute checks per command)
- [x] Access control commands: `chmod`, `chown`, `whoami`, `su`

## Phase 2: Template Engine
- [x] Implement `VariableManager` with `{{VAR_NAME}}` placeholder syntax
- [x] Commands: `setvar`, `unsetvar`, `vars`
- [x] Implement `TemplateDefinition` with directory and file entries
- [x] Implement `TemplateManager::define()` with inline file content support (`path::content`)
- [x] Implement `TemplateManager::build()` with full variable substitution
- [x] Commands: `define`, `templates`, `rmtemplate`, `build`
- [x] Interactive multi-line template definition mode

## Phase 3: Process Management
- [x] Define `Process` with PID, PPID, state, children
- [x] Implement `ProcessManager` with PID 1 (init) bootstrap
- [x] Process lifecycle: `spawn`, `terminate`, zombie state
- [x] Commands: `ps`, `spawn`, `kill`

## Phase 4: Inter-Process Communication
- [x] Implement `Pipe` with FIFO deque buffer
- [x] Commands: `mkpipe`, `pipes`, `pwrite`, `pread`
- [x] Basic pipe-chain support in REPL (`cmd1 | cmd2`)

## Phase 5: Signals
- [x] Define `Signal` enum (SIGTERM, SIGKILL, SIGINT, SIGHUP, SIGUSR1, SIGUSR2, SIGCHLD, SIGSTOP, SIGCONT)
- [x] Implement `SignalRegistry` with custom handler registration
- [x] Uncatchable signals: SIGKILL, SIGSTOP
- [x] Default signal actions (terminate/stop/continue/ignore)
- [x] Signal delivery command: `signal <pid> <SIG>`

## Phase 6: Threads & Mutexes
- [x] Define `SimThread` with TID, owner PID, and state (READY/RUNNING/BLOCKED/TERMINATED)
- [x] Implement `Scheduler` with round-robin context switching
- [x] Commands: `threads`, `newthread`, `killthread`, `schedule`
- [x] Implement `MutexSim` with acquire/release and deadlock prevention
- [x] Commands: `mutexes`, `newmutex`, `acquire`, `release`

## Phase 7: Shell & UX
- [x] REPL loop with prompt showing `user:/path$`
- [x] Quoted string support in tokeniser (`parse()`)
- [x] ANSI colour output (blue directories, cyan symlinks, red errors)
- [x] Windows ANSI support via `SetConsoleMode`
- [x] Built-in `help` with per-topic output
- [x] `--user` and `--no-color` CLI flags
- [x] Pre-seeded initial filesystem (`/home/user`, `/tmp`, `/var`, `/bin`)

---

## Backlog / Future Improvements
- [ ] Persistent filesystem: save/load the tree to a JSON or binary file
- [ ] `grep` command: search file content within the simulated FS
- [ ] `find` command: locate nodes by name or type
- [ ] File size tracking and `du`/`df` simulation
- [ ] Multi-user groups: proper group permission checking
- [ ] Hard links (reference counting on inodes)
- [ ] Interactive pipe editor: full `cmd1 | cmd2 | cmd3` chaining
- [ ] Process I/O redirection: `cmd > file` and `cmd < file`
- [ ] `history` command: REPL command history with up-arrow recall
- [ ] Scripting mode: execute a list of commands from a file
- [ ] Unit tests for each subsystem (filesystem, templates, processes)
- [ ] Signal handler registration from the shell: `sighandle <pid> <SIG> <action>`
- [ ] Thread blocking on mutex acquire (simulated BLOCKED state integration)
- [ ] `top`-style live process monitor display
