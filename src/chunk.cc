#include "object/wao_basic.h"
#include "xld.h"

namespace xld::wasm {

void OutputWhdr::copy_buf(Context &ctx) {
    // https://github.com/tamaroning/mold/blob/3df7c8e89c507865abe0fad4ff5355f4d328f78d/elf/output-chunks.cc#L47
    WasmObjectHeader &whdr = *((WasmObjectHeader *)ctx.buf);
    memcpy(whdr.magic, WASM_MAGIC, sizeof(WASM_MAGIC));
    whdr.version = WASM_VERSION;
}

} // namespace xld::wasm
