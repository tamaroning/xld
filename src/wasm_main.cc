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
        SyncOut(ctx) << "Open " << path_clean(path) << "\n";
        ctx.objs.push_back(
            ObjectFile<E>::create(ctx, must_open_file(ctx, path)));
    }

    if (ctx.objs.empty())
        Fatal(ctx) << "no input files\n";

    Debug(ctx) << "aaa";

    resolve_symbols(ctx);

    return 0;
}

template int wasm_main<WASM32>(int, char **);
// template int wasm_main<WASM64>(int, char **);

} // namespace xld::wasm
