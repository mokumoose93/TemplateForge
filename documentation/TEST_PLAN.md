# TemplateForge — Test Plan

## How to Run

Build the binary first, then launch the REPL. All test cases below are
entered interactively unless a flag variant is noted.

```bash
cd templateforge
make
./templateforge          # default user = "user"
./templateforge --user root
./templateforge --no-color
```

---

## 1. Startup & Seeding

| # | Action | Expected Result |
|---|--------|-----------------|
| 1.1 | Run `./templateforge` | Banner prints; prompt shows `[user@/home/user]$` |
| 1.2 | Run `./templateforge --user alice` | Prompt shows `[alice@/home/user]$` |
| 1.3 | Run `./templateforge --no-color` | Output contains no ANSI escape codes |
| 1.4 | Run `./templateforge --user root` | Prompt shows `[root@/home/user]$` |

---

## 2. Navigation

### 2.1 `pwd`

| # | Action | Expected Result |
|---|--------|-----------------|
| 2.1.1 | `pwd` | `/home/user` |
| 2.1.2 | `cd /` then `pwd` | `/` |

### 2.2 `cd`

| # | Action | Expected Result |
|---|--------|-----------------|
| 2.2.1 | `cd /tmp` | No output; prompt updates to `/tmp` |
| 2.2.2 | `cd ..` from `/home/user` | Prompt shows `/home` |
| 2.2.3 | `cd .` | Stays in same directory |
| 2.2.4 | `cd /home/user/../user` | Resolves to `/home/user` |
| 2.2.5 | `cd /nonexistent` | Error: `cd: no such directory` |
| 2.2.6 | `cd` (no argument) | Error: `cd: missing operand` |
| 2.2.7 | `cd /tmp/readme.txt` | Error: `cd: not a directory` |

### 2.3 `ls`

| # | Action | Expected Result |
|---|--------|-----------------|
| 2.3.1 | `ls` from `/home/user` | Lists `.profile` |
| 2.3.2 | `ls /` | Lists `bin  home  tmp  var` (dirs first, alpha) |
| 2.3.3 | `ls -l /` | Same entries with permission strings and owner |
| 2.3.4 | `ls /nonexistent` | Error: `ls: no such file or directory` |
| 2.3.5 | `ls /tmp/readme.txt` | Returns the single file entry |

### 2.4 `tree`

| # | Action | Expected Result |
|---|--------|-----------------|
| 2.4.1 | `tree /` | Full tree from root with all seeded paths |
| 2.4.2 | `tree` (no arg) | Tree from current directory |
| 2.4.3 | `tree /nonexistent` | Error: `tree: no such path` |

---

## 3. File & Directory Manipulation

### 3.1 `mkdir`

| # | Action | Expected Result |
|---|--------|-----------------|
| 3.1.1 | `mkdir /tmp/testdir` | Success message |
| 3.1.2 | `ls /tmp` | Shows `testdir` |
| 3.1.3 | `mkdir /tmp/a/b/c` | Creates all intermediate directories |
| 3.1.4 | `mkdir /tmp/testdir` (again) | No error — directory already exists (idempotent) |
| 3.1.5 | `mkdir /tmp/readme.txt/sub` | Error: `mkdir: 'readme.txt' is not a directory` |
| 3.1.6 | `mkdir` (no arg) | Error: `mkdir: missing operand` |

### 3.2 `touch`

| # | Action | Expected Result |
|---|--------|-----------------|
| 3.2.1 | `touch /tmp/newfile.txt` | Success message |
| 3.2.2 | `cat /tmp/newfile.txt` | Empty output |
| 3.2.3 | `touch /tmp/newfile.txt` (again) | Success (updates existing file) |
| 3.2.4 | `touch /nonexistent/file.txt` | Error: `touch: no such parent directory` |
| 3.2.5 | `touch` (no arg) | Error: `touch: missing operand` |

### 3.3 `rm`

