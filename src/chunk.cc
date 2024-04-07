#include "xld_private/chunk.h"
#include "common/leb128.h"
#include "common/log.h"
#include "xld.h"

namespace xld::wasm {

u64 OutputWhdr::compute_section_size(Context &ctx) {
    return sizeof(WasmObjectHeader);
}

void OutputWhdr::copy_buf(Context &ctx) {
    WasmObjectHeader &whdr = *((WasmObjectHeader *)ctx.buf + loc.offset);
    memcpy(whdr.magic, WASM_MAGIC, sizeof(WASM_MAGIC));
    whdr.version = WASM_VERSION;
}

u64 GlobalSection::compute_section_size(Context &ctx) {
    u64 size = 1; // section type
    size += 5;    // content size
    size += 5;    // number of globals
    for (WasmGlobal *global : globals) {
        size += global->span.size();
    }
    return size;
}

void GlobalSection::copy_buf(Context &ctx) {
    // section
    u8 *buf = ctx.buf + loc.offset;
    *(buf++) = WASM_SEC_GLOBAL;
    u32 size = 5;

    for (WasmGlobal *global : globals) {
        size += global->span.size();
    }
    buf += encode_uleb128(size, buf, 5);

    // globals
    buf += encode_uleb128(globals.size(), buf, 5);
    for (WasmGlobal *global : globals) {
        memcpy(buf, global->span.data(), global->span.size());
        buf += global->span.size();
    }
}

} // namespace xld::wasm
