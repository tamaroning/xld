#include "wasm.h"
#include "xld.h"

int main(int argc, char **argv) {
    return xld::wasm::linker_main<xld::wasm::WASM32>(argc, argv);
}
