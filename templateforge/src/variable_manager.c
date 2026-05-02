#include "variable_manager.h"
#include "str_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int valid_name(const char* n) {
    if (!n || !*n) return 0;
    if (!isalpha((unsigned char)n[0]) && n[0] != '_') return 0;
    for (int i = 1; n[i]; i++)
        if (!isalnum((unsigned char)n[i]) && n[i] != '_') return 0;
    return 1;
}

void vm_init(VarManager* vm) { vm->entries = NULL; vm->count = vm->cap = 0; }

void vm_free(VarManager* vm) {
    for (int i = 0; i < vm->count; i++) { free(vm->entries[i].name); free(vm->entries[i].value); }
    free(vm->entries); vm->entries = NULL; vm->count = vm->cap = 0;
}

static VarEntry* vm_find(const VarManager* vm, const char* name) {
    for (int i = 0; i < vm->count; i++)
        if (strcmp(vm->entries[i].name, name) == 0) return &vm->entries[i];
    return NULL;
}

int vm_set(VarManager* vm, const char* name, const char* value, char* eb, int esz) {
    if (!valid_name(name)) { snprintf(eb, esz, "setvar: invalid variable name '%s'", name); return -1; }
    VarEntry* e = vm_find(vm, name);
    if (e) { free(e->value); e->value = strdup(value ? value : ""); return 0; }
    if (vm->count >= vm->cap) {
        int nc = vm->cap ? vm->cap * 2 : 8;
        vm->entries = realloc(vm->entries, nc * sizeof(VarEntry));
        vm->cap = nc;
    }
    vm->entries[vm->count].name  = strdup(name);
    vm->entries[vm->count].value = strdup(value ? value : "");
    vm->count++;
    return 0;
}

int vm_unset(VarManager* vm, const char* name) {
    for (int i = 0; i < vm->count; i++) {
        if (strcmp(vm->entries[i].name, name) == 0) {
            free(vm->entries[i].name); free(vm->entries[i].value);
            memmove(&vm->entries[i], &vm->entries[i+1], (vm->count - i - 1) * sizeof(VarEntry));
            vm->count--; return 1;
        }
    }
    return 0;
}

const char* vm_get(const VarManager* vm, const char* name) {
    VarEntry* e = vm_find(vm, name); return e ? e->value : NULL;
}

/* Manual {{VAR}} substitution — replaces std::regex scanning */
char* vm_substitute(const VarManager* vm, const char* text) {
    if (!text) return strdup("");
    StrBuf sb; sb_init(&sb);
    const char* p = text;
    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            const char* start = p + 2;
            const char* end   = strstr(start, "}}");
            if (end) {
                int nlen = (int)(end - start);
                char vname[256];
                if (nlen > 0 && nlen < (int)sizeof(vname)) {
                    memcpy(vname, start, nlen); vname[nlen] = '\0';
                    if (valid_name(vname)) {
                        const char* val = vm_get(vm, vname);
                        if (val) { sb_append(&sb, val); p = end + 2; continue; }
                    }
                }
            }
        }
        sb_appendc(&sb, *p++);
    }
    char* r = sb_dup(&sb); sb_free(&sb); return r;
}
