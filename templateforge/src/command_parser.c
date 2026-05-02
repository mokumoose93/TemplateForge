#include "command_parser.h"
#include "str_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ---- Tokeniser ---- */

int cmd_parse(const char* raw, char** tokens, int max) {
    int count = 0;
    char in_quote = 0;
    StrBuf cur; sb_init(&cur);
    for (int i = 0; raw[i]; i++) {
        char c = raw[i];
        if (in_quote) {
            if (c == in_quote) in_quote = 0;
            else sb_appendc(&cur, c);
        } else if (c == '\'' || c == '"') {
            in_quote = c;
        } else if (c == ' ' || c == '\t') {
            if (cur.len > 0 && count < max) { tokens[count++] = sb_dup(&cur); sb_reset(&cur); }
        } else {
            sb_appendc(&cur, c);
        }
    }
    if (cur.len > 0 && count < max) tokens[count++] = sb_dup(&cur);
    sb_free(&cur);
    return count;
}

void cmd_free_tokens(char** tokens, int count) {
    for (int i = 0; i < count; i++) free(tokens[i]);
}

/* ---- Helpers ---- */

static int has_flag(char** args, int n, const char* flag) {
    for (int i = 0; i < n; i++) if (strcmp(args[i], flag) == 0) return 1;
    return 0;
}

