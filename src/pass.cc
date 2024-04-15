#include "pass.h"
#include "common/log.h"
#include "common/system.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/symbol.h"
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
    for (InputFile *file : ctx.files) {
        if (file->kind != InputFile::Object)
            return;

        ObjectFile *obj = static_cast<ObjectFile *>(file);
        for (auto import : obj->imports) {
            Symbol *sym = get_symbol(ctx, import.field);
            if (sym->is_defined()) {
                continue;
            }

            if (import.kind == WASM_EXTERNAL_MEMORY) {
                continue;
            }

            if (import.kind == WASM_EXTERNAL_TABLE) {
                if (import.field == "__indirect_function_table") {
                    continue;
                } else {
                    Error(ctx) << "Imported table must have name "
                                  "__indirect_function_table, but found "
                               << import.field;
                }
            }

            u32 func_index = 0;
            if (allow_undefined_symbol(ctx, sym)) {
                // if undefined symbols are allowed, import all of them
                switch (import.kind) {
                case WASM_EXTERNAL_FUNCTION: {
                    u32 new_sig_index = ctx.signatures.size();
                    ctx.signatures.push_back(obj->signatures[import.sig_index]);
                    import.sig_index = new_sig_index;
                    ctx.import_functions.push_back(import);
                    get_symbol(ctx, import.field)->index = func_index;
                    func_index++;
                } break;
                case WASM_EXTERNAL_MEMORY:
                    continue;
                case WASM_EXTERNAL_TABLE:
                    continue;
                default:
                    Warn(ctx) << "TODO: import kind: " << (u32)import.kind;
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

    // __stack_pointer
    {
        // https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/WriterUtils.cpp#L169
        WasmGlobal *g = new WasmGlobal{.type = mutable_global_type_i32,
                                       .init_expr = int32_const(65536 - 16),
                                       .symbol_name = "__stack_pointer"};
        add_synthetic_global_symbol(ctx, obj, g);
        Symbol *sym = get_symbol(ctx, g->symbol_name);
        sym->is_alive = true;
        sym->visibility = Symbol::Visibility::Hidden;
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
        ctx.output_memory = WasmLimits{.flags = 0, .minimum = 1, .maximum = 0};
        ctx.exports.insert({std::string(kDefaultMemoryName),
                            WasmExport{
                                .name = std::string(kDefaultMemoryName),
                                .kind = WASM_EXTERNAL_MEMORY,
                                .index = 0,
                            }});
    }

    for (InputFile *file : ctx.files) {
        if (file->kind != InputFile::Object) {
            return;
        }
        ObjectFile *obj = static_cast<ObjectFile *>(file);

        u32 sig_insert_start = ctx.signatures.size();
        u32 global_insert_start = ctx.globals.size();
        u32 function_insert_start = ctx.functions.size();

        // signatures
        {
            u32 sig_index_start = sig_insert_start;
            for (WasmSignature sig : obj->signatures) {
                ctx.signatures.emplace_back(sig);
            }
        }

        // globals
        {
            u32 global_index_start =
                global_insert_start + ctx.import_globals.size();
            for (WasmGlobal g : obj->globals) {
                Symbol *sym = get_symbol(ctx, g.symbol_name);
                sym->index = global_index_start + ctx.globals.size() +
                             ctx.import_globals.size();
                ctx.globals.emplace_back(g);
            }
        }
        // functions
        {
            u32 sig_index_start = function_insert_start;
            u32 func_index_start =
                ctx.functions.size() + ctx.import_functions.size();
            for (WasmFunction f : obj->functions) {
                u32 original_sig_index = f.sig_index;
                u32 sig_index = sig_index_start + f.sig_index;
                f.sig_index = sig_index;
                Symbol *sym = get_symbol(ctx, f.symbol_name);
                u32 index = func_index_start + ctx.functions.size() +
                            ctx.import_functions.size();
                sym->index = index;
                ctx.functions.emplace_back(f);
            }
        }

        for (WasmSymbol &wsym : obj->symbols) {
            if (wsym.is_binding_local())
                continue;

            u32 index;
            std::string export_name = wsym.info.name;
            {
                if (wsym.is_defined()) {
                    if (wsym.is_type_global()) {
                        WasmGlobal &g = obj->get_defined_global(
                            wsym.info.value.element_index);
                        index = global_insert_start +
                                ctx.import_globals.size() +
                                (wsym.info.value.element_index -
                                 obj->num_imported_globals);
                    } else if (wsym.is_type_function()) {
                        WasmFunction &f = obj->get_defined_function(
                            wsym.info.value.element_index);
                        index = function_insert_start +
                                ctx.import_functions.size() +
                                (wsym.info.value.element_index -
                                 obj->num_imported_functions);
                        if (f.export_name.has_value())
                            export_name = f.export_name.value();
                    }
                } else {
                    index = get_symbol(ctx, wsym.info.name)->index;
                }
            }

            // FIXME: correct?
            if (wsym.is_undefined())
                continue;

            Symbol *sym = get_symbol(ctx, wsym.info.name);
            sym->index = index;

            if (wsym.is_type_global()) {
                Symbol *sym = get_symbol(ctx, wsym.info.name);
                if (!wsym.is_binding_local() &&
                    should_export_symbol(ctx, sym)) {
                    ctx.exports.insert(
                        {export_name, WasmExport{
                                          .name = export_name,
                                          .kind = WASM_EXTERNAL_GLOBAL,
                                          .index = index,
                                      }});
                }
            } else if (wsym.is_type_function()) {
                Symbol *sym = get_symbol(ctx, wsym.info.name);
                if (!wsym.is_binding_local() &&
                    should_export_symbol(ctx, sym)) {
                    ctx.exports.insert(
                        {export_name, WasmExport{
                                          .name = export_name,
                                          .kind = WASM_EXTERNAL_FUNCTION,
                                          .index = index,
                                      }});
                }
            }
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
