
#include "object/wao_symbol.h"
#include "xld.h"

namespace xld::wasm {

InputFile::InputFile(Context &ctx, MappedFile *mf)
    : mf(mf), filename(mf->name) {

    if (mf->size < sizeof(WasmObjectHeader))
        Fatal(ctx) << filename << ": file too small\n";

    const WasmObjectHeader *ohdr =
        reinterpret_cast<WasmObjectHeader *>(mf->data);
    if (!(ohdr->magic[0] == WASM_MAGIC[0] && ohdr->magic[1] == WASM_MAGIC[1] &&
          ohdr->magic[2] == WASM_MAGIC[2] && ohdr->magic[3] == WASM_MAGIC[3]))
        Fatal(ctx) << filename << ": bad magic\n";

    if (ohdr->version != WASM_VERSION)
        Warn(ctx) << filename << " is version " << ohdr->version
                  << ". xld only supports" << WASM_VERSION << '\n';
}

ObjectFile::ObjectFile(Context &ctx, MappedFile *mf) : InputFile(ctx, mf) {}

ObjectFile *ObjectFile::create(Context &ctx, MappedFile *mf) {
    ObjectFile *obj = new ObjectFile(ctx, mf);
    ctx.obj_pool.push_back(std::unique_ptr<ObjectFile>(obj));
    return obj;
}

void ObjectFile::resolve_symbols(Context &ctx) {
    // Add symbols
    for (WasmSymbol &wsym : this->symbols) {
        // we care about global or weak symbols
        if (!wsym.is_binding_global() && !wsym.is_binding_weak())
            continue;
        if (wsym.is_undefined())
            continue;

        Symbol *sym = get_symbol(ctx, wsym.info.name);

        bool error = !sym->file && !sym->is_weak && !wsym.is_binding_weak();
        if (error) {
            Error(ctx) << "duplicate strong symbol definition: "
                       << wsym.info.name << '\n';
        }
        bool should_override =
            sym->file || (sym->is_weak && !wsym.is_binding_weak());
        if (should_override) {
            sym->file = this;
            sym->is_weak = wsym.is_binding_weak();
        }
    }
}

} // namespace xld::wasm
