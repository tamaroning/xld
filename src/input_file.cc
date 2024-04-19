
#include "common/leb128.h"
#include "common/log.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/symbol.h"

namespace xld::wasm {

u64 InputSection::get_size() { return span.size() - loc.copy_start; }

void InputSection::write_to(Context &ctx, u8 *buf) {
    if (wrote.exchange(true))
        return;

    Debug(ctx) << "writing section: " << name << " " << obj->filename;
    memcpy(buf, span.data() + loc.copy_start, get_size());
}

#define WASM_RELOC(x, y)                                                       \
    case (x):                                                                  \
        return (#x);

static std::string_view get_reloc_type_name(u8 type) {
    switch (type) {
#include "wasm/wasm_relocs.def"
    default:
        return "<unknown>";
    }
}

#undef WASM_RELOC

void InputSection::apply_reloc(Context &ctx, u64 osec_content_file_offset) {
    if (reloc_applied.exchange(true))
        return;

    Debug(ctx) << "applying relocs for " << name;
    u8 *isec_base = ctx.buf + osec_content_file_offset + loc.offset;
    for (WasmRelocation &reloc : relocs) {
        u8 *reloc_loc = isec_base + (reloc.offset - loc.copy_start);

        switch (reloc.type) {
        case R_WASM_FUNCTION_INDEX_LEB: {
            std::string &name = this->obj->symbols[reloc.index].info.name;
            Symbol *sym = get_symbol(ctx, name);
            u32 index = sym->index;
            encode_uleb128(index, reloc_loc, 5);
        } break;
        case R_WASM_GLOBAL_INDEX_LEB: {
            std::string &name = this->obj->symbols[reloc.index].info.name;
            Symbol *sym = get_symbol(ctx, name);
            u32 index = sym->index;
            encode_uleb128(index, reloc_loc, 5);
        } break;
        default:
            Error(ctx) << "TODO: Unknown reloc type: "
                       << get_reloc_type_name(reloc.type);
        }
    }
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
    ASSERT(index >= num_imported_functions &&
           index < num_imported_functions + functions.size());
    return functions[index - num_imported_functions];
}

bool ObjectFile::is_defined_global(u32 index) {
    return index >= num_imported_globals &&
           index < num_imported_globals + globals.size();
}

WasmGlobal &ObjectFile::get_defined_global(u32 index) {
    ASSERT(index >= num_imported_globals &&
           index < num_imported_globals + globals.size());
    return globals[index - num_imported_globals];
}

bool ObjectFile::is_defined_memories(u32 index) {
    return index >= num_imported_memories &&
           index < num_imported_memories + memories.size();
}

WasmLimits &ObjectFile::get_defined_memories(u32 index) {
    ASSERT(index >= num_imported_memories &&
           index < num_imported_memories + memories.size());
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
    // Register all symbols in symtab to global symbol map
    for (WasmSymbol &wsym : this->symbols) {
        if (wsym.is_binding_local())
            continue;
        // we only care about global or weak symbols
        if (!wsym.is_binding_global() && !wsym.is_binding_weak())
            continue;
        if (wsym.is_undefined())
            continue;

        // Non-local symbol has a unique name. So we can get a Symbol* safely.
        Symbol *sym = get_symbol(ctx, wsym.info.name);

        if (wsym.is_exported())
            sym->is_exported = true;

        if (sym->is_defined() && sym->binding == Symbol::Binding::Global &&
            wsym.is_defined() && wsym.is_binding_global()) {
            Error(ctx) << "Duplicate strong symbol definition: "
                       << wsym.info.name << '\n';
        }

        if (!sym->wsym.has_value() ||
            get_rank(wsym) > get_rank(sym->wsym.value())) {
            sym->file = this;
            sym->wsym = wsym;
            sym->elem_index = wsym.info.value.element_index;
            if (wsym.is_type_function())
                sym->isec = this->code.value();
            if (wsym.is_type_data())
                sym->isec = this->data.value();

            if (wsym.is_binding_weak())
                sym->binding = Symbol::Binding::Weak;
            else if (wsym.is_binding_global())
                sym->binding = Symbol::Binding::Global;
        }
    };
}

} // namespace xld::wasm
