#include "template_manager.h"
#include "str_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void tm_init(TplManager* tm) { tm->defs = NULL; tm->count = tm->cap = 0; }

static void tpldef_free(TplDef* d) {
    free(d->name);
    for (int i = 0; i < d->count; i++) { free(d->entries[i].path); free(d->entries[i].content); }
    free(d->entries);
}

void tm_free(TplManager* tm) {
    for (int i = 0; i < tm->count; i++) tpldef_free(&tm->defs[i]);
    free(tm->defs); tm->defs = NULL; tm->count = tm->cap = 0;
}

static void trim(char* s) {
    int l = 0, r = (int)strlen(s) - 1;
    while (s[l] == ' ' || s[l] == '\t') l++;
    while (r > l && (s[r] == ' ' || s[r] == '\t')) r--;
    int n = r - l + 1; if (n < 0) n = 0;
    memmove(s, s + l, n); s[n] = '\0';
}

int tm_define(TplManager* tm, const char* name,
              const char** raw_paths, int n,
              char* eb, int esz) {
    if (!name || !*name) { snprintf(eb, esz, "Template name cannot be empty"); return -1; }

    TplDef tpl; memset(&tpl, 0, sizeof(tpl));
    tpl.name = strdup(name);

    for (int i = 0; i < n; i++) {
        char raw[2048]; snprintf(raw, sizeof(raw), "%s", raw_paths[i]);
        trim(raw); if (!raw[0]) continue;

        char* content  = "";
        char* path_part = raw;
        char content_buf[2048] = {0};

        char* sep = strstr(raw, "::");
        if (sep) { *sep = '\0'; path_part = raw; snprintf(content_buf, sizeof(content_buf), "%s", sep + 2); content = content_buf; }

        TplEntry entry;
        int plen = (int)strlen(path_part);
        if (plen > 0 && path_part[plen - 1] == '/') {
            path_part[plen - 1] = '\0';
            entry.path    = strdup(path_part);
            entry.is_dir  = 1;
            entry.content = strdup("");
        } else {
            entry.path    = strdup(path_part);
            entry.is_dir  = 0;
            entry.content = strdup(content);
        }

        /* Conflict checks (mirrors the C++ validation we added) */
        for (int j = 0; j < tpl.count; j++) {
            TplEntry* ex = &tpl.entries[j];
            if (strcmp(ex->path, entry.path) == 0) {
                if (ex->is_dir != entry.is_dir) {
                    snprintf(eb, esz, "define: path '%s' cannot be both a file and a directory", entry.path);
                    free(entry.path); free(entry.content); tpldef_free(&tpl); return -1;
                }
                break;
            }
            int ep = (int)strlen(entry.path), xp = (int)strlen(ex->path);
            /* existing file used as dir ancestor of new path */
            if (!ex->is_dir && ep > xp + 1 && strncmp(entry.path, ex->path, xp) == 0 && entry.path[xp] == '/') {
                snprintf(eb, esz, "define: '%s' is already defined as a file but '%s' treats it as a directory", ex->path, entry.path);
                free(entry.path); free(entry.content); tpldef_free(&tpl); return -1;
            }
            /* new file used as dir ancestor of existing path */
            if (!entry.is_dir && xp > ep + 1 && strncmp(ex->path, entry.path, ep) == 0 && ex->path[ep] == '/') {
                snprintf(eb, esz, "define: '%s' is defined as a file but '%s' treats it as a directory", entry.path, ex->path);
                free(entry.path); free(entry.content); tpldef_free(&tpl); return -1;
            }
        }

        if (tpl.count >= tpl.cap) {
            int nc = tpl.cap ? tpl.cap * 2 : 8;
            tpl.entries = realloc(tpl.entries, nc * sizeof(TplEntry));
            tpl.cap = nc;
        }
        tpl.entries[tpl.count++] = entry;
    }

    /* Replace or insert */
    for (int i = 0; i < tm->count; i++) {
        if (strcmp(tm->defs[i].name, name) == 0) { tpldef_free(&tm->defs[i]); tm->defs[i] = tpl; return 0; }
    }
    if (tm->count >= tm->cap) {
        int nc = tm->cap ? tm->cap * 2 : 8;
        tm->defs = realloc(tm->defs, nc * sizeof(TplDef)); tm->cap = nc;
    }
    tm->defs[tm->count++] = tpl;
    return 0;
}

