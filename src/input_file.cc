
#include "common/log.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/symbol.h"

namespace xld::wasm {

u64 InputSection::get_size() { return span.size() - loc.copy_start; }

void InputSection::write_to(Context &ctx, u8 *buf) {
    memcpy(buf, span.data() + loc.copy_start, get_size());
}

/*
WASM_RELOC(R_WASM_FUNCTION_INDEX_LEB, 0)
WASM_RELOC(R_WASM_TABLE_INDEX_SLEB, 1)
WASM_RELOC(R_WASM_TABLE_INDEX_I32, 2)
WASM_RELOC(R_WASM_MEMORY_ADDR_LEB, 3)
WASM_RELOC(R_WASM_MEMORY_ADDR_SLEB, 4)
WASM_RELOC(R_WASM_MEMORY_ADDR_I32, 5)
WASM_RELOC(R_WASM_TYPE_INDEX_LEB, 6)
WASM_RELOC(R_WASM_GLOBAL_INDEX_LEB, 7)
WASM_RELOC(R_WASM_FUNCTION_OFFSET_I32, 8)
WASM_RELOC(R_WASM_SECTION_OFFSET_I32, 9)
WASM_RELOC(R_WASM_TAG_INDEX_LEB, 10)
WASM_RELOC(R_WASM_MEMORY_ADDR_REL_SLEB, 11)
WASM_RELOC(R_WASM_TABLE_INDEX_REL_SLEB, 12)
WASM_RELOC(R_WASM_GLOBAL_INDEX_I32, 13)
WASM_RELOC(R_WASM_MEMORY_ADDR_LEB64, 14)
WASM_RELOC(R_WASM_MEMORY_ADDR_SLEB64, 15)
WASM_RELOC(R_WASM_MEMORY_ADDR_I64, 16)
WASM_RELOC(R_WASM_MEMORY_ADDR_REL_SLEB64, 17)
WASM_RELOC(R_WASM_TABLE_INDEX_SLEB64, 18)
WASM_RELOC(R_WASM_TABLE_INDEX_I64, 19)
WASM_RELOC(R_WASM_TABLE_NUMBER_LEB, 20)
WASM_RELOC(R_WASM_MEMORY_ADDR_TLS_SLEB, 21)
WASM_RELOC(R_WASM_FUNCTION_OFFSET_I64, 22)
WASM_RELOC(R_WASM_MEMORY_ADDR_LOCREL_I32, 23)
WASM_RELOC(R_WASM_TABLE_INDEX_REL_SLEB64, 24)
WASM_RELOC(R_WASM_MEMORY_ADDR_TLS_SLEB64, 25)
WASM_RELOC(R_WASM_FUNCTION_INDEX_I32, 26)
*/

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
    Debug(ctx) << "applying relocs for " << name;
    u8 *isec_base = ctx.buf + osec_content_file_offset + loc.offset;
    for (WasmRelocation &reloc : relocs) {
        u8 *reloc_loc = isec_base + (reloc.offset - loc.copy_start);

        switch (reloc.type) {

        case R_WASM_FUNCTION_INDEX_LEB: {
            std::string &name = this->obj->symbols[reloc.index].info.name;
            Debug(ctx) << name << " is defined in"
                       << get_symbol(ctx, name)->file->filename;
        } break;
        case R_WASM_GLOBAL_INDEX_LEB:
        default:
            Error(ctx) << "Unknown reloc type: "
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