/* Returns heap string joining args[start..n-1] with spaces; caller frees. */
static char* join(char** args, int n, int start) {
    StrBuf sb; sb_init(&sb);
    for (int i = start; i < n; i++) { if (i > start) sb_appendc(&sb, ' '); sb_append(&sb, args[i]); }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

/* Collect args that don't start with '-' */
static int non_flag_args(char** args, int n, char** out, int outmax) {
    int c = 0;
    for (int i = 0; i < n && c < outmax; i++)
        if (!args[i][0] || args[i][0] != '-') out[c++] = args[i];
    return c;
}

/* ---- dispatch ---- */

char* cmd_dispatch(char** tokens, int count, AppContext* ctx) {
    if (count == 0) return strdup("");
    const char* cmd = tokens[0];
    char** args = tokens + 1;
    int    na   = count - 1;

    FSModel*    fs   = ctx->fs;
    TplManager* tm   = ctx->tm;
    VarManager* vm   = ctx->vm;
    ProcManager* pm  = ctx->pm;
    OutputFmt*  view = ctx->view;

    char eb[512] = {0};

    /* ---- Navigation ---- */
    if (strcmp(cmd,"pwd")==0) { char* p = fs_pwd(fs); char* r = fmt_pwd(view,p); free(p); return r; }

    if (strcmp(cmd,"cd")==0) {
        if (!na) return fmt_err(view,"cd: missing operand");
        if (fs_cd(fs, args[0], eb, sizeof(eb)) != 0) return fmt_err(view, eb);
        return strdup("");
    }

    if (strcmp(cmd,"ls")==0) {
        int lng = has_flag(args, na, "-l");
        char* nf[64]; int nn = non_flag_args(args, na, nf, 64);
        const char* path = nn ? nf[0] : "";
        int cnt; Node** nodes = fs_ls(fs, path, &cnt, eb, sizeof(eb));
        if (!nodes) return fmt_err(view, eb);
        char* r = fmt_ls(view, nodes, cnt, lng); free(nodes); return r;
    }

    if (strcmp(cmd,"tree")==0) {
        const char* path = na ? args[0] : "";
        Node* node = path[0] ? fs_get_node(fs, path, 1) : (Node*)fs->cwd;
        if (!node) { snprintf(eb,sizeof(eb),"tree: no such path: '%s'",path); return fmt_err(view,eb); }
        return fmt_tree(view, node, "", 1);
    }

    /* ---- File / directory manipulation ---- */
    if (strcmp(cmd,"mkdir")==0) {
        if (!na) return fmt_err(view,"mkdir: missing operand");
        StrBuf sb; sb_init(&sb);
        for (int i = 0; i < na; i++) {
            if (fs_mkdir(fs, args[i], eb, sizeof(eb)) != 0) { sb_free(&sb); return fmt_err(view, eb); }
            char* s = fmt_ok(view, ""); free(s); /* use appendf directly */
            sb_appendf(&sb, "Directory '%s' created", args[i]);
            char* ok = fmt_ok(view, sb.data); sb_reset(&sb);
            sb_append(&sb, ok); free(ok);
            if (i + 1 < na) sb_appendc(&sb, '\n');
        }
        char* r = sb_dup(&sb); sb_free(&sb); return r;
    }

    if (strcmp(cmd,"touch")==0) {
        if (!na) return fmt_err(view,"touch: missing operand");
        StrBuf sb; sb_init(&sb);
        for (int i = 0; i < na; i++) {
            if (fs_touch(fs, args[i], "", eb, sizeof(eb)) != 0) { sb_free(&sb); return fmt_err(view,eb); }
            char tmp[512]; snprintf(tmp,sizeof(tmp),"File '%s' created/updated",args[i]);
            char* ok = fmt_ok(view, tmp); sb_append(&sb, ok); free(ok);
            if (i + 1 < na) sb_appendc(&sb, '\n');
        }
        char* r = sb_dup(&sb); sb_free(&sb); return r;
    }

    if (strcmp(cmd,"rm")==0) {
        if (!na) return fmt_err(view,"rm: missing operand");
        int rec = has_flag(args,na,"-r") || has_flag(args,na,"-rf");
        char* nf[64]; int nn = non_flag_args(args,na,nf,64);
        StrBuf sb; sb_init(&sb);
        for (int i = 0; i < nn; i++) {
            if (fs_rm(fs, nf[i], rec, eb, sizeof(eb)) != 0) { sb_free(&sb); return fmt_err(view,eb); }
            char tmp[512]; snprintf(tmp,sizeof(tmp),"Removed '%s'",nf[i]);
            char* ok = fmt_ok(view,tmp); sb_append(&sb,ok); free(ok);
            if (i + 1 < nn) sb_appendc(&sb,'\n');
        }
        char* r = sb_dup(&sb); sb_free(&sb); return r;
    }

    if (strcmp(cmd,"mv")==0) {
        if (na < 2) return fmt_err(view,"mv: missing destination operand");
        if (fs_mv(fs,args[0],args[1],eb,sizeof(eb)) != 0) return fmt_err(view,eb);
        char tmp[512]; snprintf(tmp,sizeof(tmp),"Moved '%s' -> '%s'",args[0],args[1]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"cp")==0) {
        if (na < 2) return fmt_err(view,"cp: missing destination operand");
        if (fs_cp(fs,args[0],args[1],eb,sizeof(eb)) != 0) return fmt_err(view,eb);
        char tmp[512]; snprintf(tmp,sizeof(tmp),"Copied '%s' -> '%s'",args[0],args[1]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"cat")==0) {
        if (!na) return fmt_err(view,"cat: missing operand");
        char* content = fs_cat(fs,args[0],eb,sizeof(eb));
        if (!content) return fmt_err(view,eb);
        return content;
    }

    if (strcmp(cmd,"write")==0) {
        if (na < 2) return fmt_err(view,"write: usage: write <path> <content>");
        char* content = join(args,na,1);
        Node* n = fs_get_node(fs,args[0],1);
        int rc = n ? fs_write_file(fs,args[0],content,eb,sizeof(eb))
                   : fs_touch(fs,args[0],content,eb,sizeof(eb));
        free(content);
        if (rc != 0) return fmt_err(view,eb);
        char tmp[512]; snprintf(tmp,sizeof(tmp),"Written to '%s'",args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"ln")==0) {
        if (na < 2) return fmt_err(view,"ln: usage: ln <target> <link_name>");
        if (fs_ln(fs,args[0],args[1],eb,sizeof(eb)) != 0) return fmt_err(view,eb);
        char tmp[512]; snprintf(tmp,sizeof(tmp),"Symlink '%s' -> '%s'",args[1],args[0]);
        return fmt_ok(view,tmp);
    }

    /* ---- Access control ---- */
    if (strcmp(cmd,"chmod")==0) {
        if (na < 2) return fmt_err(view,"chmod: usage: chmod <octal> <path>");
        if (fs_chmod(fs,args[1],args[0],eb,sizeof(eb)) != 0) return fmt_err(view,eb);
        char tmp[512]; snprintf(tmp,sizeof(tmp),"Permissions of '%s' set to %s",args[1],args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"chown")==0) {
        if (na < 2) return fmt_err(view,"chown: usage: chown <owner> <path>");
        if (fs_chown(fs,args[1],args[0],eb,sizeof(eb)) != 0) return fmt_err(view,eb);
        char tmp[512]; snprintf(tmp,sizeof(tmp),"Owner of '%s' changed to '%s'",args[1],args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"whoami")==0) return strdup(fs->current_user);

    if (strcmp(cmd,"su")==0) {
        if (!na) return fmt_err(view,"su: missing username");
        snprintf(fs->current_user, sizeof(fs->current_user), "%s", args[0]);
        char tmp[128]; snprintf(tmp,sizeof(tmp),"Switched to user '%s'",args[0]);
        return fmt_ok(view,tmp);
    }

    /* ---- Variables ---- */
    if (strcmp(cmd,"setvar")==0) {
        if (na < 2) return fmt_err(view,"setvar: usage: setvar <NAME> <value>");
        char* val = join(args,na,1);
        int rc = vm_set(vm,args[0],val,eb,sizeof(eb)); free(val);
        if (rc != 0) return fmt_err(view,eb);
        char tmp[256]; snprintf(tmp,sizeof(tmp),"$%s = \"%s\"",args[0], vm_get(vm,args[0]));
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"unsetvar")==0) {
        if (!na) return fmt_err(view,"unsetvar: missing variable name");
        if (!vm_unset(vm,args[0])) { snprintf(eb,sizeof(eb),"unsetvar: '%s' is not set",args[0]); return fmt_err(view,eb); }
        char tmp[128]; snprintf(tmp,sizeof(tmp),"Variable '%s' removed",args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"vars")==0) {
        const char* names[256]; const char* vals[256]; int nv = 0;
        for (int i = 0; i < vm->count && nv < 256; i++)
            { names[nv] = vm->entries[i].name; vals[nv] = vm->entries[i].value; nv++; }
        return fmt_vars(view, names, vals, nv);
    }

    /* ---- Templates ---- */
    if (strcmp(cmd,"define")==0) {
        if (!na) return fmt_err(view,"define: usage: define <template_name>");
        /* signal REPL to enter interactive define mode */
        StrBuf sb; sb_init(&sb);
        sb_append(&sb,"::DEFINE:"); sb_append(&sb,args[0]);
        char* r = sb_dup(&sb); sb_free(&sb); return r;
    }

    if (strcmp(cmd,"templates")==0) {
        if (tm->count == 0) return strdup("No templates defined. Use 'define <name>' to create one.");
        StrBuf sb; sb_init(&sb);
        for (int i = 0; i < tm->count; i++) {
            char* s = tm_summary(&tm->defs[i]);
            sb_append(&sb,"  "); sb_append(&sb,s); free(s);
            if (i + 1 < tm->count) sb_appendc(&sb,'\n');
        }
        char* r = sb_dup(&sb); sb_free(&sb); return r;
    }

    if (strcmp(cmd,"rmtemplate")==0) {
        if (!na) return fmt_err(view,"rmtemplate: missing template name");
        if (!tm_remove(tm,args[0])) { snprintf(eb,sizeof(eb),"Template '%s' not found",args[0]); return fmt_err(view,eb); }
        char tmp[128]; snprintf(tmp,sizeof(tmp),"Template '%s' removed",args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"build")==0) {
        if (na < 2) return fmt_err(view,"build: usage: build <template> <path>");
        int cnt; char** created = tm_build(tm,args[0],args[1],fs,vm,&cnt,eb,sizeof(eb));
        if (!created) return fmt_err(view,eb);
        char hdr[256]; snprintf(hdr,sizeof(hdr),"Built template '%s' at '%s'",args[0],args[1]);
        char* ok = fmt_ok(view,hdr);
        StrBuf sb; sb_init(&sb); sb_append(&sb,ok); free(ok);
        for (int i = 0; i < cnt; i++) { sb_appendf(&sb,"\n  + %s",created[i]); free(created[i]); }
        free(created);
        char* r = sb_dup(&sb); sb_free(&sb); return r;
    }

    /* ---- Processes ---- */
    if (strcmp(cmd,"ps")==0) return fmt_processes(view, pm->procs, pm->n_procs);

    if (strcmp(cmd,"spawn")==0) {
        const char* name = na ? args[0] : "process";
        int ppid = (na > 1) ? atoi(args[1]) : -1;
        int pid = pm_spawn(pm, name, ppid, eb, sizeof(eb));
        if (pid < 0) return fmt_err(view,eb);
        char tmp[128]; snprintf(tmp,sizeof(tmp),"Process '%s' spawned with PID %d",name,pid);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"kill")==0) {
        if (!na) return fmt_err(view,"kill: missing PID");
        if (pm_terminate(pm,atoi(args[0]),eb,sizeof(eb)) != 0) return fmt_err(view,eb);
        char tmp[64]; snprintf(tmp,sizeof(tmp),"Process %s terminated",args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"signal")==0) {
        if (na < 2) return fmt_err(view,"signal: usage: signal <pid> <SIGNAL>");
        char* res = pm_send_signal(pm,atoi(args[0]),args[1],eb,sizeof(eb));
        if (!res) return fmt_err(view,eb);
        char* r = fmt_ok(view,res); free(res); return r;
    }

    /* ---- Pipes ---- */
    if (strcmp(cmd,"mkpipe")==0) {
        int id = pm_create_pipe(pm,eb,sizeof(eb));
        if (id < 0) return fmt_err(view,eb);
        char tmp[64]; snprintf(tmp,sizeof(tmp),"Pipe created with ID %d",id);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"pipes")==0) return fmt_pipes(view, pm->pipes, pm->n_pipes);

    if (strcmp(cmd,"pwrite")==0) {
        if (na < 2) return fmt_err(view,"pwrite: usage: pwrite <id> <data>");
        char* data = join(args,na,1);
        pm_pipe_write(pm,atoi(args[0]),data,eb,sizeof(eb));
        free(data);
        if (eb[0]) return fmt_err(view,eb);
        char tmp[64]; snprintf(tmp,sizeof(tmp),"Wrote to pipe %s",args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"pread")==0) {
        if (!na) return fmt_err(view,"pread: missing pipe_id");
        char* data = pm_pipe_read(pm,atoi(args[0]),eb,sizeof(eb));
        if (eb[0]) return fmt_err(view,eb);
        if (!data) return strdup("(pipe is empty)");
        return data; /* heap, caller frees via result */
    }

    /* ---- Threads ---- */
    if (strcmp(cmd,"threads")==0) return fmt_threads(view, pm->sched.threads, pm->sched.count);

    if (strcmp(cmd,"newthread")==0) {
        if (na < 2) return fmt_err(view,"newthread: usage: newthread <pid> <name>");
        int tid = sched_create_thread(&pm->sched, args[1], atoi(args[0]));
        if (tid < 0) return fmt_err(view,"thread table full");
        char tmp[128]; snprintf(tmp,sizeof(tmp),"Thread '%s' (TID %d) created",args[1],tid);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"killthread")==0) {
        if (!na) return fmt_err(view,"killthread: missing TID");
        if (sched_terminate_thread(&pm->sched,atoi(args[0]),eb,sizeof(eb)) != 0) return fmt_err(view,eb);
        char tmp[64]; snprintf(tmp,sizeof(tmp),"Thread %s terminated",args[0]);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"schedule")==0) {
        int tid = sched_schedule(&pm->sched);
        if (tid < 0) return fmt_inf(view,"No runnable threads.");
        char tmp[64]; snprintf(tmp,sizeof(tmp),"Scheduler: TID %d is now RUNNING",tid);
        return fmt_ok(view,tmp);
    }

    /* ---- Mutexes ---- */
    if (strcmp(cmd,"mutexes")==0) return fmt_mutexes(view, pm->mutexes, pm->n_mutexes);

    if (strcmp(cmd,"newmutex")==0) {
        if (!na) return fmt_err(view,"newmutex: missing mutex name");
        int mid = pm_create_mutex(pm,args[0],eb,sizeof(eb));
        if (mid < 0) return fmt_err(view,eb);
        char tmp[128]; snprintf(tmp,sizeof(tmp),"Mutex '%s' created (ID %d)",args[0],mid);
        return fmt_ok(view,tmp);
    }

    if (strcmp(cmd,"acquire")==0) {
        if (!na) return fmt_err(view,"acquire: missing mutex name/id");
        int tid = (na > 1) ? atoi(args[1]) : 0;
        MutexSim* m = pm_find_mutex(pm,args[0],eb,sizeof(eb));
        if (!m) return fmt_err(view,eb);
        char msg[256]; int ok = mutex_acquire(m,tid,msg,sizeof(msg));
        return ok ? fmt_ok(view,msg) : fmt_err(view,msg);
    }

    if (strcmp(cmd,"release")==0) {
        if (!na) return fmt_err(view,"release: missing mutex name/id");
        int tid = (na > 1) ? atoi(args[1]) : 0;
        MutexSim* m = pm_find_mutex(pm,args[0],eb,sizeof(eb));
        if (!m) return fmt_err(view,eb);
        char msg[256]; int ok = mutex_release(m,tid,msg,sizeof(msg));
        return ok ? fmt_ok(view,msg) : fmt_err(view,msg);
    }

    /* ---- Misc ---- */
    if (strcmp(cmd,"help") ==0) return cmd_help_text(na ? args[0] : "");
    if (strcmp(cmd,"clear")==0) return strdup("::CLEAR");
    if (strcmp(cmd,"exit") ==0 || strcmp(cmd,"quit")==0) return strdup("::EXIT");

    snprintf(eb,sizeof(eb),"'%s': command not found  (type 'help' for commands)",cmd);
    return fmt_err(view,eb);
}

