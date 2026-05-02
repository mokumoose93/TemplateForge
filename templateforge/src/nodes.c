#include "nodes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- AccessMetadata ---- */

static char pbit(int p, int bit, char c) { return (p & bit) ? c : '-'; }

void access_perm_string(const AccessMetadata* a, char out[10]) {
    out[0] = pbit(a->owner_perms, PERM_READ,    'r');
    out[1] = pbit(a->owner_perms, PERM_WRITE,   'w');
    out[2] = pbit(a->owner_perms, PERM_EXECUTE, 'x');
    out[3] = pbit(a->group_perms, PERM_READ,    'r');
    out[4] = pbit(a->group_perms, PERM_WRITE,   'w');
    out[5] = pbit(a->group_perms, PERM_EXECUTE, 'x');
    out[6] = pbit(a->other_perms, PERM_READ,    'r');
    out[7] = pbit(a->other_perms, PERM_WRITE,   'w');
    out[8] = pbit(a->other_perms, PERM_EXECUTE, 'x');
    out[9] = '\0';
}

void access_octal_string(const AccessMetadata* a, char out[4]) {
    snprintf(out, 4, "%d%d%d", a->owner_perms, a->group_perms, a->other_perms);
}

int access_from_octal(const char* s, int* op, int* gp, int* xp) {
    if (!s || strlen(s) != 3) return 0;
    for (int i = 0; i < 3; i++) if (s[i] < '0' || s[i] > '7') return 0;
    *op = s[0] - '0'; *gp = s[1] - '0'; *xp = s[2] - '0';
    return 1;
}

/* ---- Internal access defaults ---- */

static void access_dir(AccessMetadata* a, const char* owner) {
    snprintf(a->owner, sizeof(a->owner), "%s", owner ? owner : "user");
    snprintf(a->group, sizeof(a->group), "users");
    a->owner_perms = PERM_RWX; a->group_perms = PERM_RX; a->other_perms = PERM_RX;
}
static void access_file(AccessMetadata* a, const char* owner) {
    snprintf(a->owner, sizeof(a->owner), "%s", owner ? owner : "user");
    snprintf(a->group, sizeof(a->group), "users");
    a->owner_perms = PERM_RW; a->group_perms = PERM_READ; a->other_perms = PERM_READ;
}
static void access_symlink(AccessMetadata* a, const char* owner) {
    snprintf(a->owner, sizeof(a->owner), "%s", owner ? owner : "user");
    snprintf(a->group, sizeof(a->group), "users");
    a->owner_perms = PERM_RWX; a->group_perms = PERM_RX; a->other_perms = PERM_RX;
}

/* ---- Node creation ---- */

Node* node_new_file(const char* name, const char* content, const char* owner) {
    Node* n = calloc(1, sizeof(Node));
    n->name         = strdup(name ? name : "");
    n->type         = NT_FILE;
    n->file.content = strdup(content ? content : "");
    access_file(&n->access, owner);
    return n;
}

Node* node_new_dir(const char* name, const char* owner) {
    Node* n = calloc(1, sizeof(Node));
    n->name = strdup(name ? name : "");
    n->type = NT_DIR;
    access_dir(&n->access, owner);
    return n;
}

Node* node_new_symlink(const char* name, const char* target, const char* owner) {
    Node* n = calloc(1, sizeof(Node));
    n->name           = strdup(name ? name : "");
    n->type           = NT_SYMLINK;
    n->symlink.target = strdup(target ? target : "");
    access_symlink(&n->access, owner);
    return n;
}

/* ---- node_free (recursive for dirs) ---- */

void node_free(Node* n) {
    if (!n) return;
    free(n->name);
    switch (n->type) {
        case NT_FILE:
            free(n->file.content);
            break;
        case NT_DIR:
            for (int i = 0; i < n->dir.count; i++) {
                free(n->dir.entries[i].key);
                node_free(n->dir.entries[i].node);
            }
            free(n->dir.entries);
            break;
        case NT_SYMLINK:
            free(n->symlink.target);
            break;
    }
    free(n);
}

