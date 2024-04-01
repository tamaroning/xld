#pragma once
#include "wasm.h"
#include "xld.h"

namespace xld::wasm {

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

    //bool is_imported = false;
    //bool is_exported = false;

    bool is_weak = false;
};

} // namespace xld::wasm