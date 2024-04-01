
#include "input_file.h"
#include "object/wao_symbol.h"
#include "wasm.h"
#include "xld.h"

namespace xld::wasm {

// If we haven't seen the same `key` before, create a new instance
// of Symbol and returns it. Otherwise, returns the previously-
// instantiated object. `key` is usually the same as `name`.
/*
template <typename E>
Symbol<E> *get_symbol(Context<E> &ctx, std::string_view key,
                      std::string_view name) {
    typename decltype(ctx.symbol_map)::const_accessor acc;
    ctx.symbol_map.insert(acc, {key, Symbol<E>(name)});
    return const_cast<Symbol<E> *>(&acc->second);
}
*/

template <typename E>
InputFile<E>::InputFile(Context<E> &ctx, MappedFile *mf)
    : mf(mf), filename(mf->name) {

    if (mf->size < sizeof(WasmObjectHeader))
        Fatal(ctx) << filename << ": file too small\n";

    const WasmObjectHeader *ohdr =
        reinterpret_cast<WasmObjectHeader *>(mf->data);
    if (!(ohdr->magic[0] == WASM_MAGIC[0] && ohdr->magic[1] == WASM_MAGIC[1] &&
          ohdr->magic[2] == WASM_MAGIC[2] && ohdr->magic[3] == WASM_MAGIC[3]))
        Fatal(ctx) << filename << ": bad magic\n";

    if (ohdr->version != WASM_VERSION)
        Fatal(ctx) << filename << " is version " << ohdr->version
                   << ". xld only supports" << WASM_VERSION << '\n';
}

template <typename E>
ObjectFile<E>::ObjectFile(Context<E> &ctx, MappedFile *mf)
    : InputFile<E>(ctx, mf) {}

template <typename E>
ObjectFile<E> *ObjectFile<E>::create(Context<E> &ctx, MappedFile *mf) {
    ObjectFile<E> *obj = new ObjectFile<E>(ctx, mf);
    ctx.obj_pool.push_back(std::unique_ptr<ObjectFile<E>>(obj));
    return obj;
}

template <typename E>
void ObjectFile<E>::resolve_symbols(Context<E> &ctx) {
    // TODO:
    Debug(ctx) << "TODO:";
}

using E = WASM32;

template class InputFile<E>;
template class ObjectFile<E>;

} // namespace xld::wasm