char node_type_label(const Node* n) {
    if (n->type == NT_DIR)     return 'd';
    if (n->type == NT_SYMLINK) return 'l';
    return '-';
}

/* ---- Directory child operations ---- */
/* Children are kept in ascending alphabetical order by name. */

void dir_add_child(Node* dir, Node* child) {
    if (!dir || dir->type != NT_DIR || !child) return;
    child->parent = dir;
    if (dir->dir.count >= dir->dir.cap) {
        int nc = dir->dir.cap ? dir->dir.cap * 2 : 8;
        dir->dir.entries = realloc(dir->dir.entries, nc * sizeof(NodeChild));
        dir->dir.cap = nc;
    }
    /* find sorted insertion point */
    int pos = dir->dir.count;
    for (int i = 0; i < dir->dir.count; i++) {
        if (strcmp(child->name, dir->dir.entries[i].key) < 0) { pos = i; break; }
    }
    if (pos < dir->dir.count)
        memmove(&dir->dir.entries[pos + 1], &dir->dir.entries[pos],
                (dir->dir.count - pos) * sizeof(NodeChild));
    dir->dir.entries[pos].key  = strdup(child->name);
    dir->dir.entries[pos].node = child;
    dir->dir.count++;
}

Node* dir_remove_child(Node* dir, const char* name) {
    if (!dir || dir->type != NT_DIR) return NULL;
    for (int i = 0; i < dir->dir.count; i++) {
        if (strcmp(dir->dir.entries[i].key, name) == 0) {
            Node* removed = dir->dir.entries[i].node;
            removed->parent = NULL;
            free(dir->dir.entries[i].key);
            memmove(&dir->dir.entries[i], &dir->dir.entries[i + 1],
                    (dir->dir.count - i - 1) * sizeof(NodeChild));
            dir->dir.count--;
            return removed;
        }
    }
    return NULL;
}

Node* dir_get_child(const Node* dir, const char* name) {
    if (!dir || dir->type != NT_DIR) return NULL;
    for (int i = 0; i < dir->dir.count; i++)
        if (strcmp(dir->dir.entries[i].key, name) == 0)
            return dir->dir.entries[i].node;
    return NULL;
}

int dir_has_child(const Node* dir, const char* name) {
    return dir_get_child(dir, name) != NULL;
}

Node** dir_sorted_children(const Node* dir, int* count_out) {
    if (!dir || dir->type != NT_DIR) { *count_out = 0; return NULL; }
    Node** result = malloc(dir->dir.count * sizeof(Node*));
    int ri = 0;
    /* dirs first (entries already alpha-sorted within each group) */
    for (int i = 0; i < dir->dir.count; i++)
        if (dir->dir.entries[i].node->type == NT_DIR)
            result[ri++] = dir->dir.entries[i].node;
    for (int i = 0; i < dir->dir.count; i++)
        if (dir->dir.entries[i].node->type != NT_DIR)
            result[ri++] = dir->dir.entries[i].node;
    *count_out = ri;
    return result;
}

Node* node_deep_copy(const Node* src) {
    if (!src) return NULL;
    switch (src->type) {
        case NT_FILE: {
            Node* c = node_new_file(src->name, src->file.content, src->access.owner);
            c->access = src->access;
            return c;
        }
        case NT_SYMLINK: {
            Node* c = node_new_symlink(src->name, src->symlink.target, src->access.owner);
            c->access = src->access;
            return c;
        }
        case NT_DIR: {
            Node* c = node_new_dir(src->name, src->access.owner);
            c->access = src->access;
            for (int i = 0; i < src->dir.count; i++)
                dir_add_child(c, node_deep_copy(src->dir.entries[i].node));
            return c;
        }
    }
    return NULL;
}
