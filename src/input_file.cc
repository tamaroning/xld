#include "wasm.h"
#include "xld.h"

namespace xld::wasm {

template <typename E>
InputFile<E>::InputFile(Context<E> &ctx, MappedFile *mf)
    : mf(mf), filename(mf->name) {
    // TODO: sizeof (WasmHdr)
    // TODO: check magic

    // wasm_sections
    // shstrtab
}

template <typename E>
ObjectFile<E>::ObjectFile(Context<E> &ctx, MappedFile *mf)
    : InputFile<E>(ctx, mf) {
        SyncOut(ctx) << "ObjectFile::ObjectFile\n";
    }

template <typename E>
ObjectFile<E> *ObjectFile<E>::create(Context<E> &ctx, MappedFile *mf) {
    SyncOut(ctx) << "ObjectFile::create\n";
    ObjectFile<E> *obj = new ObjectFile<E>(ctx, mf);
    SyncOut(ctx) << "ObjectFile::create 1\n";
    ctx.obj_pool.push_back(std::unique_ptr<ObjectFile<E>>(obj));
    SyncOut(ctx) << "ObjectFile::create end\n";
    return obj;
}

using E = WASM32;

template class InputFile<E>;
template class ObjectFile<E>;

} // namespace xld::wasm
