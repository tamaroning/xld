#include "pass.h"
#include "common/log.h"
#include "common/system.h"
#include "oneapi/tbb/parallel_for.h"
#include "oneapi/tbb/parallel_for_each.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/symbol.h"
#include <atomic>
#include <map>
#include <mutex>
#include <set>

namespace xld::wasm {

void resolve_symbols(Context &ctx) {
    // Add symbols to the global symbol table
    tbb::parallel_for_each(
        ctx.files, [&](InputFile *file) { file->resolve_symbols(ctx); });
}

void check_undefined(Context &ctx) {
    Debug(ctx) << "Checking undefined symbols";
    tbb::parallel_for_each(ctx.symbol_map, [&](auto &pair) {
        Symbol &sym = pair.second;
        if (sym.is_defined())
            return;

        if (allow_undefined_symbol(ctx, &sym))
            return;

        Error(ctx) << "Undefined symbol: " << sym.name;
    });
}

void calculate_imports(Context &ctx) {
    // resolve all undefined symbols
    // cannot parallelize because of pushing to output imports
    std::mutex m;
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
    // Create an internal object file to hold linker-synthesized symbols
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
        // We set the actual value in setup_memory
        WasmGlobal g = WasmGlobal{.type = mutable_global_type_i32,
                                  .init_expr = int32_const(0),
                                  .symbol_name = "__stack_pointer"};
        add_synthetic_global_symbol(ctx, obj, &g,
                                    WASM_SYMBOL_BINDING_GLOBAL |
                                        WASM_SYMBOL_VISIBILITY_HIDDEN);
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

    ctx.files.insert(ctx.files.begin(), obj);
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
    push(ctx.data_count = new DataCountSection());
    push(ctx.code = new CodeSection());
    push(ctx.data_sec = new DataSection());
    push(ctx.name = new NameSection());

    tbb::concurrent_set<Symbol *> saw;
    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object)
            return;

        ObjectFile *obj = static_cast<ObjectFile *>(file);

        // Encode symbol definiton as (symbol type, index) because a symbol
        // table in a object file may contain multiple symbols refering to the
        // same item.
        std::set<std::pair<WasmSymbolType, u32>> visited;
        for (auto &wsym : obj->symbols) {
            if (wsym.is_binding_local())
                continue;
            if (wsym.is_undefined())
                continue;
            if (!visited.insert({wsym.info.kind, wsym.info.value.element_index})
                     .second)
                continue;

            Symbol *sym = get_symbol(ctx, wsym.info.name);

            if (wsym.is_type_function()) {
                ctx.functions.push_back(sym);
                if (should_export_symbol(ctx, sym))
                    ctx.export_functions.push_back(sym);
            } else if (wsym.is_type_global()) {
                ctx.globals.push_back(sym);
                if (should_export_symbol(ctx, sym))
                    ctx.export_globals.push_back(sym);
            } else if (wsym.is_type_data()) {
                ctx.data_symbols.push_back(sym);
                if (should_export_symbol(ctx, sym))
                    ctx.export_datas.push_back(sym);
            }
        }
    });
}

void assign_index(Context &ctx) {
    {
        u32 idx = 0;
        for (auto sym : ctx.import_functions) {
            sym->index = idx++;
        }
    }
    {
        u32 idx = 0;
        for (auto sym : ctx.import_globals) {
            sym->index = idx++;
        }
    }

    tbb::parallel_for(
        static_cast<std::size_t>(0), ctx.functions.size(), [&](std::size_t i) {
            ctx.functions[i]->index = i + ctx.import_functions.size();
        });
    tbb::parallel_for(static_cast<std::size_t>(0), ctx.globals.size(),
                      [&](std::size_t i) {
                          ctx.globals[i]->index = i + ctx.import_globals.size();
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

// align offset to `align`
// e.g. align(0, 16) = 0, align(1, 16) = 16, align(33, 16) = 48
static i32 align(i32 offset, i32 align) {
    return ((offset + align - 1) / align) * align;
}

void setup_memory(Context &ctx) {
    Debug(ctx) << "Setting up memory layout";
    i32 offset = 0;
    const u32 memory_size = kPageSize * kMinMemoryPages;
    {
        // memory
        ctx.output_memory = WasmLimits{.flags = WASM_LIMITS_FLAG_HAS_MAX,
                                       .minimum = kMinMemoryPages,
                                       .maximum = kMaxMemoryPages};
    }

    // At least, we need to set __memory_base, __table_base, and __stack_pointer
    // https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md#interface-and-usage
    // TODO: __memory_base
    // TODO: __table_base

    // Set virtual addresses to all symbols and push data segments if needed
    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object)
            return;
        ObjectFile *obj = static_cast<ObjectFile *>(file);

        // segment index -> offset
        std::map<u32, i32> visited_segs;
        for (auto &wsym : obj->symbols) {
            if (wsym.is_binding_local())
                continue;
            if (wsym.is_undefined())
                continue;
            if (!wsym.is_type_data())
                continue;

            Symbol *sym = get_symbol(ctx, wsym.info.name);

            u32 seg_index = wsym.info.value.data_ref.segment;
            auto it = visited_segs.find(seg_index);
            i32 seg_offset;
            if (it != visited_segs.end()) {
                seg_offset = it->second;
                continue;
            } else {
                WasmDataSegment seg = obj->data_segments[seg_index];
                offset = align(offset, seg.p2align);
                Debug(ctx) << "Data segment: " << seg_index << " offset: 0x"
                           << offset << " size: 0x" << seg.content.size();
                seg_offset = offset;
                seg.offset = int32_const(offset);
                offset += seg.content.size();
                ctx.segments.emplace_back(seg);
            }

            sym->virtual_address = seg_offset + wsym.info.value.data_ref.offset;
        }
    });

    // TODO: __data_end

    offset = align(offset, kStackAlign);
    // TODO: __stack_low
    offset += kStackSize;
    // TODO: __stack_high
    // TODO: __heap_base
    {
        Symbol *sp = get_symbol(ctx, "__stack_pointer");
        sp->file->globals[sp->index].init_expr = int32_const(offset);
    }

    // TODO: heap

    if (offset > memory_size)
        Error(ctx) << "Corrupted memory layout";
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
