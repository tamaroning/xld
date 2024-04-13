#include "pass.h"
#include "common/log.h"
#include "common/system.h"
#include "wasm/object.h"
#include "xld.h"

namespace xld::wasm {

void resolve_symbols(Context &ctx) {
    // Add symbols to the global symbol table
    for (InputFile *file : ctx.files) {
        file->resolve_symbols(ctx);
    }
}

void calculate_imports(Context &ctx) {
    // resolve all undefined symbols
    // cannot parallelize because of pushing to output imports
    for (InputFile *file : ctx.files) {
        if (file->kind != InputFile::Object)
            return;

        ObjectFile *obj = static_cast<ObjectFile *>(file);
        for (auto import : obj->imports) {
            Symbol *sym = get_symbol(ctx, import.field);
            if (sym->is_defined()) {
                continue;
            }

            if (allow_undefined_symbol(ctx, sym)) {
                // if undefined symbols are allowed, import all of them
                switch (import.kind) {
                case WASM_EXTERNAL_FUNCTION: {
                    u32 new_sig_index = ctx.signatures.size();
                    ctx.signatures.push_back(OutputElem<WasmSignature>(
                        obj->signatures[import.sig_index], obj));
                    import.sig_index = new_sig_index;
                    ctx.import_functions.push_back(import);
                } break;
                default:
                    Warn(ctx) << "TODO: import kind: " << import.kind;
                    break;
                }
            } else {
                Error(ctx) << "Undefined symbol: " << import.field;
            }
        }
    }
}

static WasmInitExpr int32_const(i32 value) {
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

    {
        // https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/WriterUtils.cpp#L169
        WasmGlobal *g = new WasmGlobal{.type = mutable_global_type_i32,
                                       .init_expr = int32_const(65536 - 16),
                                       .symbol_name = "__stack_pointer"};
        add_synthetic_global_symbol(ctx, obj, g);
        get_symbol(ctx, g->symbol_name)->is_alive = true;
        get_symbol(ctx, g->symbol_name)->visibility =
            Symbol::Visibility::Hidden;
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
    push(ctx.import = new ImportSection());
    push(ctx.function = new FunctionSection());
    push(ctx.table = new TableSection());
    push(ctx.memory = new MemorySection());
    push(ctx.global = new GlobalSection());
    push(ctx.export_ = new ExportSection());
    push(ctx.code = new CodeSection());
    push(ctx.name = new NameSection());

    {
        ctx.output_memory = WasmLimits{.flags = 0, .minimum = 1, .maximum = 0};
        ctx.exports.emplace_back(WasmExport{
            .name = std::string(kDefaultMemoryName),
            .kind = WASM_EXTERNAL_MEMORY,
            .index = 0,
        });
    }

    for (InputFile *file : ctx.files) {
        if (file->kind != InputFile::Object) {
            return;
        }

        // globals
        ObjectFile *obj = static_cast<ObjectFile *>(file);
        for (WasmGlobal g : obj->globals) {
            Symbol *sym = get_symbol(ctx, g.symbol_name);
            sym->index = ctx.import_globals.size() + ctx.globals.size();
            if (should_export_symbol(ctx, sym)) {
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
            u32 index = ctx.import_functions.size() + ctx.functions.size();
            sym->index = index;
            if (should_export_symbol(ctx, sym)) {
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
