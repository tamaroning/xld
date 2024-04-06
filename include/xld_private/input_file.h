#pragma once

#include "common/mmap.h"
#include "common/system.h"
#include "wasm_symbol.h"

namespace xld::wasm {

// forward-decl
class Context;

class InputFile {
  public:
    // Only object file is supported
    enum Kind {
        Object,
    };

    InputFile(Context &ctx, const std::string &filename, MappedFile *mf);

    virtual ~InputFile() = default;

    // template <typename T>
    // std::span<T> get_data(Context &ctx, i64 idx);

    virtual void resolve_symbols(Context &ctx) = 0;
    // void clear_symbols();

    // This pointer can be nullptr for the internal files
    MappedFile *mf = nullptr;
    std::string filename;
    Kind kind = Object;

    std::vector<WasmSymbol> symbols;
};

} // namespace xld::wasm