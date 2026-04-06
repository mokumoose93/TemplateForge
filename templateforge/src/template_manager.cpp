#include "template_manager.h"
#include "filesystem.h"
#include "variable_manager.h"
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// TemplateDefinition
// ---------------------------------------------------------------------------
std::string TemplateDefinition::summary() const {
    int dirs = 0, files = 0;
    for (auto& e : entries) { if (e.is_dir) ++dirs; else ++files; }
    std::ostringstream ss;
    ss << "'" << name << "': " << dirs << " director" << (dirs == 1 ? "y" : "ies")
       << ", " << files << " file" << (files == 1 ? "" : "s");
    return ss.str();
}

// ---------------------------------------------------------------------------
// TemplateManager::define
// ---------------------------------------------------------------------------
const TemplateDefinition& TemplateManager::define(
    const std::string& name, const std::vector<std::string>& raw_paths)
{
    if (name.empty())
        throw std::invalid_argument("Template name cannot be empty");

    TemplateDefinition tpl;
    tpl.name = name;

    for (auto raw : raw_paths) {
        // trim whitespace
        while (!raw.empty() && (raw.front() == ' ' || raw.front() == '\t'))
            raw.erase(raw.begin());
        while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t'))
            raw.pop_back();
        if (raw.empty()) continue;

        std::string content;
        std::string path_part = raw;

        // Split on "::" for inline file content
        auto sep = raw.find("::");
        if (sep != std::string::npos) {
            path_part = raw.substr(0, sep);
            content   = raw.substr(sep + 2);
        }

        TemplateEntry entry;
        if (!path_part.empty() && path_part.back() == '/') {
            entry.path   = path_part.substr(0, path_part.size() - 1);
            entry.is_dir = true;
        } else {
            entry.path    = path_part;
            entry.is_dir  = false;
            entry.content = content;
        }
        tpl.entries.push_back(std::move(entry));
    }

    templates_[name] = std::move(tpl);
    return templates_.at(name);
}

// ---------------------------------------------------------------------------
// TemplateManager::get / has / remove
// ---------------------------------------------------------------------------
bool TemplateManager::has(const std::string& name) const {
    return templates_.count(name) > 0;
}

bool TemplateManager::remove(const std::string& name) {
    return templates_.erase(name) > 0;
}

const TemplateDefinition* TemplateManager::get(const std::string& name) const {
    auto it = templates_.find(name);
    return (it != templates_.end()) ? &it->second : nullptr;
}

std::vector<const TemplateDefinition*> TemplateManager::list() const {
    std::vector<const TemplateDefinition*> result;
    for (auto& [k, v] : templates_) result.push_back(&v);
    return result;
}

// ---------------------------------------------------------------------------
// TemplateManager::build
// ---------------------------------------------------------------------------
std::vector<std::string> TemplateManager::build(
    const std::string& name,
    const std::string& target_path,
    FileSystemModel&   fs,
    VariableManager&   vm) const
{
    const TemplateDefinition* tpl = get(name);
    if (!tpl)
        throw std::invalid_argument("build: template '" + name + "' is not defined");

    // Ensure the target directory exists
    auto existing = fs.get_node(target_path);
    if (!existing) {
        fs.mkdir(target_path);
    } else if (!existing->is_dir()) {
        throw FileSystemError(
            "build: target '" + target_path + "' exists and is not a directory");
    }

    std::vector<std::string> created;
    std::string base = target_path;
    while (!base.empty() && base.back() == '/') base.pop_back();

    for (auto& entry : tpl->entries) {
        std::string rel  = vm.substitute(entry.path);
        std::string full = base + "/" + rel;

        if (entry.is_dir) {
            fs.mkdir(full);
        } else {
            // Create intermediate directories if needed
            auto slash = full.rfind('/');
            if (slash != std::string::npos) {
                std::string parent_path = full.substr(0, slash);
                if (!parent_path.empty() && !fs.get_node(parent_path))
                    fs.mkdir(parent_path);
            }
            std::string content = vm.substitute(entry.content);
            fs.touch(full, content);
        }
        created.push_back(full);
    }
    return created;
}
