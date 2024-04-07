#include "xld_private/chunk.h"
#include "common/leb128.h"
#include "common/log.h"
#include "wasm/object.h"
#include "xld.h"

namespace xld::wasm {

static void write_varuint32(u8 *&buf, u32 value) {
    buf += encode_uleb128(value, buf);
}

static u32 get_varuint32_size(u32 value) { return get_uleb128_size(value); }

static void write_byte(u8 *&buf, u8 byte) { *(buf++) = byte; }

static void write_name(u8 *&buf, std::string_view name) {
    write_varuint32(buf, name.size());
    memcpy(buf, name.data(), name.size());
    buf += name.size();
}

static void finalize_section_size_common(u64 &size) {
    size++;
    size += get_varuint32_size(size);
}

static u32 get_name_size(std::string_view name) {
    return get_uleb128_size(name.size()) + name.size();
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
    u64 size = 0;
    size += get_varuint32_size(ctx.signatures.size()); // number of types
    for (auto &sig : ctx.signatures) {
        size++;
        size += get_varuint32_size(sig.wdata.params.size());
        size += sig.wdata.params.size();
        size += get_varuint32_size(sig.wdata.returns.size());
        size += sig.wdata.returns.size();
    }
    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void TypeSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_TYPE);
    write_varuint32(buf, loc.content_size);

    write_varuint32(buf, ctx.signatures.size());
    for (auto &sig : ctx.signatures) {
        write_byte(buf, WASM_TYPE_FUNC);
        write_varuint32(buf, sig.wdata.params.size());
        for (auto param : sig.wdata.params) {
            write_byte(buf, param);
        }
        write_varuint32(buf, sig.wdata.returns.size());
        for (auto ret : sig.wdata.returns) {
            write_byte(buf, ret);
        }
    }
}

u64 FunctionSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(ctx.functions.size()); // number of functions
    for (auto &func : ctx.functions) {
        size += get_varuint32_size(func.wdata.sig_index);
    }
    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void FunctionSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_FUNCTION);
    write_varuint32(buf, loc.content_size);

    write_varuint32(buf, ctx.functions.size());
    for (auto &func : ctx.functions) {
        write_varuint32(buf, func.wdata.sig_index);
    }
    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

u64 GlobalSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(ctx.globals.size()); // number of globals
    for (auto &global : ctx.globals) {
        size += global.wdata.span.size();
    }
    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void GlobalSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_GLOBAL);
    // content size
    write_varuint32(buf, loc.content_size);

    // globals
    write_varuint32(buf, ctx.globals.size());
    for (auto &global : ctx.globals) {
        memcpy(buf, global.wdata.span.data(), global.wdata.span.size());
        buf += global.wdata.span.size();
    }

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

u64 NameSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_name_size("name"); // number of names

    // function subsec size
    size++; // function subsec kind
    function_subsec_size = 0;
    function_subsec_size += get_varuint32_size(ctx.functions.size());
    for (auto &f : ctx.functions) {
        function_subsec_size += get_varuint32_size(f.wdata.symbol_name.size());
        function_subsec_size += get_name_size(f.wdata.symbol_name);
    }
    size += get_varuint32_size(function_subsec_size);
    size += function_subsec_size;

    // global subsec size
    size++; // global subsec kind
    global_subsec_size = 0;
    global_subsec_size += get_varuint32_size(ctx.globals.size());
    for (auto &g : ctx.globals) {
        global_subsec_size += get_varuint32_size(g.wdata.symbol_name.size());
        global_subsec_size += get_name_size(g.wdata.symbol_name);
    }
    size += get_varuint32_size(global_subsec_size);
    size += global_subsec_size;

    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void NameSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_CUSTOM);
    // content size
    write_varuint32(buf, loc.content_size);

    write_name(buf, "name");

    // NOTE: We must write subsections in the order of function, global, etc.
    // https://github.com/WebAssembly/design/blob/main/BinaryEncoding.md#name-section

    // function subsec kind
    write_byte(buf, WASM_NAMES_FUNCTION);
    // function subsec size
    write_varuint32(buf, function_subsec_size);
    // function subsec data
    write_varuint32(buf, ctx.functions.size());
    int index = 0;
    for (auto &f : ctx.functions) {
        write_varuint32(buf, index);
        write_name(buf, f.wdata.symbol_name);
        index++;
    }

    // global subsec kind
    write_byte(buf, WASM_NAMES_GLOBAL);
    // global subsec size
    write_varuint32(buf, global_subsec_size);
    // global subsec data
    write_varuint32(buf, ctx.globals.size());
    index = 0;
    for (auto &g : ctx.globals) {
        write_varuint32(buf, index);
        write_name(buf, g.wdata.symbol_name);
        index++;
    }

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

} // namespace xld::wasm
