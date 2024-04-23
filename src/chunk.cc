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

static void write_varuint64(u8 *&buf, u64 value) {
    buf += encode_uleb128(value, buf);
}

static u32 get_varuint64_size(u64 value) { return get_uleb128_size(value); }

static void write_varint32(u8 *&buf, i32 value) {
    buf += encode_sleb128(value, buf);
}

static u32 get_varint32_size(i32 value) { return get_sleb128_size(value); }

static void write_varint64(u8 *&buf, i64 value) {
    buf += encode_sleb128(value, buf);
}

static u32 get_varint64_size(i64 value) { return get_sleb128_size(value); }

static void write_byte(u8 *&buf, u8 byte) { *(buf++) = byte; }

static void write_u32(u8 *&buf, u32 value) {
    memcpy(buf, &value, sizeof(value));
    buf += sizeof(value);
}

static void write_u64(u8 *&buf, u64 value) {
    memcpy(buf, &value, sizeof(value));
    buf += sizeof(value);
}

static void write_name(u8 *&buf, std::string_view name) {
    write_varuint32(buf, name.size());
    memcpy(buf, name.data(), name.size());
    buf += name.size();
}

static u32 get_name_size(std::string_view name) {
    return get_uleb128_size(name.size()) + name.size();
}

static void write_limits(u8 *&buf, WasmLimits &limits) {
    write_byte(buf, limits.flags);
    write_varuint32(buf, limits.minimum);
    if (limits.flags & WASM_LIMITS_FLAG_HAS_MAX) {
        write_varuint32(buf, limits.maximum);
    }
}

static u32 get_limits_size(WasmLimits &limits) {
    u32 size = 0;
    size++; // flags
    size += get_varuint32_size(limits.minimum);
    if (limits.flags & WASM_LIMITS_FLAG_HAS_MAX) {
        size += get_varuint32_size(limits.maximum);
    }
    return size;
}

static void finalize_section_size_common(u64 &size) {
    size++;
    size += get_varuint32_size(size);
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
        size += get_varuint32_size(sig.params.size());
        size += sig.params.size();
        size += get_varuint32_size(sig.returns.size());
        size += sig.returns.size();
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
        write_varuint32(buf, sig.params.size());
        for (auto param : sig.params) {
            write_byte(buf, param);
        }
        write_varuint32(buf, sig.returns.size());
        for (auto ret : sig.returns) {
            write_byte(buf, ret);
        }
    }
}

u64 ImportSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    u32 num_imports = ctx.import_functions.size();
    size += get_varuint32_size(num_imports); // number of imports
    for (Symbol *sym : ctx.import_functions) {
        // FIXME: Should not be "env"?
        size += get_name_size("env");
        size += get_name_size(sym->name);
        size += 1;                                  // kind
        size += get_varuint32_size(sym->sig_index); // type index
    }
    // TODO: globals
    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void ImportSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_IMPORT);
    write_varuint32(buf, loc.content_size);

    u32 num_imports = ctx.import_functions.size();
    write_varuint32(buf, num_imports);
    for (Symbol *sym : ctx.import_functions) {
        write_name(buf, "env");
        write_name(buf, sym->name);
        write_byte(buf, WASM_EXTERNAL_FUNCTION);
        write_varuint32(buf, sym->sig_index);
    }
    // TODO: globals
}

