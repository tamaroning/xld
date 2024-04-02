#include "input_file.h"
#include "wasm.h"
#include "xld.h"

namespace xld::wasm {

template <typename E>
void resolve_symbols(Context<E> &ctx) {
    // Add symbols to the global symbol table
    for (InputFile<E> *file : ctx.files) {
        file->resolve_symbols(ctx);
    }

    // resolve all imports
}

template <typename E>
void create_internal_file(Context<E> &ctx) {
    Warn(ctx) << "TODO: create internal file";
    // env.__stack_pointer
}

using E = WASM32;
template void resolve_symbols(Context<E> &);
template void create_internal_file(Context<E> &);

} // namespace xld::wasm
