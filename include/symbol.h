#pragma once
#include "xld.h"

namespace xld::wasm {

template <typename E>
class InputFile;

template <typename E>
class ObjectFile;

// Symbol class represents a defined symbol.
template <typename E>
class Symbol {
  public:
    Symbol(std::string_view name, ObjectFile<E> *file)
        : name(name), file(file) {}

    // The name of the symbol.
    std::string_view name;

    // A symbol is owned (defined) by a file. If multiple files define the
    // symbol, the strongest binding is chosen.
    InputFile<E> *file = nullptr;

    // bool is_imported = false;
    // bool is_exported = false;

    bool is_weak = false;
};

// If we haven't seen the same `name` before, create a new instance
// of Symbol and returns it. Otherwise, returns the previously-
// instantiated object.
template <typename E>
Symbol<E> *get_symbol(Context<E> &ctx, std::string_view name) {
    typename decltype(ctx.symbol_map)::const_accessor acc;
    ctx.symbol_map.insert(acc, {name, Symbol<E>(name)});
    return const_cast<Symbol<E> *>(&acc->second);
}

} // namespace xld::wasm