#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>

namespace ptxcc {

enum class SymbolType {
    INT,
    KERNEL
};

struct Symbol {
    std::string name;
    SymbolType type;
    size_t scope_depth{0};
};

class SymbolTable {
public:
    SymbolTable() {
        enter_scope(); // Global scope
    }

    void enter_scope() {
        scopes_.emplace_back();
    }

    void exit_scope() {
        if (!scopes_.empty()) {
            scopes_.pop_back();
        }
    }

    size_t current_depth() const {
        return scopes_.empty() ? 0 : scopes_.size() - 1;
    }

    bool define(const std::string& name, SymbolType type) {
        if (scopes_.empty()) return false;
        auto& current_scope = scopes_.back();
        if (current_scope.find(name) != current_scope.end()) {
            return false; // Already declared in this scope
        }
        current_scope[name] = Symbol{name, type, current_depth()};
        return true;
    }

    std::optional<Symbol> lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return std::nullopt;
    }

    bool is_declared_in_current_scope(const std::string& name) const {
        if (scopes_.empty()) return false;
        return scopes_.back().find(name) != scopes_.back().end();
    }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

} // namespace ptxcc
