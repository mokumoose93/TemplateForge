#include "filesystem.h"
#include "template_manager.h"
#include "variable_manager.h"
#include "command_parser.h"
#include "process_manager.h"
#include "output_formatter.h"
#include "str_buf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ---- Seed the filesystem with a minimal initial tree ---- */
static void seed_filesystem(FSModel* fs, const char* normal_user) {
    char eb[256];
    const char* dirs[] = {"/home","/home/user","/tmp","/var","/bin"};
    for (int i = 0; i < 5; i++) fs_mkdir(fs, dirs[i], eb, sizeof(eb));
    fs_chown(fs, "/home/user", normal_user, eb, sizeof(eb));
    fs_chmod(fs, "/tmp", "777", eb, sizeof(eb));
    fs_touch(fs, "/home/user/.profile",
             "# User shell profile\nexport PATH=/bin:$PATH\n", eb, sizeof(eb));
    fs_touch(fs, "/tmp/readme.txt",
             "Temporary directory \xe2\x80\x94 contents may not persist.\n", eb, sizeof(eb));
    fs_cd(fs, "/home/user", eb, sizeof(eb));
}

/* ---- Interactive template-definition sub-loop ---- */
static char* enter_define_mode(const char* name, TplManager* tm, OutputFmt* view) {
    char* inf = fmt_inf(view,
        "Defining template — enter paths one per line.\n"
        "  Directories  : end path with '/'  (e.g.  src/components/)\n"
        "  Files        : no trailing '/'     (e.g.  src/index.cpp)\n"
        "  Inline content: path::content      (e.g.  README.md::# My Project)\n"
        "  Finish       : type 'end' or leave a blank line.");
    printf("%s\n", inf); free(inf);

    const char* raw_paths[512];
    char bufs[512][2048];
    int  n = 0;

    char line[2048];
    while (1) {
        printf("  define> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        /* strip newline */
        int l = (int)strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
        /* trim leading whitespace */
        int s = 0; while (line[s] == ' ' || line[s] == '\t') s++;
        /* trim trailing whitespace */
        int e = l - 1; while (e > s && (line[e] == ' ' || line[e] == '\t')) e--;
        char trimmed[2048]; int tlen = (e >= s) ? e - s + 1 : 0;
        memcpy(trimmed, line + s, tlen); trimmed[tlen] = '\0';
        if (!trimmed[0]) break;
        if (strcmp(trimmed,"end")==0 || strcmp(trimmed,"done")==0) break;
        if (n < 512) { snprintf(bufs[n], 2048, "%s", trimmed); raw_paths[n] = bufs[n]; n++; }
    }

    if (n == 0) {
        char eb[256]; snprintf(eb,sizeof(eb),"define: no entries — template '%s' not saved", name);
        return fmt_err(view, eb);
    }

    char eb[512];
    if (tm_define(tm, name, raw_paths, n, eb, sizeof(eb)) != 0) return fmt_err(view, eb);

    const TplDef* tpl = tm_get(tm, name);
    char* summary = tm_summary(tpl);
    char buf[512]; snprintf(buf,sizeof(buf),"Template '%s' defined  (%s)", name, summary);
    free(summary);
    char* r = fmt_ok(view, buf); return r;
}

/* ---- Enable ANSI colour on Windows 10+ ---- */
static void enable_ansi(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return;
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

static const char* BANNER =
    "\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97\n"
    "\xe2\x95\x91        TemplateForge  \xe2\x80\x94  Simulated File System       \xe2\x95\x91\n"
    "\xe2\x95\x91  Type 'help' for commands,  'exit' to quit           \xe2\x95\x91\n"
    "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\n";

/* ---- REPL ---- */
int main(int argc, char* argv[]) {
    enable_ansi();

    char initial_user[64] = "user";
    int  use_color = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i],"--no-color")==0) use_color = 0;
        else if (strcmp(argv[i],"--user")==0 && i+1 < argc)
            snprintf(initial_user, sizeof(initial_user), "%s", argv[++i]);
    }

    FSModel     fs; fs_init(&fs, "root");
    TplManager  tm; tm_init(&tm);
    VarManager  vm; vm_init(&vm);
    ProcManager pm; pm_init(&pm);
    OutputFmt   view; fmt_init(&view, use_color);

    seed_filesystem(&fs, initial_user);
    snprintf(fs.current_user, sizeof(fs.current_user), "%s", initial_user);

    printf("%s\n", BANNER);

    AppContext ctx = { &fs, &tm, &vm, &pm, &view };

    char line[4096];
    char* tokens[128];

    while (1) {
        char* pwd = fs_pwd(&fs);
        char* prompt = fmt_prompt(&view, pwd, fs.current_user);
        printf("%s", prompt); fflush(stdout);
        free(pwd); free(prompt);

        if (!fgets(line, sizeof(line), stdin)) { printf("\nexit\n"); break; }
        /* strip newline */
        int l = (int)strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';

        int count = cmd_parse(line, tokens, 128);
        if (count == 0) continue;

        char* result = cmd_dispatch(tokens, count, &ctx);
        cmd_free_tokens(tokens, count);

        if (!result || !result[0])      { free(result); continue; }
        if (strcmp(result,"::EXIT")==0)  { char* g = fmt_ok(&view,"Goodbye."); printf("%s\n",g); free(g); free(result); break; }
        if (strcmp(result,"::CLEAR")==0) {
#ifdef _WIN32
            system("cls");
#else
            system("clear");
#endif
            free(result); continue;
        }
        if (strncmp(result,"::DEFINE:",9)==0) {
            char* name = result + 9;
            char* out = enter_define_mode(name, &tm, &view);
            printf("%s\n", out); free(out); free(result); continue;
        }

        printf("%s\n", result); free(result);
    }

    /* cleanup */
    fs_free(&fs);
    tm_free(&tm);
    vm_free(&vm);
    pm_free(&pm);
    return 0;
}