| # | Action | Expected Result |
|---|--------|-----------------|
| 3.3.1 | `touch /tmp/del.txt` then `rm /tmp/del.txt` | Success; file gone from `ls` |
| 3.3.2 | `mkdir /tmp/emptydir` then `rm /tmp/emptydir` | Success |
| 3.3.3 | `mkdir /tmp/full` then `touch /tmp/full/f` then `rm /tmp/full` | Error: `is non-empty (use -r)` |
| 3.3.4 | `rm -r /tmp/full` | Recursively removes directory and contents |
| 3.3.5 | `rm /nonexistent` | Error: `rm: no such file or directory` |
| 3.3.6 | `rm` (no arg) | Error: `rm: missing operand` |

### 3.4 `mv`

| # | Action | Expected Result |
|---|--------|-----------------|
| 3.4.1 | `touch /tmp/a.txt` then `mv /tmp/a.txt /tmp/b.txt` | `b.txt` exists; `a.txt` gone |
| 3.4.2 | `mkdir /tmp/dest` then `mv /tmp/b.txt /tmp/dest` | `b.txt` inside `/tmp/dest` |
| 3.4.3 | `mv /nonexistent /tmp/x` | Error: `mv: no such file or directory` |
| 3.4.4 | `mv /tmp/b.txt` (only one arg) | Error: `mv: missing destination operand` |

### 3.5 `cp`

| # | Action | Expected Result |
|---|--------|-----------------|
| 3.5.1 | `cp /tmp/readme.txt /tmp/readme_copy.txt` | New file with same content |
| 3.5.2 | `mkdir /tmp/srcdir` then `touch /tmp/srcdir/f` then `cp /tmp/srcdir /tmp/dstdir` | Deep copy created |
| 3.5.3 | `cp /nonexistent /tmp/x` | Error: `cp: no such file or directory` |
| 3.5.4 | `cp /tmp/readme.txt` (only one arg) | Error: `cp: missing destination operand` |

### 3.6 `cat` & `write`

| # | Action | Expected Result |
|---|--------|-----------------|
| 3.6.1 | `cat /tmp/readme.txt` | Displays seeded content |
| 3.6.2 | `write /tmp/readme.txt hello world` | Success; `cat` returns `hello world` |
| 3.6.3 | `write /tmp/newfile.txt line one` | Creates file if absent and writes |
| 3.6.4 | `cat /nonexistent` | Error: `cat: no such file` |
| 3.6.5 | `cat /tmp` | Error: `cat: 'tmp' is a directory` |
| 3.6.6 | `write` (one arg only) | Error: `write: usage: write <path> <content>` |

### 3.7 Symbolic Links (`ln`)

| # | Action | Expected Result |
|---|--------|-----------------|
| 3.7.1 | `ln /tmp/readme.txt /tmp/link` | Success message |
| 3.7.2 | `cat /tmp/link` | Returns same content as `readme.txt` |
| 3.7.3 | `cd /tmp/link` | Error: follows symlink to file, not a directory |
| 3.7.4 | `ln /home/user /tmp/userlink` then `cd /tmp/userlink` | Navigates into `/home/user` |
| 3.7.5 | `ln /nonexistent /tmp/broken` then `cat /tmp/broken` | Error: broken symlink |
| 3.7.6 | `ln /tmp/readme.txt /tmp/link` (again) | Error: `ln: 'link' already exists` |
| 3.7.7 | `ln` (one arg only) | Error: `ln: usage: ln <target> <link_name>` |

---

## 4. Access Control

### 4.1 `whoami` & `su`

| # | Action | Expected Result |
|---|--------|-----------------|
| 4.1.1 | `whoami` | Prints current user (e.g. `user`) |
| 4.1.2 | `su alice` | Success; `whoami` returns `alice` |
| 4.1.3 | `su` (no arg) | Error: `su: missing username` |

### 4.2 `chmod`

