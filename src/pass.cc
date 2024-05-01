#include "pass.h"
#include "common/integers.h"
#include "common/leb128.h"
#include "common/log.h"
#include "common/system.h"
#include "oneapi/tbb/parallel_for.h"
#include "oneapi/tbb/parallel_for_each.h"
#include "wasm/object.h"
#include "wasm/utils.h"
#include "xld.h"
#include "xld_private/chunk.h"
#include "xld_private/input_file.h"
#include "xld_private/output_elem.h"
#include "xld_private/symbol.h"
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>

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
    /*
    static WasmGlobalType global_type_i32 = {ValType(WASM_TYPE_I32), false};
    static WasmGlobalType global_type_i64 = {ValType(WASM_TYPE_I64), false};
    */
    static WasmGlobalType mutable_global_type_i32 = {ValType(WASM_TYPE_I32),
                                                     true};
    /*
    static WasmGlobalType mutable_global_type_i64 = {ValType(WASM_TYPE_I64),
                                                    true};
    */

    // __stack_pointer
    {
        // We set the actual value in `setup_memory`
        WasmGlobal g = WasmGlobal{.type = mutable_global_type_i32,
                                  .init_expr = int32_const(0),
                                  .symbol_name = "__stack_pointer"};
        add_synthetic_global_symbol(ctx, obj, &g,
                                    WASM_SYMBOL_BINDING_GLOBAL |
                                        WASM_SYMBOL_VISIBILITY_HIDDEN);
    }

    // __wasm_call_ctors
    {
        // add type () -> nil
        std::vector<ValType> v = {};
        WasmSignature void_sig = WasmSignature{
            v,
            v,
        };
        u32 sig_index = obj->signatures.size();
        obj->signatures.push_back(void_sig);
        u32 index = obj->functions.size();
        WasmFunction f = {
            .index = index,
            .sig_index = sig_index,
            .symbol_name = "__wasm_call_ctors",
        };
        WasmSymbolInfo info =
            WasmSymbolInfo{.name = f.symbol_name,
                           .kind = WASM_SYMBOL_TYPE_FUNCTION,
                           .flags = WASM_SYMBOL_BINDING_GLOBAL,
                           .value = {
                               .element_index = index,
                           }};
        WasmSymbol wsym(info, nullptr, nullptr, &obj->signatures[sig_index]);
        obj->functions.push_back(f);
        {
            // Dummy content.
            static std::vector<u8> dummy = {0x00, WASM_OPCODE_END};
            ctx.__wasm_call_ctors = new InputFragment(0, obj, dummy, 0);
            ctx.ifrag_pool.emplace_back(ctx.__wasm_call_ctors);
            obj->code_ifrags.emplace_back(ctx.__wasm_call_ctors);
        }
        obj->symbols.push_back(wsym);
    }

    // __indirect_function_table is set in `setup_indirect_functions`

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
    push(ctx.elem = new ElemSection());
    push(ctx.data_count = new DataCountSection());
    push(ctx.code = new CodeSection());
    push(ctx.data_sec = new DataSection());
    push(ctx.name = new NameSection());
}

