#pragma once
#include "wao.h"

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

    bool isTypeFunction() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_FUNCTION;
    }

    bool isTypeTable() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_TABLE;
    }

    bool isTypeData() const { return info.kind == wasm::WASM_SYMBOL_TYPE_DATA; }

    bool isTypeGlobal() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_GLOBAL;
    }

    bool isTypeSection() const {
        return info.kind == wasm::WASM_SYMBOL_TYPE_SECTION;
    }

    bool isTypeTag() const { return info.kind == wasm::WASM_SYMBOL_TYPE_TAG; }

    bool isDefined() const { return !isUndefined(); }

    bool isUndefined() const {
        return (info.flags & wasm::WASM_SYMBOL_UNDEFINED) != 0;
    }

    bool isBindingWeak() const {
        return getBinding() == wasm::WASM_SYMBOL_BINDING_WEAK;
    }

    bool isBindingGlobal() const {
        return getBinding() == wasm::WASM_SYMBOL_BINDING_GLOBAL;
    }

    bool isBindingLocal() const {
        return getBinding() == wasm::WASM_SYMBOL_BINDING_LOCAL;
    }

    unsigned getBinding() const {
        return info.flags & wasm::WASM_SYMBOL_BINDING_MASK;
    }

    bool isHidden() const {
        return getVisibility() == wasm::WASM_SYMBOL_VISIBILITY_HIDDEN;
    }

    unsigned getVisibility() const {
        return info.flags & wasm::WASM_SYMBOL_VISIBILITY_MASK;
    }
};

} // namespace xld::wasm