| # | Action | Expected Result |
|---|--------|-----------------|
| 4.2.1 | `touch /tmp/sec.txt` then `chmod 600 /tmp/sec.txt` | Success |
| 4.2.2 | `ls -l /tmp/sec.txt` | Permission string shows `rw-------` |
| 4.2.3 | `su alice` then `cat /tmp/sec.txt` | Error: permission denied (alice has no read) |
| 4.2.4 | `su user` then `chmod 777 /tmp/sec.txt` | Success (owner can chmod) |
| 4.2.5 | `su alice` then `chmod 777 /tmp/sec.txt` | Error: only owner or root can chmod |
| 4.2.6 | `chmod abc /tmp/sec.txt` | Error: `chmod: invalid permission string` |
| 4.2.7 | `chmod 755` (no path) | Error: `chmod: usage: chmod <octal> <path>` |

### 4.3 `chown`

| # | Action | Expected Result |
|---|--------|-----------------|
| 4.3.1 | `su root` then `chown alice /tmp/sec.txt` | Success |
| 4.3.2 | `ls -l /tmp/sec.txt` | Owner shows `alice` |
| 4.3.3 | `su user` then `chown root /tmp/sec.txt` | Error: `chown: permission denied (root only)` |
| 4.3.4 | `chown alice` (no path) | Error: `chown: usage: chown <owner> <path>` |

### 4.4 Permission enforcement on write operations

| # | Action | Expected Result |
|---|--------|-----------------|
| 4.4.1 | `chmod 555 /tmp` then `su alice` then `touch /tmp/x` | Error: permission denied (no write on `/tmp`) |
| 4.4.2 | `su root` then `chmod 777 /tmp` | Restores write access |

---

## 5. Variable Management

| # | Action | Expected Result |
|---|--------|-----------------|
| 5.1 | `setvar NAME myproject` | `$NAME = "myproject"` |
| 5.2 | `vars` | Lists `NAME = myproject` |
| 5.3 | `setvar NAME updated` | Updates existing variable |
| 5.4 | `unsetvar NAME` | Variable removed |
| 5.5 | `vars` (after unset) | `NAME` no longer listed |
| 5.6 | `unsetvar MISSING` | Error: `unsetvar: 'MISSING' is not set` |
| 5.7 | `setvar` (one arg) | Error: `setvar: usage: setvar <NAME> <value>` |
| 5.8 | `setvar GREETING hello world` | Value is `hello world` (multi-word joined) |

---

## 6. Template Management

### 6.1 `define` & `templates`

| # | Action | Expected Result |
|---|--------|-----------------|
| 6.1.1 | `define myapp` → enter `src/` → `src/main.cpp` → `README.md::# {{NAME}}` → blank line | Template `myapp` saved |
| 6.1.2 | `templates` | Lists `myapp` with entry count summary |
| 6.1.3 | `define` (no arg) | Error: `define: usage: define <template_name>` |

### 6.2 `build`

| # | Action | Expected Result |
|---|--------|-----------------|
| 6.2.1 | `setvar NAME HelloApp` then `build myapp /tmp/proj` | Creates `/tmp/proj/src/`, `/tmp/proj/src/main.cpp`, `/tmp/proj/README.md` |
| 6.2.2 | `cat /tmp/proj/README.md` | Contains `# HelloApp` (placeholder substituted) |
| 6.2.3 | `tree /tmp/proj` | Full tree matches template structure |
| 6.2.4 | `build nonexistent /tmp/x` | Error referencing missing template |
| 6.2.5 | `build myapp` (no path) | Error: `build: usage: build <template> <path>` |

### 6.3 `rmtemplate`

| # | Action | Expected Result |
|---|--------|-----------------|
| 6.3.1 | `rmtemplate myapp` | Success |
| 6.3.2 | `templates` | `myapp` no longer listed |
| 6.3.3 | `rmtemplate myapp` (again) | Error: `Template 'myapp' not found` |
| 6.3.4 | `rmtemplate` (no arg) | Error: `rmtemplate: missing template name` |

### 6.4 Variable substitution edge cases

