#include "common/file.h"
#include "wasm.h"
#include "xld.h"

namespace xld::wasm {

template <typename E>
int wasm_main(int argc, char **argv) {
    // TODO: main logic here
    Context<E> ctx;

    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];
        SyncOut(ctx) << path << "\n";
        ctx.objs.push_back(
            ObjectFile<E>::create(ctx, must_open_file(ctx, path)));
    }

    SyncOut(ctx) << "a";
    Warn(ctx) << "a";
    Error(ctx) << "a";
    Fatal(ctx) << "a";

    return 0;
}

template int wasm_main<WASM32>(int, char **);
// template int wasm_main<WASM64>(int, char **);

} // namespace xld::wasm
