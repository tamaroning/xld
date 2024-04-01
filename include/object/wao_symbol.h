#pragma once
#include "wao_basic.h"

// Copy of LLVM

namespace xld::wasm {

class WasmSymbol {
  public:
    WasmSymbol(const wasm::WasmSymbolInfo &info,
               const wasm::WasmGlobalType *global_type,
               const wasm::WasmTableType *table_type,
               const wasm::WasmSignature *signature)
        : info(info), global_type(global_type), table_type(table_type),
          signature(signature) {
        assert(!signature ||
               signature->kind != wasm::WasmSignature::Placeholder);
    }

    // Symbol info as represented in the symbol's 'syminfo' entry of an object
    // file's symbol table.
    wasm::WasmSymbolInfo info;
    const wasm::WasmGlobalType *global_type;
    const wasm::WasmTableType *table_type;
    const wasm::WasmSignature *signature;

    bool is_type_function() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_FUNCTION;
    }

    bool is_type_table() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_TABLE;
    }

    bool is_type_data() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_DATA;
    }

    bool is_type_global() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_GLOBAL;
    }

    bool is_type_section() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_SECTION;
    }

    bool is_type_tag() const { return info.kind == wasm::WASM_SYMBOL_TYPE_TAG; }

    bool is_undefined() const {
        return (info.flags & wasm::WASM_SYMBOL_UNDEFINED) != 0;
    }

    bool is_binding_weak() const {
        return get_binding() == wasm::WASM_SYMBOL_BINDING_WEAK;
    }

    bool is_binding_global() const {
        return get_binding() == wasm::WASM_SYMBOL_BINDING_GLOBAL;
    }

    bool is_binding_local() const {
        return get_binding() == wasm::WASM_SYMBOL_BINDING_LOCAL;
    }

    unsigned get_binding() const {
        return info.flags & wasm::WASM_SYMBOL_BINDING_MASK;
    }

    bool is_hidden() const {
        return get_visibility() == wasm::WASM_SYMBOL_VISIBILITY_HIDDEN;
    }

    unsigned get_visibility() const {
        return info.flags & wasm::WASM_SYMBOL_VISIBILITY_MASK;
    }
};

} // namespace xld::wasm