| # | Action | Expected Result |
|---|--------|-----------------|
| 6.4.1 | Define template with `config.txt::port={{PORT}}`, build without setting `PORT` | File contains literal `port={{PORT}}` (unresolved placeholder kept as-is) |
| 6.4.2 | `setvar PORT 8080` then rebuild | File contains `port=8080` |
| 6.4.3 | Define template with multiple `{{NAME}}` in content | All occurrences substituted |

---

## 7. Process Management

### 7.1 `ps` & `spawn`

| # | Action | Expected Result |
|---|--------|-----------------|
| 7.1.1 | `ps` on startup | Lists PID 1 (init) |
| 7.1.2 | `spawn worker` | Success with assigned PID (≥ 2); state RUNNING |
| 7.1.3 | `ps` | Both init and worker listed |
| 7.1.4 | `spawn child 2` | Creates process with parent PID 2 |
| 7.1.5 | `spawn` (no arg) | Spawns process with default name `process` |

### 7.2 `kill`

| # | Action | Expected Result |
|---|--------|-----------------|
| 7.2.1 | `kill <pid>` for a spawned process | Success; process no longer in `ps` |
| 7.2.2 | `kill 9999` (nonexistent PID) | Error referencing unknown process |
| 7.2.3 | `kill` (no arg) | Error: `kill: missing PID` |

### 7.3 `signal`

| # | Action | Expected Result |
|---|--------|-----------------|
| 7.3.1 | `spawn p` then `signal <pid> SIGTERM` | Process terminated |
| 7.3.2 | `spawn p` then `signal <pid> SIGKILL` | Process terminated (SIGKILL uncatchable) |
| 7.3.3 | `spawn p` then `signal <pid> SIGSTOP` | Process state becomes STOPPED |
| 7.3.4 | `signal <pid> SIGCONT` | Process resumes (state RUNNING) |
| 7.3.5 | `signal <pid> INVALID` | Error: invalid signal name |
| 7.3.6 | `signal <pid>` (no signal) | Error: `signal: usage: signal <pid> <SIGNAL>` |

---

## 8. Pipes (IPC)

| # | Action | Expected Result |
|---|--------|-----------------|
| 8.1 | `mkpipe` | Success with pipe ID 1 |
| 8.2 | `mkpipe` again | Success with pipe ID 2 |
| 8.3 | `pipes` | Lists both pipes as empty |
| 8.4 | `pwrite 1 hello` | Success |
| 8.5 | `pwrite 1 world` | Success |
| 8.6 | `pipes` | Pipe 1 shows 2 items buffered |
| 8.7 | `pread 1` | Returns `hello` (FIFO order) |
| 8.8 | `pread 1` | Returns `world` |
| 8.9 | `pread 1` | Returns `(pipe is empty)` |
| 8.10 | `pread` (no arg) | Error: `pread: missing pipe_id` |
| 8.11 | `pwrite 1` (no data) | Error: `pwrite: usage: pwrite <id> <data>` |

---

## 9. Threads & Scheduler

### 9.1 Thread creation & listing

| # | Action | Expected Result |
|---|--------|-----------------|
| 9.1.1 | `spawn proc` then `newthread <pid> worker` | Success with TID; state READY |
| 9.1.2 | `threads` | Lists the new thread |
| 9.1.3 | `newthread <pid> second` | Second thread created |
| 9.1.4 | `newthread` (missing args) | Error: `newthread: usage: newthread <pid> <name>` |

### 9.2 `schedule`

| # | Action | Expected Result |
|---|--------|-----------------|
| 9.2.1 | `schedule` with no threads | `No runnable threads.` |
| 9.2.2 | Create two threads then `schedule` | First thread becomes RUNNING, others READY |
| 9.2.3 | `schedule` again | Demotes first to READY, second becomes RUNNING (round-robin) |

### 9.3 `killthread`

| # | Action | Expected Result |
|---|--------|-----------------|
| 9.3.1 | `killthread <tid>` | Thread state becomes TERMINATED; gone from `threads` |
| 9.3.2 | `killthread` (no arg) | Error: `killthread: missing TID` |

