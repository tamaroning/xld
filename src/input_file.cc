
#include "xld_private/input_file.h"
#include "common/leb128.h"
#include "common/log.h"
#include "wasm/object.h"
#include "xld.h"
#include "xld_private/symbol.h"

namespace xld::wasm {

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

void InputFragment::write_to(Context &ctx, u8 *buf) {
    memcpy(buf, span.data(), get_size());
}

u64 InputFragment::get_size() { return span.size(); }

void InputFragment::apply_reloc(Context &ctx, u64 osec_content_file_offset) {
    // https://github.com/WebAssembly/tool-conventions/blob/main/Linking.md
    // Note that for all relocation types, the bytes being relocated:
    //   from offset to offset + 5 for LEB/SLEB relocations;
    //   from offset to offset + 10 for LEB64/SLEB64 relocations;
    //   from offset to offset + 4 for I32 relocations;
    //   or from offset to offset + 8 for I64;
    //
    // For R_WASM_MEMORY_ADDR_*, R_WASM_FUNCTION_OFFSET_I32, and
    // R_WASM_SECTION_OFFSET_I32 relocations (and their 64-bit counterparts)
    // the following field is additionally present:
    //    addend varint32: addend to add to the address
    u8 *frag_base = ctx.buf + osec_content_file_offset + out_offset;
    for (WasmRelocation &reloc : relocs) {
        u8 *reloc_loc = frag_base + (reloc.offset - in_offset);

        switch (reloc.type) {
        case R_WASM_FUNCTION_INDEX_LEB: {
            std::string &name = this->obj->symbols[reloc.index].info.name;
            Symbol *sym = get_symbol(ctx, name);
            u32 val = sym->index;
            encode_uleb128(val, reloc_loc, 5);
        } break;
        case R_WASM_GLOBAL_INDEX_LEB: {
            std::string &name = this->obj->symbols[reloc.index].info.name;
            Symbol *sym = get_symbol(ctx, name);
            u32 val = sym->index;
            encode_uleb128(val, reloc_loc, 5);
        } break;
        case R_WASM_MEMORY_ADDR_LEB: {
            std::string &name = this->obj->symbols[reloc.index].info.name;
            Symbol *sym = get_symbol(ctx, name);
            u32 val = sym->virtual_address + reloc.addend;
            encode_uleb128(val, reloc_loc, 5);
        } break;
        default:
            Error(ctx) << "TODO: Unknown reloc type: "
                       << get_reloc_type_name(reloc.type);
        }
        {
            std::string &name = this->obj->symbols[reloc.index].info.name;
            Symbol &sym = *get_symbol(ctx, name);
            Debug(ctx) << "- reloc for symbol: " << sym.name << " ("
                       << get_reloc_type_name(reloc.type) << ")";
        }
    }
}

u64 InputSection::get_size() { return span.size(); }

void InputSection::write_to(Context &ctx, u8 *buf) {
    Debug(ctx) << "writing section: " << name << " " << obj->filename;
    memcpy(buf, span.data(), get_size());
}

void InputSection::apply_reloc(Context &ctx, u64 osec_content_file_offset) {}

InputFile::InputFile(Context &ctx, const std::string &filename, MappedFile *mf)
    : mf(mf), filename(filename) {
    if (!mf)
        return;

    if (mf->size < sizeof(WasmObjectHeader))
        Fatal(ctx) << filename << ": file too small";

    const WasmObjectHeader *ohdr =
        reinterpret_cast<WasmObjectHeader *>(mf->data);
    if (!(ohdr->magic[0] == WASM_MAGIC[0] && ohdr->magic[1] == WASM_MAGIC[1] &&
          ohdr->magic[2] == WASM_MAGIC[2] && ohdr->magic[3] == WASM_MAGIC[3]))
        Fatal(ctx) << filename << ": bad magic";

    if (ohdr->version != WASM_VERSION)
        Warn(ctx) << filename << " is version " << ohdr->version
                  << ". xld only supports" << WASM_VERSION;
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

InputFragment *ObjectFile::get_function_code(u32 index) {
    ASSERT(index >= num_imported_functions &&
           index < num_imported_functions + functions.size());
    return code_ifrags[index - num_imported_functions];
}

std::vector<InputFragment *> &ObjectFile::get_function_codes() {
    return code_ifrags;
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

bool ObjectFile::is_valid_function_symbol(u32 index) {
    return index < symbols.size() && symbols[index].is_type_function();
}

bool ObjectFile::is_valid_table_symbol(u32 index) {
    return index < symbols.size() && symbols[index].is_type_table();
}

bool ObjectFile::is_valid_global_symbol(u32 index) {
    return index < symbols.size() && symbols[index].is_type_global();
}

bool ObjectFile::is_valid_tag_symbol(u32 index) {
    return index < symbols.size() && symbols[index].is_type_tag();
}

bool ObjectFile::is_valid_data_symbol(u32 index) {
    return index < symbols.size() && symbols[index].is_type_data();
}

bool ObjectFile::is_valid_section_symbol(u32 index) {
    return index < symbols.size() && symbols[index].is_type_section();
}

static void override_symbol(Context &ctx, Symbol *sym, ObjectFile *file,
                            WasmSymbol &wsym) {
    sym->file = file;
    sym->wsym = wsym;
    sym->elem_index = wsym.info.value.element_index;
    if (wsym.is_type_function() && file->is_defined_function(sym->elem_index))
        sym->ifrag = file->get_function_code(sym->elem_index);
    if (wsym.is_type_data())
        sym->ifrag = file->data_ifrags[sym->elem_index];

    if (wsym.is_binding_weak())
        sym->binding = Symbol::Binding::Weak;
    else if (wsym.is_binding_global())
        sym->binding = Symbol::Binding::Global;
}

void ObjectFile::resolve_symbols(Context &ctx) {
    // Register all symbols in symtab to global symbol map
    for (WasmSymbol &wsym : this->symbols) {
        if (wsym.is_binding_local())
            continue;

        // Non-local symbol has a unique name.
        Symbol *sym = get_symbol(ctx, wsym.info.name);
        std::scoped_lock lock(sym->mu);

        if (!sym->wsym.has_value()) {
            override_symbol(ctx, sym, this, wsym);
            continue;
        }

        if (wsym.is_exported())
            sym->is_exported = true;

        if (sym->is_defined() && sym->binding == Symbol::Binding::Global &&
            wsym.is_defined() && wsym.is_binding_global()) {
            Error(ctx) << "Duplicate strong symbol definition: "
                       << wsym.info.name;
        }

        if (!sym->wsym.has_value() ||
            get_rank(wsym) > get_rank(sym->wsym.value())) {
            override_symbol(ctx, sym, this, wsym);
        }
    };
}

} // namespace xld::wasm
