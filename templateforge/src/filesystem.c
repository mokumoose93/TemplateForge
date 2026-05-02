#include "filesystem.h"
#include "str_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SEGS  128
#define SEG_LEN   256
#define SYM_LIMIT 40

typedef struct { char s[MAX_SEGS][SEG_LEN]; int n; } PathSegs;

/* ---- Path utilities ---- */

static void segs_from_cwd(const FSModel* fs, PathSegs* out) {
    const Node* parts[MAX_SEGS];
    int n = 0;
    const Node* cur = fs->cwd;
    while (cur && cur != fs->root) { if (n < MAX_SEGS) parts[n++] = cur; cur = cur->parent; }
    out->n = 0;
    for (int i = n - 1; i >= 0 && out->n < MAX_SEGS; i--)
        snprintf(out->s[out->n++], SEG_LEN, "%s", parts[i]->name);
}

static void resolve_path(const FSModel* fs, const char* path, PathSegs* out) {
    out->n = 0;
    if (!path || !path[0]) { segs_from_cwd(fs, out); return; }
    if (path[0] != '/') segs_from_cwd(fs, out);
    const char* p = path;
    while (*p) {
        if (*p == '/') { p++; continue; }
        const char* end = p;
        while (*end && *end != '/') end++;
        int len = (int)(end - p);
        if (len == 1 && p[0] == '.') { /* skip */ }
        else if (len == 2 && p[0] == '.' && p[1] == '.') { if (out->n > 0) out->n--; }
        else if (out->n < MAX_SEGS) {
            int cl = len < SEG_LEN - 1 ? len : SEG_LEN - 1;
            memcpy(out->s[out->n], p, cl);
            out->s[out->n][cl] = '\0';
            out->n++;
        }
        p = end;
    }
}

static Node* node_at_segs(const FSModel* fs, const PathSegs* segs,
                           int follow, int depth);

static Node* resolve_symlink(const FSModel* fs, const Node* ln, int depth) {
    if (depth > SYM_LIMIT || !ln || ln->type != NT_SYMLINK) return (Node*)ln;
    PathSegs segs; resolve_path(fs, ln->symlink.target, &segs);
    Node* t = node_at_segs(fs, &segs, 0, depth);
    if (!t) return NULL;
    return t->type == NT_SYMLINK ? resolve_symlink(fs, t, depth + 1) : t;
}

static Node* node_at_segs(const FSModel* fs, const PathSegs* segs,
                           int follow, int depth) {
    Node* cur = fs->root;
    for (int i = 0; i < segs->n; i++) {
        if (cur->type != NT_DIR) return NULL;
        Node* child = dir_get_child(cur, segs->s[i]);
        if (!child) return NULL;
        if (follow && child->type == NT_SYMLINK) {
            child = resolve_symlink(fs, child, depth + 1);
            if (!child) return NULL;
        }
        cur = child;
    }
    return cur;
}

Node* fs_get_node(const FSModel* fs, const char* path, int follow) {
    PathSegs segs; resolve_path(fs, path, &segs);
    return segs.n == 0 ? fs->root : node_at_segs(fs, &segs, follow, 0);
}

/* Returns parent directory and fills name_out; NULL if path resolves to root */
static Node* parent_and_name(const FSModel* fs, const char* path,
                              char* name_out, int name_sz) {
    PathSegs segs; resolve_path(fs, path, &segs);
    if (segs.n == 0) return NULL;
    snprintf(name_out, name_sz, "%s", segs.s[segs.n - 1]);
    segs.n--;
    return segs.n == 0 ? fs->root : node_at_segs(fs, &segs, 1, 0);
}

/* ---- Permission helpers ---- */

static int check_perm(const FSModel* fs, const Node* n, int req) {
    if (strcmp(fs->current_user, "root") == 0) return 1;
    int eff = strcmp(fs->current_user, n->access.owner) == 0
              ? n->access.owner_perms : n->access.other_perms;
    return (eff & req) == req;
}

