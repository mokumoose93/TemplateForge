#include "output_formatter.h"
#include "str_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define BLUE   "\033[34m"
#define CYAN   "\033[36m"

void fmt_init(OutputFmt* f, int use_color) { f->use_color = use_color; }

static char* col(const OutputFmt* f, const char* text, const char* code) {
    if (!f->use_color) return strdup(text);
    StrBuf sb; sb_init(&sb);
    sb_append(&sb, code); sb_append(&sb, text); sb_append(&sb, RESET);
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

char* fmt_pwd(const OutputFmt* f, const char* path) { return col(f, path, BOLD); }

char* fmt_prompt(const OutputFmt* f, const char* cwd, const char* user) {
    char* u = col(f, user, strcmp(user,"root")==0 ? RED : GREEN);
    char* p = col(f, cwd,  CYAN);
    StrBuf sb; sb_init(&sb);
    sb_append(&sb, u); sb_appendc(&sb, ':'); sb_append(&sb, p); sb_append(&sb, "$ ");
    char* r = sb_dup(&sb); sb_free(&sb); free(u); free(p); return r;
}

char* fmt_ls(const OutputFmt* f, Node** nodes, int count, int long_fmt) {
    if (count == 0) return strdup("(empty)");
    StrBuf sb; sb_init(&sb);

    if (!long_fmt) {
        for (int i = 0; i < count; i++) {
            if (i > 0) sb_append(&sb, "  ");
            Node* n = nodes[i];
            if (n->type == NT_DIR) {
                char tmp[512]; snprintf(tmp, sizeof(tmp), "%s/", n->name);
                char* s = col(f, tmp, BOLD BLUE); sb_append(&sb, s); free(s);
            } else if (n->type == NT_SYMLINK) {
                char tmp[512]; snprintf(tmp, sizeof(tmp), "%s@", n->name);
                char* s = col(f, tmp, CYAN); sb_append(&sb, s); free(s);
            } else {
                sb_append(&sb, n->name);
            }
        }
    } else {
        for (int i = 0; i < count; i++) {
            Node* n = nodes[i];
            char perms[10]; access_perm_string(&n->access, perms);
            sb_appendf(&sb, "%c%s  %-12s  ", node_type_label(n), perms, n->access.owner);
            if (n->type == NT_DIR) {
                char tmp[512]; snprintf(tmp, sizeof(tmp), "%s/", n->name);
                char* s = col(f, tmp, BOLD BLUE); sb_append(&sb, s); free(s);
            } else if (n->type == NT_SYMLINK) {
                char* nc = col(f, n->name, CYAN);
                char* tc = col(f, n->symlink.target, YELLOW);
                sb_append(&sb, nc); sb_append(&sb, " -> "); sb_append(&sb, tc);
                free(nc); free(tc);
            } else {
                sb_append(&sb, n->name);
            }
            if (i + 1 < count) sb_appendc(&sb, '\n');
        }
    }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

/* Recursive tree helper — writes into sb */
static void fmt_tree_sb(const OutputFmt* f, const Node* node,
                         const char* prefix, int is_last, StrBuf* sb) {
    char label_buf[512];
    if (node->type == NT_DIR) {
        char tmp[256]; snprintf(tmp, sizeof(tmp), "%s/", node->name);
        char* s = col(f, tmp, BOLD BLUE);
        snprintf(label_buf, sizeof(label_buf), "%s", s); free(s);
    } else if (node->type == NT_SYMLINK) {
        char* nc = col(f, node->name, CYAN);
        snprintf(label_buf, sizeof(label_buf), "%s -> %s", nc, node->symlink.target);
        free(nc);
    } else {
        snprintf(label_buf, sizeof(label_buf), "%s", node->name);
    }

    if (prefix[0] == '\0') {
        sb_append(sb, label_buf);
    } else {
        sb_append(sb, prefix);
        sb_append(sb, is_last ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 "   /* └── */
                              : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 "); /* ├── */
        sb_append(sb, label_buf);
    }

    if (node->type == NT_DIR) {
        int cc; Node** children = dir_sorted_children(node, &cc);
        char child_prefix[1024];
        snprintf(child_prefix, sizeof(child_prefix), "%s%s", prefix,
                 is_last ? "    " : "\xe2\x94\x82   "); /* "│   " */
        for (int i = 0; i < cc; i++) {
            sb_appendc(sb, '\n');
            fmt_tree_sb(f, children[i], child_prefix, i == cc - 1, sb);
        }
        free(children);
    }
}

char* fmt_tree(const OutputFmt* f, const Node* node, const char* prefix, int is_last) {
    StrBuf sb; sb_init(&sb);
    fmt_tree_sb(f, node, prefix ? prefix : "", is_last, &sb);
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

char* fmt_vars(const OutputFmt* f, const char** names, const char** values, int n) {
    if (n == 0) return strdup("No variables set. Use 'setvar NAME value' to define one.");
    StrBuf sb; sb_init(&sb);
    char* hdr = col(f, "Variables:", BOLD); sb_append(&sb, hdr); sb_appendc(&sb, '\n'); free(hdr);
    for (int i = 0; i < n; i++) {
        char tmp[256]; snprintf(tmp, sizeof(tmp), "{{%s}}", names[i]);
        char* k = col(f, tmp, YELLOW);
        sb_appendf(&sb, "  %s  =  \"%s\"", k, values[i]);
        free(k);
        if (i + 1 < n) sb_appendc(&sb, '\n');
    }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

char* fmt_processes(const OutputFmt* f, const Process* procs, int n) {
    if (n == 0) return strdup("No processes.");
    StrBuf sb; sb_init(&sb);
    char* hdr = col(f, "   PID  PPID  STATE         NAME", BOLD);
    sb_append(&sb, hdr); sb_appendc(&sb, '\n'); free(hdr);
    sb_append(&sb, "--------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        char line[256]; process_status_line(&procs[i], line, sizeof(line));
        sb_append(&sb, line);
        if (i + 1 < n) sb_appendc(&sb, '\n');
    }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

char* fmt_threads(const OutputFmt* f, const SimThread* threads, int n) {
    if (n == 0) return strdup("No threads.");
    StrBuf sb; sb_init(&sb);
    char* hdr = col(f, "   TID   PID  STATE         NAME", BOLD);
    sb_append(&sb, hdr); sb_appendc(&sb, '\n'); free(hdr);
    sb_append(&sb, "--------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        char line[256]; thread_status_line(&threads[i], line, sizeof(line));
        sb_append(&sb, line);
        if (i + 1 < n) sb_appendc(&sb, '\n');
    }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

char* fmt_pipes(const OutputFmt* f, const Pipe* pipes, int n) {
    if (n == 0) return strdup("No pipes. Use 'mkpipe' to create one.");
    StrBuf sb; sb_init(&sb);
    char* hdr = col(f, "  ID  BUFFERED", BOLD);
    sb_append(&sb, hdr); sb_appendc(&sb, '\n'); free(hdr);
    sb_append(&sb, "----------------\n");
    for (int i = 0; i < n; i++) {
        sb_appendf(&sb, "%4d  %8d", pipes[i].id, pipes[i].count);
        if (i + 1 < n) sb_appendc(&sb, '\n');
    }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

char* fmt_mutexes(const OutputFmt* f, const MutexSim* mtxs, int n) {
    if (n == 0) return strdup("No mutexes. Use 'newmutex <name>' to create one.");
    StrBuf sb; sb_init(&sb);
    char* hdr = col(f, "  ID  NAME                STATUS", BOLD);
    sb_append(&sb, hdr); sb_appendc(&sb, '\n'); free(hdr);
    sb_append(&sb, "--------------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        char status[64]; mutex_status_str(&mtxs[i], status, sizeof(status));
        sb_appendf(&sb, "%4d  %-20s  %s", mtxs[i].id, mtxs[i].name, status);
        if (i + 1 < n) sb_appendc(&sb, '\n');
    }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

char* fmt_ok (const OutputFmt* f, const char* msg) { return col(f, msg, GREEN);  }
char* fmt_inf(const OutputFmt* f, const char* msg) { return col(f, msg, YELLOW); }
char* fmt_err(const OutputFmt* f, const char* msg) {
    char buf[4096]; snprintf(buf, sizeof(buf), "Error: %s", msg);
    return col(f, buf, RED);
}