void add_definitions(Context &ctx) {
    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object)
            return;

        ObjectFile *obj = static_cast<ObjectFile *>(file);

        // Encode symbol definiton as (symbol type, index) because a symbol
        // table in a object file may contain multiple symbols refering to the
        // same item.
        std::set<std::pair<WasmSymbolType, u32>> visited;
        for (auto &wsym : obj->symbols) {
            Debug(ctx) << "Adding definition: " << wsym.info.name;
            // if (wsym.is_binding_local())
            //     continue;
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

void setup_ctors(Context &ctx) {
    Debug(ctx) << "Setting up ctors";
    // Priority -> ctors
    std::map<u32, std::vector<Symbol *>, std::greater<u32>> map;
    for (Symbol *f : ctx.functions) {
        if (f->is_undefined())
            continue;
        if (!f->wsym.value().info.init_func_priority.has_value())
            continue;
        Debug(ctx) << "ctor: " << f->name;
        u32 priority = f->wsym.value().info.init_func_priority.value();
        map[priority].push_back(f);
    }

    static std::stringstream s;
    s << (u8)0x00; // no locals
    for (auto &[priority, ctors] : map) {
        for (Symbol *f : ctors) {
            s << (u8)WASM_OPCODE_CALL;
            encode_uleb128(f->index, s);
        }
    }
    s << (u8)WASM_OPCODE_END;
    ctx.__wasm_call_ctors->span =
        std::span<const u8>{(const u8 *)s.view().data(), s.view().size()};
}

void setup_indirect_functions(Context &ctx) {
    // __indirect_function_table
    ctx.__indirect_function_table = OutputElem{ValType(WASM_TYPE_FUNCREF)};
    ctx.__indirect_function_table.flags = 0;
    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object)
            return;
        ObjectFile *obj = static_cast<ObjectFile *>(file);

        for (InputSection *isec : obj->sections) {
            for (auto &reloc : isec->relocs) {
                switch (reloc.type) {
                case R_WASM_TABLE_INDEX_I32:
                case R_WASM_TABLE_INDEX_I64:
                case R_WASM_TABLE_INDEX_SLEB:
                case R_WASM_TABLE_INDEX_SLEB64: {
                    std::string &name = obj->symbols[reloc.index].info.name;
                    Symbol *sym = get_symbol(ctx, name);
                    if (sym->is_defined()) {
                        ctx.__indirect_function_table.elements.emplace_back(
                            sym);
                    }
                } break;
                default:
                    break;
                }
            }
        }
    });

    ASSERT(ctx.tables.empty());
    ctx.tables.push_back(WasmTableType{
        .elem_type = ValType(WASM_TYPE_FUNCREF),
        .limits = WasmLimits{
            .flags = 0,
            // TODO: reserve the first index maybe??
            .minimum = ctx.__indirect_function_table.elements.size() + 1,
            .maximum = 0}});
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

    // At least, we need to set __memory_base, __table_base, and
    // __stack_pointer
    // https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md#interface-and-usage
    // TODO: __memory_base
    // TODO: __table_base

    // merge data fragments
    for (InputFile *file : ctx.files) {
        if (file->kind != InputFile::Object)
            return;
        ObjectFile *obj = static_cast<ObjectFile *>(file);

        // merge data segments
        for (auto &wsym : obj->symbols) {
            if (wsym.is_binding_local())
                continue;
            if (wsym.is_undefined())
                continue;
            if (!wsym.is_type_data())
                continue;

            // Get a global symbol and the segment where it resides
            u32 seg_index = wsym.info.value.data_ref.segment;
            // data segment
            const WasmDataSegment &seg = obj->data_segments[seg_index];
            auto oseg = OutputSegment::get_or_create(ctx, seg.name);
            auto frag_offset = oseg->get_frag_offset(wsym.info.name);
            if (frag_offset.has_value())
                return;
            else {
                InputFragment *seg_ifrag = obj->data_ifrags[seg_index];
                oseg->merge(ctx, seg, seg_ifrag);
            }
        }
    }

    // assign offset to segments
    for (auto &[name, seg] : ctx.segments) {
        offset = align(offset, seg.p2align);
        seg.set_virtual_address(offset);
        Debug(ctx) << "Segment: " << name << " offset: 0x" << offset
                   << " size: 0x" << seg.get_size();
        offset += seg.get_size();
    }

    // assign virtual address to data symbols
    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object)
            return;
        ObjectFile *obj = static_cast<ObjectFile *>(file);

        for (auto &wsym : obj->symbols) {
            if (wsym.is_binding_local())
                continue;
            if (wsym.is_undefined())
                continue;
            if (!wsym.is_type_data())
                continue;

            u32 seg_index = wsym.info.value.data_ref.segment;
            const WasmDataSegment &seg = obj->data_segments[seg_index];
            auto oseg = OutputSegment::get_or_create(ctx, seg.name);
            i32 oseg_va = oseg->get_virtual_address();
            i32 frag_offset = oseg->get_frag_offset(seg.name).value();

            Symbol *sym = get_symbol(ctx, wsym.info.name);
            sym->virtual_address =
                oseg_va + frag_offset + wsym.info.value.data_ref.offset;
            Debug(ctx) << "Data symbol: " << sym->name << " va: 0x"
                       << sym->virtual_address;
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
