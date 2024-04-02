#include "wasm.h"
#include "xld.h"

namespace xld::wasm {


void resolve_symbols(Context &ctx) {
    // Add symbols to the global symbol table
    for (InputFile *file : ctx.files) {
        file->resolve_symbols(ctx);
    }

    // resolve all imports
}


void create_internal_file(Context &ctx) {
    Warn(ctx) << "TODO: create internal file";
    // env.__stack_pointer
}

} // namespace xld::wasm
