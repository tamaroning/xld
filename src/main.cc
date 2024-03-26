#include "wasm.h"
#include "xld.h"

int main(int argc, char **argv) {
    return xld::wasm::wasm_main<xld::wasm::WASM32>(argc, argv);
}
