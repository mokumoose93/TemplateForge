#include "variable_manager.h"
#include <regex>
#include <stdexcept>

void VariableManager::set(const std::string& name, const std::string& value) {
    // Validate: must start with letter/underscore, then alphanumeric/underscore
    static const std::regex valid_name(R"([A-Za-z_][A-Za-z0-9_]*)");
    if (!std::regex_match(name, valid_name))
        throw std::invalid_argument(
            "setvar: invalid variable name '" + name + "'");
    vars_[name] = value;
}

bool VariableManager::unset(const std::string& name) {
    return vars_.erase(name) > 0;
}

std::optional<std::string> VariableManager::get(const std::string& name) const {
    auto it = vars_.find(name);
    if (it == vars_.end()) return std::nullopt;
    return it->second;
}

const std::map<std::string, std::string>& VariableManager::all() const {
    return vars_;
}

std::string VariableManager::substitute(const std::string& text) const {
    static const std::regex ph(R"(\{\{([A-Za-z_][A-Za-z0-9_]*)\}\})");
    std::string result;
    auto it  = std::sregex_iterator(text.begin(), text.end(), ph);
    auto end = std::sregex_iterator();

    std::size_t last = 0;
    for (; it != end; ++it) {
        auto& m = *it;
        result += text.substr(last, m.position() - last);
        auto vi = vars_.find(m[1].str());
        result += (vi != vars_.end()) ? vi->second : m[0].str();
        last = m.position() + m.length();
    }
    result += text.substr(last);
    return result;
}