static int require_perm(const FSModel* fs, const Node* n, int req,
                        const char* verb, char* eb, int esz) {
    if (!check_perm(fs, n, req)) {
        snprintf(eb, esz, "Permission denied: cannot %s '%s' (user: %s)",
                 verb, n->name, fs->current_user);
        return -1;
    }
    return 0;
}

/* ---- Init / Free ---- */

void fs_init(FSModel* fs, const char* user) {
    snprintf(fs->current_user, sizeof(fs->current_user), "%s", user ? user : "user");
    fs->root = node_new_dir("/", "root");
    fs->root->access.owner_perms = PERM_RWX;
    fs->root->access.group_perms = PERM_RX;
    fs->root->access.other_perms = PERM_RX;
    fs->cwd = fs->root;
}

void fs_free(FSModel* fs) { node_free(fs->root); fs->root = fs->cwd = NULL; }

/* ---- Navigation ---- */

char* fs_pwd(const FSModel* fs) {
    const Node* parts[MAX_SEGS]; int n = 0;
    const Node* cur = fs->cwd;
    while (cur && cur != fs->root) { if (n < MAX_SEGS) parts[n++] = cur; cur = cur->parent; }
    if (n == 0) return strdup("/");
    StrBuf sb; sb_init(&sb);
    for (int i = n - 1; i >= 0; i--) { sb_appendc(&sb, '/'); sb_append(&sb, parts[i]->name); }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}

int fs_cd(FSModel* fs, const char* path, char* eb, int esz) {
    Node* n = fs_get_node(fs, path, 1);
    if (!n)                { snprintf(eb, esz, "cd: no such directory: '%s'", path); return -1; }
    if (n->type != NT_DIR) { snprintf(eb, esz, "cd: not a directory: '%s'",   path); return -1; }
    if (require_perm(fs, n, PERM_EXECUTE, "enter", eb, esz) != 0) return -1;
    fs->cwd = n; return 0;
}

Node** fs_ls(const FSModel* fs, const char* path, int* count, char* eb, int esz) {
    Node* target = (path && *path) ? fs_get_node(fs, path, 1) : (Node*)fs->cwd;
    if (!target) { snprintf(eb, esz, "ls: no such file or directory: '%s'", path); *count = 0; return NULL; }
    if (target->type != NT_DIR) {
        Node** arr = malloc(sizeof(Node*)); arr[0] = target; *count = 1; return arr;
    }
    if (require_perm(fs, target, PERM_READ, "list", eb, esz) != 0) { *count = 0; return NULL; }
    return dir_sorted_children(target, count);
}

/* ---- Structural operations ---- */

int fs_mkdir(FSModel* fs, const char* path, char* eb, int esz) {
    PathSegs segs; resolve_path(fs, path, &segs);
    if (segs.n == 0) { snprintf(eb, esz, "mkdir: cannot create root"); return -1; }
    Node* cur = fs->root;
    for (int i = 0; i < segs.n; i++) {
        if (cur->type != NT_DIR) { snprintf(eb, esz, "mkdir: '%s' is not a directory", cur->name); return -1; }
        Node* child = dir_get_child(cur, segs.s[i]);
        if (!child) {
            if (require_perm(fs, cur, PERM_WRITE, "write to", eb, esz) != 0) return -1;
            Node* nd = node_new_dir(segs.s[i], fs->current_user);
            dir_add_child(cur, nd); cur = nd;
        } else if (child->type == NT_DIR) {
            cur = child;
        } else {
            snprintf(eb, esz, "mkdir: '%s' exists and is not a directory", segs.s[i]); return -1;
        }
    }
    return 0;
}