u64 FunctionSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(ctx.functions.size()); // number of functions
    for (Symbol *sym : ctx.functions) {
        size += get_varuint32_size(sym->sig_index);
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
    for (Symbol *sym : ctx.functions) {
        write_varuint32(buf, sym->sig_index);
    }
    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

static u32 get_init_expr_mvp_size(Context &ctx, WasmInitExprMVP &inst) {
    u32 size = 0;
    size++;
    switch (inst.opcode) {
    case WASM_OPCODE_I32_CONST:
        size += get_varint32_size(inst.value.int32);
        break;
    case WASM_OPCODE_I64_CONST:
        size += get_varint64_size(inst.value.int64);
        break;
    case WASM_OPCODE_F32_CONST:
        size += sizeof(inst.value.float32);
        break;
    case WASM_OPCODE_F64_CONST:
        size += sizeof(inst.value.float64);
        break;
    case WASM_OPCODE_GLOBAL_GET:
        size += get_varuint32_size(inst.value.global);
        break;
    case WASM_OPCODE_REF_NULL:
        size++;
        break;
    default:
        Fatal(ctx) << "Invalid opcode in MVP init_expr: 0x" << std::hex
                   << inst.opcode;
    }
    size++; // END
    return size;
}

static u32 get_init_expr_size(Context &ctx, WasmInitExpr &init_expr) {
    assert(!init_expr.extended);
    return get_init_expr_mvp_size(ctx, init_expr.inst);
}

static void write_init_expr_mvp(Context &ctx, u8 *&buf, WasmInitExprMVP &inst) {
    write_byte(buf, inst.opcode);
    switch (inst.opcode) {
    case WASM_OPCODE_I32_CONST:
        write_varint32(buf, inst.value.int32);
        break;
    case WASM_OPCODE_I64_CONST:
        write_varint64(buf, inst.value.int64);
        break;
    case WASM_OPCODE_F32_CONST:
        write_u32(buf, inst.value.float32);
        break;
    case WASM_OPCODE_F64_CONST:
        write_u64(buf, inst.value.float64);
        break;
    case WASM_OPCODE_GLOBAL_GET:
        write_varuint32(buf, inst.value.global);
        break;
    case WASM_OPCODE_REF_NULL:
        write_byte(buf, ValType::EXTERNREF);
        break;
    default:
        Fatal(ctx) << "Invalid opcode in MVP init_expr: 0x" << std::hex
                   << inst.opcode;
    }
    write_byte(buf, WASM_OPCODE_END);
}

static void write_init_expr(Context &ctx, u8 *&buf, WasmInitExpr &init_expr) {
    assert(!init_expr.extended);
    write_init_expr_mvp(ctx, buf, init_expr.inst);
}

u64 GlobalSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(ctx.globals.size()); // number of globals
    for (Symbol *sym : ctx.globals) {
        WasmGlobal &global = sym->file->globals[sym->elem_index];
        size++; // valtype
        size++; // mut
        if (global.init_expr.body.has_value()) {
            size += global.init_expr.body.value().size();
        } else {
            size += get_init_expr_size(ctx, global.init_expr);
        }
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
    for (Symbol *sym : ctx.globals) {
        WasmGlobal &global = sym->file->globals[sym->elem_index];
        write_byte(buf, global.type.type);
        write_byte(buf, global.type.mut);
        if (global.init_expr.body.has_value()) {
            memcpy(buf, global.init_expr.body.value().data(),
                   global.init_expr.body.value().size());
            buf += global.init_expr.body.value().size();
        } else {
            write_init_expr(ctx, buf, global.init_expr);
        }
    }

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

u64 TableSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(1); // number of tables
    size++;                        // reftype
    size += get_limits_size(ctx.indirect_function_table.limits);

    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void TableSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_TABLE);
    // content size
    write_varuint32(buf, loc.content_size);

    // Only indirect_function_table is supported
    write_byte(buf, 1);                                     // number of tables
    write_byte(buf, ctx.indirect_function_table.elem_type); // reftype
    write_limits(buf, ctx.indirect_function_table.limits);

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

u64 MemorySection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(1); // number of memories
    size += get_limits_size(ctx.output_memory);

    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void MemorySection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_MEMORY);
    // content size
    write_varuint32(buf, loc.content_size);

    // memories
    write_varuint32(buf, 1);
    write_limits(buf, ctx.output_memory);

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

