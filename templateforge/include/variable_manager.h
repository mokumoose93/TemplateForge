#pragma once

/* Replaces C++ VariableManager (std::map<string,string> + std::regex). */
typedef struct {
    char* name;   /* owned */
    char* value;  /* owned */
} VarEntry;

typedef struct {
    VarEntry* entries;
    int       count;
    int       cap;
} VarManager;

void        vm_init      (VarManager* vm);
void        vm_free      (VarManager* vm);
/* Returns 0 on success, -1 if name is invalid (writes to errbuf). */
int         vm_set       (VarManager* vm, const char* name, const char* value,
                          char* errbuf, int errsz);
int         vm_unset     (VarManager* vm, const char* name); /* 1=removed, 0=not found */
const char* vm_get       (const VarManager* vm, const char* name); /* NULL if absent */

/* Replace all {{VAR}} occurrences.  Returns heap string; caller must free(). */
char*       vm_substitute(const VarManager* vm, const char* text);
