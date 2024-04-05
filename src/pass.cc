#include "object/wao_basic.h"
#include "object/wao_symbol.h"
#include "xld.h"

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
                    sym->is_used_in_regular_obj = true;
                } else {
                    // Debug(ctx) << "Resolved symbol: " << wsym.info.name;
                }
            }
        }
    }
}

/*
static UndefinedGlobal *
createUndefinedGlobal(StringRef name, llvm::wasm::WasmGlobalType *type) {
    auto *sym = cast<UndefinedGlobal>(
        symtab->addUndefinedGlobal(name, std::nullopt, std::nullopt,
                                   WASM_SYMBOL_UNDEFINED, nullptr, type));
    config->allowUndefinedSymbols.insert(sym->getName());
    sym->isUsedInRegularObj = true;
    return sym;
}
*/

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
    WasmGlobal stack_pointer_g = WasmGlobal{
        .type =
            WasmGlobalType{
                .type = ValType(WASM_TYPE_I32),
                .mut = false,
            },
        .init_expr = int_const(0),
        .symbol_name = "__stack_pointer",
    };
    get_symbol(ctx, "__stack_pointer")->is_used_in_regular_obj = true;

    obj->symbols.push_back(stack_pointer_s);
    obj->globals.push_back(stack_pointer_g);

    ctx.files.push_back(obj);
}

void create_synthetic_sections(Context &ctx) {
    Warn(ctx) << "TODO: whdr";
    auto push = [&](Chunk *s) {
        ctx.chunks.push_back(s);
        ctx.chunk_pool.emplace_back(s);
    };

    push(new OutputWhdr());
}

void copy_chunks(Context &ctx) {
    Warn(ctx) << "TODO: copy_chunks";
    tbb::parallel_for_each(ctx.chunks, [&](Chunk *chunk) {
        Debug(ctx) << "Copying chunk: " << chunk->name;
        chunk->copy_buf(ctx);
    });
}

} // namespace xld::wasm