int fs_touch(FSModel* fs, const char* path, const char* content, char* eb, int esz) {
    char name[SEG_LEN];
    Node* parent = parent_and_name(fs, path, name, sizeof(name));
    if (!parent) { snprintf(eb, esz, "touch: no such parent directory for '%s'", path); return -1; }
    if (require_perm(fs, parent, PERM_WRITE, "write to", eb, esz) != 0) return -1;
    Node* ex = dir_get_child(parent, name);
    if (ex) {
        if (ex->type != NT_FILE) { snprintf(eb, esz, "touch: '%s' exists and is not a file", name); return -1; }
        if (require_perm(fs, ex, PERM_WRITE, "write to", eb, esz) != 0) return -1;
        free(ex->file.content); ex->file.content = strdup(content ? content : "");
    } else {
        dir_add_child(parent, node_new_file(name, content ? content : "", fs->current_user));
    }
    return 0;
}

int fs_rm(FSModel* fs, const char* path, int recursive, char* eb, int esz) {
    char name[SEG_LEN];
    Node* parent = parent_and_name(fs, path, name, sizeof(name));
    if (!parent) { snprintf(eb, esz, "rm: cannot remove '%s'", path); return -1; }
    Node* n = dir_get_child(parent, name);
    if (!n)     { snprintf(eb, esz, "rm: no such file or directory: '%s'", path); return -1; }
    if (require_perm(fs, parent, PERM_WRITE, "remove from", eb, esz) != 0) return -1;
    if (n->type == NT_DIR && !recursive && n->dir.count > 0) {
        snprintf(eb, esz, "rm: '%s' is non-empty (use -r)", path); return -1;
    }
    /* reset cwd if it sits inside the removed subtree */
    for (Node* c = fs->cwd; c; c = c->parent) { if (c == n) { fs->cwd = fs->root; break; } }
    node_free(dir_remove_child(parent, name));
    return 0;
}

int fs_mv(FSModel* fs, const char* src, const char* dst, char* eb, int esz) {
    char sname[SEG_LEN];
    Node* sp = parent_and_name(fs, src, sname, sizeof(sname));
    if (!sp) { snprintf(eb, esz, "mv: cannot move '%s'", src); return -1; }
    Node* n = dir_get_child(sp, sname);
    if (!n) { snprintf(eb, esz, "mv: no such file or directory: '%s'", src); return -1; }
    if (require_perm(fs, sp, PERM_WRITE, "remove from", eb, esz) != 0) return -1;

    Node* dn = fs_get_node(fs, dst, 1);
    if (dn && dn->type == NT_DIR) {
        if (require_perm(fs, dn, PERM_WRITE, "write to", eb, esz) != 0) return -1;
        if (dir_has_child(dn, n->name)) { snprintf(eb, esz, "mv: '%s' already exists in destination", n->name); return -1; }
        dir_remove_child(sp, sname); dir_add_child(dn, n);
    } else {
        char dname[SEG_LEN];
        Node* dp = parent_and_name(fs, dst, dname, sizeof(dname));
        if (!dp) { snprintf(eb, esz, "mv: invalid destination: '%s'", dst); return -1; }
        if (require_perm(fs, dp, PERM_WRITE, "write to", eb, esz) != 0) return -1;
        dir_remove_child(sp, sname);
        Node* old = dir_remove_child(dp, dname); if (old) node_free(old);
        free(n->name); n->name = strdup(dname);
        dir_add_child(dp, n);
    }
    return 0;
}

