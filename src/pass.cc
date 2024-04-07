#include "pass.h"
#include "common/log.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/chunk.h"

namespace xld::wasm {

void resolve_symbols(Context &ctx) {
    // Add symbols to the global symbol table
    for (InputFile *file : ctx.files) {
        file->resolve_symbols(ctx);
    }

    // resolve all undefined symbols
    for (InputFile *file : ctx.files) {
        for (WasmSymbol &wsym : file->symbols) {
            if (wsym.is_undefined()) {
                Symbol *sym = get_symbol(ctx, wsym.info.name);
                if (sym->is_undefined()) {
                    Error(ctx) << "Undefined symbol: " << wsym.info.name;
                } else {
                    // Debug(ctx) << "Resolved symbol: " << wsym.info.name;
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

void create_internal_file(Context &ctx) {
    ObjectFile *obj = ObjectFile::create(ctx, "<internal>");

    // TODO: add synthetic symbols
    static WasmGlobalType global_type_i32 = {ValType(WASM_TYPE_I32), false};
    static WasmGlobalType global_type_i64 = {ValType(WASM_TYPE_I64), false};
    static WasmGlobalType mutable_global_type_i32 = {ValType(WASM_TYPE_I32),
                                                     true};
    static WasmGlobalType mutable_global_typeI64 = {ValType(WASM_TYPE_I64),
                                                    true};

    // TODO: PIC
    // TODO: wasm64
    WasmSymbolInfo stack_pointer_info = {
        .name = "__stack_pointer",
        .kind = WASM_SYMBOL_TYPE_GLOBAL,
        .flags = WASM_SYMBOL_BINDING_GLOBAL,
    };
    WasmSymbol stack_pointer_s(stack_pointer_info, &global_type_i32, nullptr,
                               nullptr);
    // TODO: writeInitExpr equivalent
    // https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/WriterUtils.cpp#L169
    std::string *sp = new std::string("\x7f\x01\x41\x80\x88\x04\x0b");
    WasmGlobal stack_pointer_g =
        WasmGlobal{.type =
                       WasmGlobalType{
                           .type = ValType(WASM_TYPE_I32),
                           .mut = false,
                       },
                   .init_expr = int_const(0),
                   .symbol_name = "__stack_pointer",
                   .span = std::span<u8>((u8 *)sp->data(), sp->size())};
    get_symbol(ctx, "__stack_pointer")->is_alive = true;

    obj->symbols.push_back(stack_pointer_s);
    obj->globals.push_back(stack_pointer_g);

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
    push(ctx.global = new GlobalSection());
    push(ctx.name = new NameSection());

    std::mutex m;
    tbb::parallel_for_each(ctx.files, [&](InputFile *file) {
        if (file->kind != InputFile::Object) {
            return;
        }
        ObjectFile *obj = static_cast<ObjectFile *>(file);
        for (WasmGlobal g : obj->globals) {
            ctx.globals.emplace_back(g);
        }
        for (WasmSignature s : obj->signatures) {
            ctx.signatures.emplace_back(s);
        }
        for (WasmFunction f : obj->functions) {
            Debug(ctx) << "Function: " << f.symbol_name;
            u32 obj_sig_index = f.sig_index;
            {
                std::lock_guard<std::mutex> lock(m);
                f.sig_index = ctx.signatures.size();
                ctx.signatures.emplace_back(obj->signatures[obj_sig_index]);
            }
            ctx.functions.emplace_back(f);
        }
    });
}

u64 compute_section_sizes(Context &ctx) {
    tbb::parallel_for_each(ctx.chunks, [&](Chunk *chunk) {
        chunk->loc.size = chunk->compute_section_size(ctx);
    });

    u64 offset = 0;
    for (Chunk *chunk : ctx.chunks) {
        chunk->loc.offset = offset;
        offset += chunk->loc.size;
        Debug(ctx) << "Section: " << chunk->name
                   << " offset: " << chunk->loc.offset
                   << " size: " << chunk->loc.size;
    }
    Debug(ctx) << "Total size: " << offset;
    return offset;
}

void copy_chunks(Context &ctx) {
    tbb::parallel_for_each(ctx.chunks, [&](Chunk *chunk) {
        Debug(ctx) << "Copying chunk: " << chunk->name;
        chunk->copy_buf(ctx);
    });
}

} // namespace xld::wasm
