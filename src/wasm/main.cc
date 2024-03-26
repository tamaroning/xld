#include "xld.h"

namespace xld::wasm {

template <typename E>
int wasm_main(int argc, char **argv) {
    // TODO: main logic here
    return 0;
}

template int wasm_main<WASM32>(int, char **);
// template int wasm_main<WASM64>(int, char **);

} // namespace xld::wasm