int fs_cp(FSModel* fs, const char* src, const char* dst, char* eb, int esz) {
    Node* sn = fs_get_node(fs, src, 1);
    if (!sn) { snprintf(eb, esz, "cp: no such file or directory: '%s'", src); return -1; }
    if (require_perm(fs, sn, PERM_READ, "read", eb, esz) != 0) return -1;
    Node* clone = node_deep_copy(sn);

    Node* dn = fs_get_node(fs, dst, 1);
    if (dn && dn->type == NT_DIR) {
        if (require_perm(fs, dn, PERM_WRITE, "write to", eb, esz) != 0) { node_free(clone); return -1; }
        if (dir_has_child(dn, clone->name)) { snprintf(eb, esz, "cp: '%s' already exists in destination", clone->name); node_free(clone); return -1; }
        dir_add_child(dn, clone);
    } else {
        char dname[SEG_LEN];
        Node* dp = parent_and_name(fs, dst, dname, sizeof(dname));
        if (!dp) { snprintf(eb, esz, "cp: invalid destination: '%s'", dst); node_free(clone); return -1; }
        if (require_perm(fs, dp, PERM_WRITE, "write to", eb, esz) != 0) { node_free(clone); return -1; }
        Node* old = dir_remove_child(dp, dname); if (old) node_free(old);
        free(clone->name); clone->name = strdup(dname);
        dir_add_child(dp, clone);
    }
    return 0;
}

int fs_ln(FSModel* fs, const char* target, const char* link_path, char* eb, int esz) {
    char name[SEG_LEN];
    Node* parent = parent_and_name(fs, link_path, name, sizeof(name));
    if (!parent)               { snprintf(eb, esz, "ln: invalid link path: '%s'", link_path); return -1; }
    if (require_perm(fs, parent, PERM_WRITE, "write to", eb, esz) != 0) return -1;
    if (dir_has_child(parent, name)) { snprintf(eb, esz, "ln: '%s' already exists", name); return -1; }
    dir_add_child(parent, node_new_symlink(name, target, fs->current_user));
    return 0;
}

char* fs_cat(const FSModel* fs, const char* path, char* eb, int esz) {
    Node* n = fs_get_node(fs, path, 1);
    if (!n)                { snprintf(eb, esz, "cat: no such file: '%s'", path); return NULL; }
    if (n->type == NT_DIR) { snprintf(eb, esz, "cat: '%s' is a directory", path); return NULL; }
    if (require_perm(fs, n, PERM_READ, "read", eb, esz) != 0) return NULL;
    return strdup(n->file.content ? n->file.content : "");
}

int fs_write_file(FSModel* fs, const char* path, const char* content, char* eb, int esz) {
    Node* n = fs_get_node(fs, path, 1);
    if (!n)                  { snprintf(eb, esz, "write: no such file: '%s'", path); return -1; }
    if (n->type != NT_FILE)  { snprintf(eb, esz, "write: '%s' is not a file", path); return -1; }
    if (require_perm(fs, n, PERM_WRITE, "write to", eb, esz) != 0) return -1;
    free(n->file.content); n->file.content = strdup(content ? content : "");
    return 0;
}

int fs_chmod(FSModel* fs, const char* path, const char* octal, char* eb, int esz) {
    Node* n = fs_get_node(fs, path, 0);
    if (!n) { snprintf(eb, esz, "chmod: no such file or directory: '%s'", path); return -1; }
    if (strcmp(fs->current_user, "root") != 0 && strcmp(fs->current_user, n->access.owner) != 0) {
        snprintf(eb, esz, "chmod: permission denied (only owner or root)"); return -1;
    }
    int op, gp, xp;
    if (!access_from_octal(octal, &op, &gp, &xp)) { snprintf(eb, esz, "chmod: invalid permission string '%s'", octal); return -1; }
    n->access.owner_perms = op; n->access.group_perms = gp; n->access.other_perms = xp;
    return 0;
}

int fs_chown(FSModel* fs, const char* path, const char* new_owner, char* eb, int esz) {
    if (strcmp(fs->current_user, "root") != 0) { snprintf(eb, esz, "chown: permission denied (root only)"); return -1; }
    Node* n = fs_get_node(fs, path, 0);
    if (!n) { snprintf(eb, esz, "chown: no such file or directory: '%s'", path); return -1; }
    snprintf(n->access.owner, sizeof(n->access.owner), "%s", new_owner);
    return 0;
}