/* ---- help_text ---- */

char* cmd_help_text(const char* topic) {
    static const struct { const char* key; const char* text; } sections[] = {
    {"navigation",
"Navigation\n"
"  pwd                        Print working directory\n"
"  ls [-l] [path]             List directory contents\n"
"  cd <path>                  Change directory\n"
"  tree [path]                Show directory tree"},
    {"files",
"File & Directory Commands\n"
"  mkdir <path> [...]         Create directory (parents auto-created)\n"
"  touch <path> [...]         Create or update a file\n"
"  rm [-r] <path> [...]       Remove file or directory (-r for recursive)\n"
"  mv <src> <dst>             Move or rename\n"
"  cp <src> <dst>             Copy (recursive for directories)\n"
"  cat <path>                 Display file content\n"
"  write <path> <text>        Write text to a file\n"
"  ln <target> <link>         Create a symbolic link"},
    {"access",
"Access Control\n"
"  whoami                     Show current user\n"
"  su <user>                  Switch user\n"
"  chmod <octal> <path>       Change permissions  (e.g. chmod 755 /bin)\n"
"  chown <owner> <path>       Change owner (root only)"},
    {"variables",
"Variables\n"
"  setvar <NAME> <value>      Define a template variable\n"
"  unsetvar <NAME>            Remove a variable\n"
"  vars                       List all variables"},
    {"templates",
"Templates\n"
"  define <name>              Start interactive template definition\n"
"  templates                  List all defined templates\n"
"  rmtemplate <name>          Remove a template\n"
"  build <name> <path>        Instantiate a template at a path"},
    {"processes",
"Processes\n"
"  ps                         List all processes\n"
"  spawn <name> [ppid]        Spawn a new process\n"
"  kill <pid>                 Terminate a process (SIGTERM)\n"
"  signal <pid> <SIG>         Send a signal to a process\n"
"                             Signals: SIGTERM SIGKILL SIGINT SIGHUP SIGUSR1 SIGUSR2"},
    {"ipc",
"IPC / Pipes\n"
"  mkpipe                     Create a new FIFO pipe\n"
"  pipes                      List all pipes\n"
"  pwrite <id> <data>         Write data to a pipe\n"
"  pread <id>                 Read one item from a pipe"},
    {"threads",
"Threads\n"
"  threads                    List all simulated threads\n"
"  newthread <pid> <name>     Create a thread under a process\n"
"  killthread <tid>           Terminate a thread\n"
"  schedule                   Advance scheduler by one tick"},
    {"mutexes",
"Mutexes\n"
"  mutexes                    List all mutexes\n"
"  newmutex <name>            Create a named mutex\n"
"  acquire <name> [tid]       Acquire a mutex\n"
"  release <name> [tid]       Release a mutex"},
    {NULL, NULL}
    };

    if (topic && topic[0]) {
        for (int i = 0; sections[i].key; i++)
            if (strcmp(sections[i].key, topic) == 0) return strdup(sections[i].text);
        char buf[256];
        snprintf(buf,sizeof(buf),"No help topic '%s'. Topics: navigation files access variables templates processes ipc threads mutexes", topic);
        return strdup(buf);
    }

    StrBuf sb; sb_init(&sb);
    sb_append(&sb, "TemplateForge \xe2\x80\x94 Simulated File System Shell\n");
    sb_append(&sb, "============================================\n\n");
    for (int i = 0; sections[i].key; i++) { sb_append(&sb, sections[i].text); sb_append(&sb, "\n\n"); }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}