---

## 10. Mutexes

| # | Action | Expected Result |
|---|--------|-----------------|
| 10.1 | `newmutex mylock` | Success with mutex ID |
| 10.2 | `mutexes` | Lists `mylock` as unlocked |
| 10.3 | `acquire mylock 1` | Success; mutex locked by TID 1 |
| 10.4 | `acquire mylock 2` | Failure: already locked |
| 10.5 | `mutexes` | Shows `mylock` locked by TID 1 |
| 10.6 | `release mylock 2` | Failure: TID 2 does not own the lock |
| 10.7 | `release mylock 1` | Success; mutex unlocked |
| 10.8 | `acquire mylock 2` | Success (lock is now free) |
| 10.9 | `newmutex` (no arg) | Error: `newmutex: missing mutex name` |
| 10.10 | `acquire` (no arg) | Error: `acquire: missing mutex name/id` |

---

## 11. Parser & Input Handling

| # | Action | Expected Result |
|---|--------|-----------------|
| 11.1 | Empty input (just Enter) | No output; prompt re-displayed |
| 11.2 | `  ls  ` (extra whitespace) | Works normally |
| 11.3 | `mkdir "dir with spaces"` | Creates directory named `dir with spaces` |
| 11.4 | `mkdir 'single quoted'` | Creates directory named `single quoted` |
| 11.5 | `unknowncmd` | Error: `'unknowncmd': command not found` |
| 11.6 | `exit` | Prints `Goodbye.` and terminates |
| 11.7 | `quit` | Same as `exit` |
| 11.8 | `clear` | Screen cleared; prompt re-displayed |
| 11.9 | EOF (Ctrl+D) | Prints `exit` and terminates cleanly |

---

## 12. Help System

| # | Action | Expected Result |
|---|--------|-----------------|
| 12.1 | `help` | Prints all command categories |
| 12.2 | `help navigation` | Prints navigation section only |
| 12.3 | `help files` | Prints file & directory commands |
| 12.4 | `help access` | Prints access control section |
| 12.5 | `help variables` | Prints variable management section |
| 12.6 | `help templates` | Prints template management section |
| 12.7 | `help processes` | Prints process management section |
| 12.8 | `help ipc` | Prints pipe commands |
| 12.9 | `help threads` | Prints thread commands |
| 12.10 | `help mutexes` | Prints mutex commands |
| 12.11 | `help badtopic` | Error listing valid topics |

---

## 13. End-to-End Scenario

This scenario exercises all subsystems together in one session.

```
# 1. Set up a variable and define a template
setvar PROJECT myservice
define webapp
  > src/
  > src/main.cpp::// {{PROJECT}} entry point
  > Makefile::all: main
  > README.md::# {{PROJECT}}
  > (blank line)

# 2. Build the template into the filesystem
mkdir /home/user/projects
build webapp /home/user/projects/myservice

# 3. Verify the output
tree /home/user/projects/myservice
cat /home/user/projects/myservice/README.md     # should show: # myservice
cat /home/user/projects/myservice/src/main.cpp  # should show: // myservice entry point

# 4. Lock a file with permissions and test access control
chmod 600 /home/user/projects/myservice/README.md
su alice
cat /home/user/projects/myservice/README.md     # should fail (permission denied)
su user

# 5. Simulate a process workflow with IPC
spawn producer
spawn consumer
mkpipe
pwrite 1 "build complete"
pread 1                   # returns "build complete"

# 6. Thread and mutex workflow
newthread 2 t1
newthread 2 t2
newmutex filelock
acquire filelock 1
acquire filelock 2        # should fail
schedule                  # t1 or t2 becomes RUNNING
release filelock 1
acquire filelock 2        # should succeed

# 7. Clean up
rmtemplate webapp
kill 2
kill 3
```

Expected: each step succeeds or fails as annotated, no crashes, no
unhandled exceptions.
