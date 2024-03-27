#include "xld.h"

namespace xld::wasm {

template <typename E>
void resolve_symbols(Context<E> &ctx) {
    Warn(ctx) << "TODO: resolve symbols";
}

using E = WASM32;
template void resolve_symbols(Context<E> &);

} // namespace xld::wasm
