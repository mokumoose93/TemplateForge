#pragma once
#include "filesystem.h"
#include "template_manager.h"
#include "variable_manager.h"
#include "process_manager.h"
#include "output_formatter.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// AppContext — bundles all subsystems for the dispatcher
// ---------------------------------------------------------------------------
struct AppContext {
    FileSystemModel&  fs;
    TemplateManager&  tm;
    VariableManager&  vm;
    ProcessManager&   pm;
    OutputFormatter&  view;
};

// ---------------------------------------------------------------------------
// parse()     — tokenise a raw input line (respects single/double quotes)
// dispatch()  — route tokens to the correct subsystem, return output string
//
// Special return values from dispatch():
//   "::EXIT"        — REPL should terminate
//   "::CLEAR"       — REPL should clear screen
//   "::DEFINE:<n>"  — REPL should enter interactive define mode for template <n>
// ---------------------------------------------------------------------------
std::vector<std::string> parse(const std::string& raw);

std::string dispatch(const std::vector<std::string>& tokens, AppContext& ctx);

// Returns the built-in help text (optionally for a specific topic)
std::string help_text(const std::string& topic = "");
