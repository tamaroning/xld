#include "pass.h"
#include "common/log.h"
#include "common/system.h"
#include "oneapi/tbb/parallel_for_each.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/symbol.h"
#include <set>
#include <unordered_map>

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
    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object)
            return;

        ObjectFile *obj = static_cast<ObjectFile *>(file);
        for (auto wsym : obj->symbols) {
            if (wsym.is_type_data())
                continue;
            if (wsym.is_binding_weak())
                continue;
            if (wsym.is_binding_local())
                continue;
            Symbol *sym = get_symbol(ctx, wsym.info.name);
            if (sym->is_defined())
                continue;

            if (wsym.is_type_function()) {
                ctx.import_functions.insert(sym);
            } else if (wsym.is_type_global()) {
                ctx.import_globals.insert(sym);
            } else {
                Error(ctx) << "TODO: import symbol type: " << wsym.info.kind;
            }
        }
    });
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

static void
add_synthetic_global_symbol(Context &ctx, ObjectFile *obj, WasmGlobal *g,
                            u32 flags = WASM_SYMBOL_BINDING_GLOBAL) {
    WasmSymbolInfo info = WasmSymbolInfo{
        .name = g->symbol_name,
        .kind = WASM_SYMBOL_TYPE_GLOBAL,
        .flags = flags,
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

    // __stack_pointer
    {
        // https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/WriterUtils.cpp#L169
        WasmGlobal *g = new WasmGlobal{.type = mutable_global_type_i32,
                                       .init_expr = int32_const(65536 - 16),
                                       .symbol_name = "__stack_pointer"};
        add_synthetic_global_symbol(ctx, obj, g,
                                    WASM_SYMBOL_BINDING_GLOBAL |
                                        WASM_SYMBOL_VISIBILITY_HIDDEN);
        /*
        Symbol *sym = get_symbol(ctx, g->symbol_name);
        sym->is_alive = true;
        sym->visibility = Symbol::Visibility::Hidden;
        */
    }

    // __indirect_function_table
    {
        ctx.indirect_function_table = {
            .elem_type = ValType(WASM_TYPE_FUNCREF),
            .limits = WasmLimits{.flags = WASM_LIMITS_FLAG_HAS_MAX,
                                 .minimum = 1,
                                 .maximum = 1},
        };
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
        // memory
        ctx.output_memory = WasmLimits{.flags = 0, .minimum = 1, .maximum = 0};
    }

    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object)
            return;

        std::set<std::pair<u8, u32>> saw;
        ObjectFile *obj = static_cast<ObjectFile *>(file);
        for (auto &wsym : obj->symbols) {
            if (wsym.is_binding_local()) {
                continue;
            }
            if (wsym.is_undefined())
                continue;

            if (saw.find({wsym.info.kind, wsym.info.value.element_index}) !=
                saw.end())
                continue;

            Symbol *sym = get_symbol(ctx, wsym.info.name);
            if (wsym.is_type_function()) {
                sym->index = ctx.functions.size() + ctx.import_functions.size();
                ctx.functions.push_back(sym);
                if (should_export_symbol(ctx, sym))
                    ctx.export_functions.push_back(sym);
            } else if (wsym.is_type_global()) {
                sym->index = ctx.globals.size() + ctx.import_globals.size();
                ctx.globals.push_back(sym);
                if (should_export_symbol(ctx, sym))
                    ctx.export_globals.push_back(sym);
            }

            saw.insert({wsym.info.kind, wsym.info.value.element_index});
        }
        if (obj->code.has_value())
            ctx.codes.push_back(obj->code.value());
    });
}

void calculate_types(Context &ctx) {
    for (Symbol *sym : ctx.import_functions) {
        ASSERT(sym->wsym.has_value());
        const WasmSignature *sig = sym->wsym.value().signature;
        sym->sig_index = ctx.signatures.size();
        ctx.signatures.push_back(*sig);
    }
    for (Symbol *sym : ctx.functions) {
        ASSERT(sym->wsym.has_value());
        const WasmSignature *sig = sym->wsym.value().signature;
        sym->sig_index = ctx.signatures.size();
        ctx.signatures.push_back(*sig);
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
