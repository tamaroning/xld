#include "pass.h"
#include "common/log.h"
#include "common/system.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/input_file.h"

namespace xld::wasm {

void resolve_symbols(Context &ctx) {
    // Add symbols to the global symbol table
    for (InputFile *file : ctx.files) {
        file->resolve_symbols(ctx);
    }

    // resolve all undefined symbols
    for (InputFile *file : ctx.files) {
        for (WasmSymbol &wsym : file->symbols) {
            Symbol *sym = get_symbol(ctx, wsym.info.name);
            if (wsym.is_undefined()) {
                if (sym->is_undefined()) {
                    Error(ctx) << "Undefined symbol: " << wsym.info.name;
                }
            }
        }
    }
}

static WasmInitExpr int_const(u64 value) {
    WasmInitExpr ie;
    ie.extended = false;
    ie.inst.opcode = WASM_OPCODE_I32_CONST;
    ie.inst.value.int32 = static_cast<int32_t>(value);
    return ie;
}

static void add_synthetic_global_symbol(Context &ctx, ObjectFile *obj,
                                        WasmGlobal *g) {
    WasmSymbolInfo info = WasmSymbolInfo{
        .name = g->symbol_name,
        .kind = WASM_SYMBOL_TYPE_GLOBAL,
        .flags = WASM_SYMBOL_BINDING_GLOBAL,
    };
    WasmSymbol wsym(info, &g->type, nullptr, nullptr);
    obj->symbols.push_back(wsym);
    obj->globals.push_back(*g);
}

void create_internal_file(Context &ctx) {
    ObjectFile *obj = ObjectFile::create(ctx, "<internal>");

    // https://github.com/kubkon/zld/blob/e4d9b667b21e51cb3882c8d113c0adb739e1c86f/src/Wasm.zig#L951

    // TODO: add synthetic symbols
    static WasmGlobalType global_type_i32 = {ValType(WASM_TYPE_I32), false};
    static WasmGlobalType global_type_i64 = {ValType(WASM_TYPE_I64), false};
    static WasmGlobalType mutable_global_type_i32 = {ValType(WASM_TYPE_I32),
                                                     true};
    static WasmGlobalType mutable_global_typeI64 = {ValType(WASM_TYPE_I64),
                                                    true};

    // TODO: PIC
    // TODO: wasm64
    {
        // TODO: writeInitExpr equivalent
        // https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/WriterUtils.cpp#L169
        std::string *sp = new std::string("\x7f\x01\x41\x80\x88\x04\x0b");
        WasmGlobal *g =
            new WasmGlobal{.type =
                               WasmGlobalType{
                                   .type = ValType(WASM_TYPE_I32),
                                   .mut = false,
                               },
                           .init_expr = int_const(0),
                           .symbol_name = "__stack_pointer",
                           .span = std::span<u8>((u8 *)sp->data(), sp->size())};
        add_synthetic_global_symbol(ctx, obj, g);
        get_symbol(ctx, g->symbol_name)->is_alive = true;
    }

    ctx.files.push_back(obj);
}

void create_synthetic_sections(Context &ctx) {
    auto push = [&](Chunk *s) {
        ctx.chunks.push_back(s);
        ctx.chunk_pool.emplace_back(s);
    };

    push(ctx.whdr = new OutputWhdr());
    push(ctx.type = new TypeSection());
    push(ctx.function = new FunctionSection());
    push(ctx.table = new TableSection());
    push(ctx.memory = new MemorySection());
    push(ctx.global = new GlobalSection());
    push(ctx.export_ = new ExportSection());
    push(ctx.code = new CodeSection());
    push(ctx.name = new NameSection());

    for (InputFile *file : ctx.files) {
        if (file->kind != InputFile::Object) {
            return;
        }
        // globals
        ObjectFile *obj = static_cast<ObjectFile *>(file);
        for (WasmGlobal g : obj->globals) {
            Symbol *sym = get_symbol(ctx, g.symbol_name);
            sym->index = ctx.globals.size();
            if (sym->is_exported) {
                ctx.exports.emplace_back(WasmExport{
                    .name = g.symbol_name,
                    .kind = WASM_EXTERNAL_GLOBAL,
                    .index = static_cast<u32>(ctx.globals.size()),
                });
            }
            ctx.globals.emplace_back(OutputElem<WasmGlobal>(g, obj));
        }
        // functions
        for (WasmFunction f : obj->functions) {
            u32 original_sig_index = f.sig_index;
            u32 sig_index = ctx.signatures.size();
            f.sig_index = sig_index;
            ctx.signatures.emplace_back(OutputElem<WasmSignature>(
                obj->signatures[original_sig_index], obj));
            Symbol *sym = get_symbol(ctx, f.symbol_name);
            u32 index = ctx.functions.size();
            sym->index = index;
            if (sym->is_exported) {
                ctx.exports.emplace_back(WasmExport{
                    .name = f.export_name.value_or(f.symbol_name),
                    .kind = WASM_EXTERNAL_FUNCTION,
                    .index = index,
                });
            }
            ctx.functions.emplace_back(OutputElem<WasmFunction>(f, obj));
        }
        // push the code section to keep the same order as the function
        // section
        if (obj->code.has_value())
            ctx.codes.emplace_back(obj->code.value());
    }
}

u64 compute_section_sizes(Context &ctx) {
    tbb::parallel_for_each(ctx.chunks, [&](Chunk *chunk) {
        chunk->loc.size = chunk->compute_section_size(ctx);
    });

    u64 offset = 0;
    for (Chunk *chunk : ctx.chunks) {
        chunk->loc.offset = offset;
        offset += chunk->loc.size;
        Debug(ctx) << std::hex << "Section: " << chunk->name << " offset: 0x"
                   << chunk->loc.offset << " size: 0x" << chunk->loc.size;
    }
    Debug(ctx) << std::hex << "Total size: 0x" << offset;
    return offset;
}

void copy_chunks(Context &ctx) {
    tbb::parallel_for_each(ctx.chunks, [&](Chunk *chunk) {
        Debug(ctx) << "Copying chunk: " << chunk->name;
        chunk->copy_buf(ctx);
    });
}

void apply_reloc(Context &ctx) {
    tbb::parallel_for_each(ctx.chunks,
                           [&](Chunk *chunk) { chunk->apply_reloc(ctx); });
}

} // namespace xld::wasm
