#pragma once
#include "object.h"

namespace xld::wasm {

inline WasmInitExpr int32_const(i32 value) {
    return WasmInitExpr{
        .extended = false,
        .inst =
            {
                .opcode = WASM_OPCODE_I32_CONST,
                .value = {.int32 = value},
            },
        .body = std::nullopt,
    };
}

} // namespace xld::wasm
