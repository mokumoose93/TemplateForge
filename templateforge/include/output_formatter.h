#pragma once
#include "nodes.h"
#include "process_manager.h"

/* Replaces C++ OutputFormatter class.
   All format_* functions return heap-allocated strings; caller must free(). */
typedef struct {
    int use_color;
} OutputFmt;

void  fmt_init(OutputFmt* f, int use_color);

/* Prompt / navigation */
char* fmt_pwd   (const OutputFmt* f, const char* path);
char* fmt_prompt(const OutputFmt* f, const char* cwd, const char* user);

/* Filesystem */
char* fmt_ls  (const OutputFmt* f, Node** nodes, int count, int long_fmt);
char* fmt_tree(const OutputFmt* f, const Node* node,
               const char* prefix, int is_last);

/* Variables (entries are name/value pairs; n is the count) */
char* fmt_vars(const OutputFmt* f,
               const char** names, const char** values, int n);

/* OS simulation */
char* fmt_processes(const OutputFmt* f, const Process* procs,  int n);
char* fmt_threads  (const OutputFmt* f, const SimThread* threads, int n);
char* fmt_pipes    (const OutputFmt* f, const Pipe* pipes,     int n);
char* fmt_mutexes  (const OutputFmt* f, const MutexSim* mtxs,  int n);

/* Feedback — all return heap strings; caller frees */
char* fmt_ok (const OutputFmt* f, const char* msg);
char* fmt_err(const OutputFmt* f, const char* msg);
char* fmt_inf(const OutputFmt* f, const char* msg);