u64 ExportSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    // number of exports
    u32 num_exports =
        1 + ctx.export_functions.size() + ctx.export_globals.size();
    size += get_varuint32_size(num_exports);
    // memory
    size += get_name_size(kDefaultMemoryName);
    size += 1;
    size += get_varuint32_size(0);

    // functions
    for (Symbol *sym : ctx.export_functions) {
        size += get_name_size(sym->name);
        size += 1;
        size += get_varuint32_size(sym->index);
    }
    // globals
    for (Symbol *sym : ctx.export_globals) {
        size += get_name_size(sym->name);
        size += 1;
        size += get_varuint32_size(sym->index);
    }
    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void ExportSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_EXPORT);
    // content size
    write_varuint32(buf, loc.content_size);

    u32 num_exports =
        1 + ctx.export_functions.size() + ctx.export_globals.size();
    write_varint32(buf, num_exports);

    // memory
    write_name(buf, kDefaultMemoryName);
    write_byte(buf, WASM_EXTERNAL_MEMORY);
    write_varuint32(buf, 0);
    // functions
    for (Symbol *sym : ctx.export_functions) {
        write_name(buf, sym->name);
        write_byte(buf, WASM_EXTERNAL_FUNCTION);
        write_varuint32(buf, sym->index);
    }
    // globals
    for (Symbol *sym : ctx.export_globals) {
        write_name(buf, sym->name);
        write_byte(buf, WASM_EXTERNAL_GLOBAL);
        write_varuint32(buf, sym->index);
    }

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

u64 DataCountSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(ctx.segments.size()); // number of data segments

    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void DataCountSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_DATACOUNT);
    // content size
    write_varuint32(buf, loc.content_size);

    // data segments
    write_varuint32(buf, ctx.segments.size());

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

u64 CodeSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_varuint32_size(ctx.functions.size()); // number of code

    for (auto f : ctx.functions) {
        size += get_varuint32_size(f->ifrag->get_size());
        f->ifrag->out_offset = size;
        size += f->ifrag->get_size();
    }
    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
}

void CodeSection::copy_buf(Context &ctx) {
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_CODE);
    // content size
    write_varuint32(buf, loc.content_size);

    u8 *const content_beg = buf;

    // functions
    write_varuint32(buf, ctx.functions.size());
    tbb::parallel_for_each(ctx.functions, [&](Symbol *f) {
        write_varuint32(buf, f->ifrag->get_size());
        f->ifrag->write_to(ctx, content_beg + f->ifrag->out_offset);
    });
}

void CodeSection::apply_reloc(Context &ctx) {
    tbb::parallel_for_each(ctx.functions, [&](Symbol *f) {
        u64 osec_content_file_offset =
            this->loc.offset + (this->loc.size - this->loc.content_size);
        f->ifrag->apply_reloc(ctx, osec_content_file_offset);
    });
}

u64 DataSection::compute_section_size(Context &ctx) {
    /*
    u64 size = 0;
    size += get_varuint32_size(ctx.segments.size()); // number of data segments

    for (WasmDataSegment &seg : ctx.segments) {
        size += get_varuint32_size(seg.init_flags);
        switch (seg.init_flags) {
        case 0: {
            size += get_init_expr_size(ctx, seg.offset);
            size += get_varuint32_size(seg.content.size());
            size += seg.content.size();
        } break;
        case WASM_DATA_SEGMENT_IS_PASSIVE: {
            size += get_varuint32_size(seg.content.size());
            size += seg.content.size();
        } break;
        case WASM_DATA_SEGMENT_HAS_MEMINDEX: {
            size += get_varuint32_size(seg.memory_index);
            size += get_init_expr_size(ctx, seg.offset);
            size += get_varuint32_size(seg.content.size());
            size += seg.content.size();
        } break;
        }
    }
    loc.content_size = size;
    finalize_section_size_common(size);
    return size;
    */
    return 0;
}

