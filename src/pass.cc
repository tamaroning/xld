#include "wasm.h"
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
                } else {
                    Debug(ctx) << "Resolved symbol: " << wsym.info.name;
                }
            }
        }
    }
}

void create_internal_file(Context &ctx) {
    Warn(ctx) << "TODO: create internal file";
    ObjectFile* obj = ObjectFile::create(ctx);

    ctx.files.push_back(obj);
}

} // namespace xld::wasm
