#pragma once

#include "context.h"
#include <string_view>

namespace xld::wasm {

// forward-decl
class InputFile;

// Global or weak symbol. This class is not instanciated for local symbols.
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

    // whether or not the symbol is originally exported
    bool is_exported = false;
    bool is_alive = false;

    u32 index = 0;

    enum class Binding {
        Weak,
        Global,
    } binding = Binding::Global;

    enum class Visibility {
        Default,
        Hidden,
    } visibility = Visibility::Default;
};

Symbol *get_symbol(Context &ctx, std::string_view name);

bool should_export_symbol(Context &ctx, Symbol *sym);

bool should_import_symbol(Context &ctx, Symbol *sym);

bool allow_undefined_symbol(Context &ctx, Symbol *sym);

} // namespace xld::wasm