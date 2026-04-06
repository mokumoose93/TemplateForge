#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

class FileSystemModel;
class VariableManager;

// ---------------------------------------------------------------------------
// TemplateEntry — one path spec inside a template definition
// ---------------------------------------------------------------------------
struct TemplateEntry {
    std::string path;
    bool        is_dir  {false};
    std::string content {};    // may contain {{VAR}} placeholders
};

// ---------------------------------------------------------------------------
// TemplateDefinition — a named set of path specs
// ---------------------------------------------------------------------------
struct TemplateDefinition {
    std::string               name;
    std::vector<TemplateEntry> entries;

    std::string summary() const;
};

// ---------------------------------------------------------------------------
// TemplateManager
// ---------------------------------------------------------------------------
class TemplateManager {
public:
    // Register a template from raw path strings.
    // Paths ending with '/' are directories; others are files.
    // Inline file content: "path/file.txt::content here"
    const TemplateDefinition& define(const std::string& name,
                                     const std::vector<std::string>& raw_paths);

    bool has(const std::string& name) const;
    bool remove(const std::string& name);

    const TemplateDefinition* get(const std::string& name) const;

    std::vector<const TemplateDefinition*> list() const;

    // Instantiate a template under target_path.
    // Returns list of created paths.  Throws on error.
    std::vector<std::string> build(const std::string& name,
                                   const std::string& target_path,
                                   FileSystemModel&   fs,
                                   VariableManager&   vm) const;

private:
    std::map<std::string, TemplateDefinition> templates_;
};
