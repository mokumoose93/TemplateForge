#pragma once
#include "filesystem.h"
#include "variable_manager.h"

/* Replaces C++ TemplateEntry / TemplateDefinition / TemplateManager. */

typedef struct {
    char* path;     /* owned */
    int   is_dir;
    char* content;  /* owned, empty string for dirs */
} TplEntry;

typedef struct {
    char*     name;     /* owned */
    TplEntry* entries;
    int       count;
    int       cap;
} TplDef;

typedef struct {
    TplDef* defs;
    int     count;
    int     cap;
} TplManager;

void          tm_init   (TplManager* tm);
void          tm_free   (TplManager* tm);

/* Register a template.  raw_paths[n] are the user-entered lines.
   Returns 0 on success, -1 on conflict (writes errbuf). */
int           tm_define (TplManager* tm, const char* name,
                         const char** raw_paths, int n,
                         char* errbuf, int errsz);

int           tm_has    (const TplManager* tm, const char* name);
int           tm_remove (TplManager* tm, const char* name);
const TplDef* tm_get    (const TplManager* tm, const char* name);

/* Returns heap-allocated summary string "name: X dirs, Y files"; caller frees. */
char*         tm_summary(const TplDef* def);

/* Instantiate template at target_path.
   On success returns heap array of heap strings (created paths); sets *count.
   Caller must free each string then the array.  Returns NULL on error. */
char**        tm_build  (const TplManager* tm, const char* name,
                         const char* target_path,
                         FSModel* fs, VarManager* vm,
                         int* count, char* errbuf, int errsz);
