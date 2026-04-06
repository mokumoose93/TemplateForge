#pragma once
#include <string>
#include <map>
#include <optional>

// ---------------------------------------------------------------------------
// VariableManager — key/value store for template variable substitution
//
// Placeholders use {{VAR_NAME}} syntax.
// ---------------------------------------------------------------------------
class VariableManager {
public:
    void set(const std::string& name, const std::string& value);
    bool unset(const std::string& name);

    std::optional<std::string> get(const std::string& name) const;
    const std::map<std::string, std::string>& all() const;

    // Replace all {{VAR_NAME}} occurrences in text with stored values.
    // Unresolved placeholders are left as-is.
    std::string substitute(const std::string& text) const;

private:
    std::map<std::string, std::string> vars_;
};