void DataSection::copy_buf(Context &ctx) {
    return;
    /*
    u8 *buf = ctx.buf + loc.offset;
    write_byte(buf, WASM_SEC_DATA);
    // content size
    write_varuint32(buf, loc.content_size);

    // data segments
    write_varuint32(buf, ctx.segments.size());
    for (WasmDataSegment &seg : ctx.segments) {
        write_varuint32(buf, seg.init_flags);
        switch (seg.init_flags) {
        case 0: {
            write_init_expr(ctx, buf, seg.offset);
            write_varuint32(buf, seg.content.size());
            memcpy(buf, seg.content.data(), seg.content.size());
            buf += seg.content.size();
        } break;
        case WASM_DATA_SEGMENT_IS_PASSIVE: {
            write_varuint32(buf, seg.content.size());
            memcpy(buf, seg.content.data(), seg.content.size());
            buf += seg.content.size();
        } break;
        case WASM_DATA_SEGMENT_HAS_MEMINDEX: {
            write_varuint32(buf, seg.memory_index);
            write_init_expr(ctx, buf, seg.offset);
            write_varuint32(buf, seg.content.size());
            memcpy(buf, seg.content.data(), seg.content.size());
            buf += seg.content.size();
        } break;
        }
    }
    ASSERT(buf == ctx.buf + loc.offset + loc.size);
    */
}

void DataSection::apply_reloc(Context &ctx) {
    /*
    tbb::parallel_for_each(ctx.data_symbols, [&](Symbol *sym) {
        u64 osec_content_file_offset =
            this->loc.offset + (this->loc.size - this->loc.content_size);
        sym->isec->apply_reloc(ctx, osec_content_file_offset);
    });
    */
}

// TODO: names should not be linking names but debug names
u64 NameSection::compute_section_size(Context &ctx) {
    u64 size = 0;
    size += get_name_size("name"); // number of names

    // function subsec size
    size++; // function subsec kind
    function_subsec_size = 0;
    function_subsec_size += get_varuint32_size(ctx.functions.size());
    for (u32 i = 0; i < ctx.functions.size(); i++) {
        Symbol *sym = ctx.functions[i];
        u32 index = ctx.import_functions.size() + i;
        function_subsec_size += get_varuint32_size(index);
        function_subsec_size += get_name_size(sym->name);
    }
    size += get_varuint32_size(function_subsec_size);
    size += function_subsec_size;

    // global subsec size
    size++; // global subsec kind
    global_subsec_size = 0;
    global_subsec_size += get_varuint32_size(ctx.globals.size());
    for (u32 i = 0; i < ctx.globals.size(); i++) {
        Symbol *sym = ctx.globals[i];
        u32 index = ctx.import_globals.size() + i;
        global_subsec_size += get_varuint32_size(index);
        global_subsec_size += get_name_size(sym->name);
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
    // https: //
    // github.com/WebAssembly/design/blob/main/BinaryEncoding.md#name-section

    // function subsec kind
    write_byte(buf, WASM_NAMES_FUNCTION);
    // function subsec size
    write_varuint32(buf, function_subsec_size);
    // function subsec data
    write_varuint32(buf, ctx.functions.size());
    for (u32 i = 0; i < ctx.functions.size(); i++) {
        Symbol *sym = ctx.functions[i];
        u32 index = ctx.import_functions.size() + i;
        write_varint32(buf, index);
        write_name(buf, sym->name);
    }

    // global subsec kind
    write_byte(buf, WASM_NAMES_GLOBAL);
    // global subsec size
    write_varuint32(buf, global_subsec_size);
    // global subsec data
    write_varuint32(buf, ctx.globals.size());
    for (u32 i = 0; i < ctx.globals.size(); i++) {
        Symbol *sym = ctx.globals[i];
        u32 index = ctx.import_globals.size() + i;
        write_varint32(buf, index);
        write_name(buf, sym->name);
    }

    ASSERT(buf == ctx.buf + loc.offset + loc.size);
}

} // namespace xld::wasm
