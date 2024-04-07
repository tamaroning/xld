
#include "common/log.h"
#include "xld.h"

namespace xld::wasm {

void InputSection::write_to(Context &ctx, u8 *buf) {
    memcpy(buf, span.data(), span.size());
}

InputFile::InputFile(Context &ctx, const std::string &filename, MappedFile *mf)
    : mf(mf), filename(filename) {
    if (!mf)
        return;

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

ObjectFile::ObjectFile(Context &ctx, const std::string &filename,
                       MappedFile *mf)
    : InputFile(ctx, filename, mf) {
    kind = Object;
}

ObjectFile *ObjectFile::create(Context &ctx, const std::string &filename,
                               MappedFile *mf) {
    ObjectFile *obj = new ObjectFile(ctx, filename, mf);
    ctx.obj_pool.push_back(std::unique_ptr<ObjectFile>(obj));
    return obj;
}

bool ObjectFile::is_defined_function(u32 index) {
    return index >= num_imported_functions &&
           index < num_imported_functions + functions.size();
}

WasmFunction &ObjectFile::get_defined_function(u32 index) {
    return functions[index - num_imported_functions];
}

bool ObjectFile::is_defined_global(u32 index) {
    return index >= num_imported_globals &&
           index < num_imported_globals + globals.size();
}

WasmGlobal &ObjectFile::get_defined_global(u32 index) {
    return globals[index - num_imported_globals];
}

bool ObjectFile::is_defined_memories(u32 index) {
    return index >= num_imported_memories &&
           index < num_imported_memories + memories.size();
}

WasmLimits &ObjectFile::get_defined_memories(u32 index) {
    return memories[index - num_imported_memories];
}

bool ObjectFile::is_valid_function_symbol(u32 Index) {
    return Index < symbols.size() && symbols[Index].is_type_function();
}

bool ObjectFile::is_valid_table_symbol(u32 Index) {
    return Index < symbols.size() && symbols[Index].is_type_table();
}

bool ObjectFile::is_valid_global_symbol(u32 Index) {
    return Index < symbols.size() && symbols[Index].is_type_global();
}

bool ObjectFile::is_valid_tag_symbol(u32 Index) {
    return Index < symbols.size() && symbols[Index].is_type_tag();
}

bool ObjectFile::is_valid_data_symbol(u32 Index) {
    return Index < symbols.size() && symbols[Index].is_type_data();
}

bool ObjectFile::is_valid_section_symbol(u32 Index) {
    return Index < symbols.size() && symbols[Index].is_type_section();
}

void ObjectFile::resolve_symbols(Context &ctx) {
    // Register symbols
    for (WasmSymbol &wsym : this->symbols) {
        // we care about global or weak symbols
        if (!wsym.is_binding_global() && !wsym.is_binding_weak())
            continue;
        if (wsym.is_undefined())
            continue;

        Symbol *sym = get_symbol(ctx, wsym.info.name);

        bool is_weak_def = wsym.is_defined() && wsym.is_binding_weak();
        bool is_global_def = wsym.is_defined() && wsym.is_binding_global();

        bool prev_is_weak_def =
            sym->is_defined() && sym->binding == Symbol::Binding::Weak;
        bool prev_is_global_def =
            sym->is_defined() && sym->binding == Symbol::Binding::Global;

        Debug(ctx) << "definiton: " << wsym.info.name;
        if (prev_is_global_def && is_global_def) {
            Error(ctx) << "duplicate strong symbol definition: "
                       << wsym.info.name << '\n';
        }

        bool should_override = is_global_def;
        if (should_override) {
            sym->file = this;
            if (wsym.is_binding_weak())
                sym->binding = Symbol::Binding::Weak;
            else if (wsym.is_binding_global())
                sym->binding = Symbol::Binding::Global;
        }
    }

    // imports and exports
    // for (const WasmImport& import: this->imports) {
    // Symbol *sym = get_symbol(ctx, import.name);
    // sym->is_imported = true;
    // }
}

} // namespace xld::wasm
