#include "common/leb128.h"
#include "xld.h"
#include "common/log.h"

namespace xld::wasm {

void OutputWhdr::copy_buf(Context &ctx) {
    // https://github.com/tamaroning/mold/blob/3df7c8e89c507865abe0fad4ff5355f4d328f78d/elf/output-chunks.cc#L47
    WasmObjectHeader &whdr = *((WasmObjectHeader *)ctx.buf);
    memcpy(whdr.magic, WASM_MAGIC, sizeof(WASM_MAGIC));
    whdr.version = WASM_VERSION;
}

void GlobalSection::copy_buf(Context &ctx) {
    // section
    u8 *buf = ctx.buf + sizeof(WasmObjectHeader);
    *(buf++) = WASM_SEC_GLOBAL;
    u32 size = 5;
    
    for (WasmGlobal *global : globals) {
        size += global->span.size();
    }
    buf += encode_uleb128(size, buf, 5);
    Debug(ctx) << "GlobalSection::copy_buf: size=" << size;

    // globals
    buf += encode_uleb128(globals.size(), buf, 5);
    for (WasmGlobal *global : globals) {
        memcpy(buf, global->span.data(), global->span.size());
        buf += global->span.size();
    }
}

} // namespace xld::wasm