int tm_has(const TplManager* tm, const char* name) {
    for (int i = 0; i < tm->count; i++) if (strcmp(tm->defs[i].name, name) == 0) return 1;
    return 0;
}

int tm_remove(TplManager* tm, const char* name) {
    for (int i = 0; i < tm->count; i++) {
        if (strcmp(tm->defs[i].name, name) == 0) {
            tpldef_free(&tm->defs[i]);
            memmove(&tm->defs[i], &tm->defs[i+1], (tm->count-i-1)*sizeof(TplDef));
            tm->count--; return 1;
        }
    }
    return 0;
}

const TplDef* tm_get(const TplManager* tm, const char* name) {
    for (int i = 0; i < tm->count; i++) if (strcmp(tm->defs[i].name, name) == 0) return &tm->defs[i];
    return NULL;
}

char* tm_summary(const TplDef* def) {
    int dirs = 0, files = 0;
    for (int i = 0; i < def->count; i++) { if (def->entries[i].is_dir) dirs++; else files++; }
    char buf[256];
    snprintf(buf, sizeof(buf), "'%s': %d director%s, %d file%s",
             def->name, dirs, dirs==1?"y":"ies", files, files==1?"":"s");
    return strdup(buf);
}

char** tm_build(const TplManager* tm, const char* name,
                const char* target_path,
                FSModel* fs, VarManager* vm,
                int* count_out, char* eb, int esz) {
    const TplDef* tpl = tm_get(tm, name);
    if (!tpl) { snprintf(eb, esz, "build: template '%s' is not defined", name); return NULL; }

    Node* existing = fs_get_node(fs, target_path, 1);
    if (!existing) { if (fs_mkdir(fs, target_path, eb, esz) != 0) return NULL; }
    else if (existing->type != NT_DIR) { snprintf(eb, esz, "build: target '%s' exists and is not a directory", target_path); return NULL; }

    /* strip trailing slash from base */
    char base[1024]; snprintf(base, sizeof(base), "%s", target_path);
    int bl = (int)strlen(base); while (bl > 0 && base[bl-1] == '/') base[--bl] = '\0';

    char** created = malloc(tpl->count * sizeof(char*));
    *count_out = 0;

    for (int i = 0; i < tpl->count; i++) {
        char* rel  = vm_substitute(vm, tpl->entries[i].path);
        char full[2048]; snprintf(full, sizeof(full), "%s/%s", base, rel);
        free(rel);

        if (tpl->entries[i].is_dir) {
            if (fs_mkdir(fs, full, eb, esz) != 0) {
                for (int j = 0; j < *count_out; j++) free(created[j]);
                free(created); return NULL;
            }
        } else {
            /* ensure parent dirs exist */
            char* slash = strrchr(full, '/');
            if (slash) {
                char parent[2048]; int pl = (int)(slash - full);
                memcpy(parent, full, pl); parent[pl] = '\0';
                if (pl > 0 && !fs_get_node(fs, parent, 1))
                    fs_mkdir(fs, parent, eb, esz);
            }
            char* content = vm_substitute(vm, tpl->entries[i].content);
            int rc = fs_touch(fs, full, content, eb, esz);
            free(content);
            if (rc != 0) {
                for (int j = 0; j < *count_out; j++) free(created[j]);
                free(created); return NULL;
            }
        }
        created[(*count_out)++] = strdup(full);
    }
    return created;
}
