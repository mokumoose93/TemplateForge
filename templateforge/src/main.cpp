#include "filesystem.h"
#include "template_manager.h"
#include "variable_manager.h"
#include "command_parser.h"
#include "process_manager.h"
#include "output_formatter.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>  // for SetConsoleOutputCP / EnableVirtualTerminalProcessing
#endif

// ---------------------------------------------------------------------------
// Seed the filesystem with a minimal initial tree
// ---------------------------------------------------------------------------
static void seed_filesystem(FileSystemModel& fs, const std::string& normal_user) {
    for (auto& p : {"/home", "/home/user", "/tmp", "/var", "/bin"})
        fs.mkdir(p);
    // Transfer ownership so the normal user can write inside their home dir,
    // and /tmp is world-writable (777).
    fs.chown("/home/user", normal_user);
    fs.chmod("/tmp", "777");
    fs.touch("/home/user/.profile",
             "# User shell profile\nexport PATH=/bin:$PATH\n");
    fs.touch("/tmp/readme.txt",
             "Temporary directory — contents may not persist.\n");
    fs.cd("/home/user");
}

// ---------------------------------------------------------------------------
// Interactive template-definition sub-loop
// ---------------------------------------------------------------------------
static std::string enter_define_mode(
    const std::string& name,
    TemplateManager&   tm,
    OutputFormatter&   view)
{
    std::cout << view.inf(
        "Defining template '" + name + "' — enter paths one per line.\n"
        "  Directories  : end path with '/'  (e.g.  src/components/)\n"
        "  Files        : no trailing '/'     (e.g.  src/index.cpp)\n"
        "  Inline content: path::content      (e.g.  README.md::# My Project)\n"
        "  Finish       : type 'end' or leave a blank line."
    ) << "\n";

    std::vector<std::string> raw_paths;
    std::string line;
    while (true) {
        std::cout << "  define> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        // Trim leading/trailing whitespace
        auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) break;   // blank line = done
        line = line.substr(first);
        auto last = line.find_last_not_of(" \t");
        if (last != std::string::npos) line = line.substr(0, last + 1);
        if (line == "end" || line == "done") break;
        raw_paths.push_back(line);
    }

    if (raw_paths.empty())
        return view.err("define: no entries — template '" + name + "' not saved");

    try {
        const auto& tpl = tm.define(name, raw_paths);
        return view.ok("Template '" + name + "' defined  (" + tpl.summary() + ")");
    } catch (const std::exception& e) {
        return view.err(e.what());
    }
}

// ---------------------------------------------------------------------------
// Banner
// ---------------------------------------------------------------------------
static const char* BANNER =
    "╔══════════════════════════════════════════════════════╗\n"
    "║        TemplateForge  —  Simulated File System       ║\n"
    "║  Type 'help' for commands,  'exit' to quit           ║\n"
    "╚══════════════════════════════════════════════════════╝\n";

// ---------------------------------------------------------------------------
// Enable ANSI colour on Windows 10+
// ---------------------------------------------------------------------------
static void enable_ansi_on_windows() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

// ---------------------------------------------------------------------------
// REPL
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    enable_ansi_on_windows();

    // --user <name>  --no-color  options
    std::string initial_user = "user";
    bool        use_color    = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-color") {
            use_color = false;
        } else if (arg == "--user" && i + 1 < argc) {
            initial_user = argv[++i];
        }
    }

    FileSystemModel  fs("root");       // seed as root so mkdir on '/' is permitted
    TemplateManager  tm;
    VariableManager  vm;
    ProcessManager   pm;
    OutputFormatter  view(use_color);

    seed_filesystem(fs, initial_user);
    fs.current_user = initial_user;   // switch to the real user after seeding

    std::cout << BANNER << "\n";

    AppContext ctx{fs, tm, vm, pm, view};

    std::string line;
    while (true) {
        std::cout << view.format_prompt(fs.pwd(), fs.current_user) << std::flush;

        if (!std::getline(std::cin, line)) {
            std::cout << "\nexit\n";
            break;
        }

        auto tokens = parse(line);
        if (tokens.empty()) continue;

        std::string result = dispatch(tokens, ctx);

        if (result.empty())      continue;
        if (result == "::EXIT")  { std::cout << view.ok("Goodbye.") << "\n"; break; }
        if (result == "::CLEAR") {
#ifdef _WIN32
            system("cls");
#else
            system("clear");
#endif
            continue;
        }
        if (result.size() >= 9 && result.substr(0, 9) == "::DEFINE:") {
            std::string name = result.substr(9);  // after "::DEFINE:"
            std::cout << enter_define_mode(name, tm, view) << "\n";
            continue;
        }

        std::cout << result << "\n";
    }

    return 0;
}
