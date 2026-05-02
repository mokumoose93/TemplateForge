#pragma once
#include "filesystem.h"
#include "template_manager.h"
#include "variable_manager.h"
#include "process_manager.h"
#include "output_formatter.h"

/* Bundles all subsystems — replaces C++ AppContext. */
typedef struct {
    FSModel*    fs;
    TplManager* tm;
    VarManager* vm;
    ProcManager* pm;
    OutputFmt*  view;
} AppContext;

/* Tokenise a raw input line respecting single/double quotes.
   Fills tokens[0..max-1]; returns token count. */
int  cmd_parse(const char* raw, char** tokens, int max);
void cmd_free_tokens(char** tokens, int count);

/* Route tokens to the correct subsystem.
   Returns heap-allocated result string; caller must free().
   Special values: "::EXIT", "::CLEAR", "::DEFINE:<name>" */
char* cmd_dispatch(char** tokens, int count, AppContext* ctx);

/* Returns heap-allocated help text; caller frees. */
char* cmd_help_text(const char* topic);
