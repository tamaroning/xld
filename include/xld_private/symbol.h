#pragma once

#include "context.h"
#include "input_span.h"
#include <string_view>

namespace xld::wasm {

// forward-decl
class InputFile;

class Symbol {
  public:
    Symbol(std::string_view name, InputFile *file) : name(name), file(file) {}

    // The name of the symbol.
    std::string_view name;

    // A symbol is owned (defined) by a file. If multiple files define the
    // symbol, the strongest binding is chosen.
    InputFile *file = nullptr;

    bool is_defined() const { return file != nullptr; }
    bool is_undefined() const { return file == nullptr; }

    // bool is_imported = false;
    // bool is_exported = false;

    bool is_alive = false;

    enum class Binding {
        Weak,
        Global,
    } binding = Binding::Weak;
};

// If we haven't seen the same `name` before, create a new instance
// of Symbol and returns it. Otherwise, returns the previously-
// instantiated object.
inline Symbol *get_symbol(Context &ctx, std::string_view name) {
    typename decltype(ctx.symbol_map)::const_accessor acc;
    ctx.symbol_map.insert(acc, {name, Symbol(name, nullptr)});
    return const_cast<Symbol *>(&acc->second);
}

} // namespace xld::wasm