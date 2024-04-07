#include "xld_private/chunk.h"
#include "common/leb128.h"
#include "common/log.h"
#include "wasm/object.h"
#include "xld.h"

namespace xld::wasm {

static void write_varuint32(u8 *&buf, u32 value) {
    encode_uleb128(value, buf, 5);
    buf += 5;
}

static u32 get_varuint32_size(u32 value) {
    // return get_uleb128_size(value);
    return 5;
}

static void write_byte(u8 *&buf, u8 byte) { *(buf++) = byte; }

static void write_name(u8 *&buf, std::string_view name) {
    write_varuint32(buf, name.size());
    memcpy(buf, name.data(), name.size());
    buf += name.size();
}

static u32 get_name_size(std::string_view name) {
    // return get_uleb128_size(name.size()) + name.size();
    return 5 + name.size();
}

u64 OutputWhdr::compute_section_size(Context &ctx) {
    return sizeof(WasmObjectHeader);
}

void OutputWhdr::copy_buf(Context &ctx) {
    WasmObjectHeader &whdr = *((WasmObjectHeader *)ctx.buf + loc.offset);
    memcpy(whdr.magic, WASM_MAGIC, sizeof(WASM_MAGIC));
    whdr.version = WASM_VERSION;
}

u64 TypeSection::compute_section_size(Context &ctx) {
    u64 size = 1;                                  // section type
    size += 5;                                     // content size
    size += get_varuint32_size(signatures.size()); // number of types
    for (WasmSignature *sig : signatures) {
        size++;
        size += get_varuint32_size(sig->params.size());
        size += sig->params.size();
        size += get_varuint32_size(sig->returns.size());
        size += sig->returns.size();
    }
    return size;
}

void TypeSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_TYPE);
    write_varuint32(buf, loc.size - 6);

    write_varuint32(buf, signatures.size());
    for (WasmSignature *sig : signatures) {
        write_byte(buf, 0x60);
        write_varuint32(buf, sig->params.size());
        for (auto param : sig->params) {
            write_byte(buf, param);
        }
        write_varuint32(buf, sig->returns.size());
        for (auto ret : sig->returns) {
            write_byte(buf, ret);
        }
    }
}

u64 GlobalSection::compute_section_size(Context &ctx) {
    u64 size = 1;                               // section type
    size += 5;                                  // content size
    size += get_varuint32_size(globals.size()); // number of globals
    for (WasmGlobal *global : globals) {
        size += global->span.size();
    }
    return size;
}

void GlobalSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_GLOBAL);
    // content size
    write_varuint32(buf, loc.size - 6);

    // globals
    write_varuint32(buf, globals.size());
    for (WasmGlobal *global : globals) {
        memcpy(buf, global->span.data(), global->span.size());
        buf += global->span.size();
    }
}

u64 NameSection::compute_section_size(Context &ctx) {
    u64 size = 1;                  // section type
    size += 5;                     // content size
    size += get_name_size("name"); // number of names

    size += 1; // global subsec kind
    // global subsec size
    global_subsec_size = 0;
    global_subsec_size += get_varuint32_size(ctx.global->globals.size());
    for (WasmGlobal *g : ctx.global->globals) {
        global_subsec_size += get_varuint32_size(g->symbol_name.size());
        global_subsec_size += get_name_size(g->symbol_name);
    }
    size += get_varuint32_size(global_subsec_size);
    size += global_subsec_size;

    return size;
}

void NameSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_CUSTOM);
    // content size
    write_varuint32(buf, loc.size - 6);

    write_name(buf, "name");

    // global subsec kind
    write_byte(buf, WASM_NAMES_GLOBAL);
    // global subsec size
    write_varuint32(buf, global_subsec_size);

    // global subsec data
    u32 name_count = ctx.global->globals.size();
    write_varuint32(buf, name_count);
    int index = 0;
    for (WasmGlobal *g : ctx.global->globals) {
        write_varuint32(buf, index);
        write_name(buf, g->symbol_name);
        index++;
    }
}

} // namespace xld::wasm
