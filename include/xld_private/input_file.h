#pragma once

#include "common/mmap.h"
#include "common/system.h"
#include "wasm/symbol.h"
#include "xld_private/chunk.h"
#include <atomic>

namespace xld::wasm {

// forward-decl
class Context;
class ObjectFile;

class InputSection {
  public:
    InputSection(u8 sec_id, u32 index, ObjectFile *obj, std::string name,
                 std::span<const u8> span)
        : sec_id(sec_id), index(index), obj(obj), name(name), span(span) {}

    void write_to(Context &ctx, u8 *buf);
    u64 get_size();
    void apply_reloc(Context &ctx, u64 osec_content_offset);

    u8 sec_id;
    // section index in the object file
    u32 index;
    ObjectFile *obj;
    std::string name;
    std::vector<WasmRelocation> relocs;

    u64 out_offset = 0;

  private:
    std::span<const u8> span;
};

class InputFragment {
  public:
    InputFragment(u32 sec_index, ObjectFile *obj, std::span<const u8> span,
                  u64 in_offset)
        : sec_index(sec_index), obj(obj), in_offset(in_offset), span(span) {}

    void write_to(Context &ctx, u8 *buf);
    u64 get_size();
    void apply_reloc(Context &ctx, u64 osec_content_offset);

    u32 sec_index;
    ObjectFile *obj;
    std::vector<WasmRelocation> relocs;
    // offset from beginning of the content of the input section
    // not pointing the first number of bytes instead the following content.
    u64 in_offset = 0;
    // offset from beginning of the content of the output section
    u64 out_offset = 0;
    u64 out_size_offset = 0;

    // does not contain size info. Only its body.
    std::span<const u8> span;
};

class InputFile {
  public:
    // Only object file is supported
    enum Kind {
        Object,
    };

    InputFile(Context &ctx, const std::string &filename, MappedFile *mf);

    virtual ~InputFile() = default;

    // template <typename T>
    // std::span<T> get_data(Context &ctx, i64 idx);

    virtual void resolve_symbols(Context &ctx) = 0;
    // void clear_symbols();

    // This pointer can be nullptr for the internal files
    MappedFile *mf = nullptr;
    std::string filename;
    Kind kind = Object;

    std::vector<WasmSymbol*> symbols;
};

class ObjectFile : public InputFile {
  public:
    static ObjectFile *create(Context &ctx, const std::string &filename,
                              MappedFile *mf = nullptr);

    bool is_defined_function(u32 index);
    WasmFunction &get_defined_function(u32 index);
    InputFragment *get_function_code(u32 index);
    std::vector<InputFragment *> &get_function_codes();

    bool is_defined_global(u32 index);
    WasmGlobal &get_defined_global(u32 index);
    bool is_defined_memories(u32 index);
    WasmLimits &get_defined_memories(u32 index);

    bool is_valid_function_symbol(u32 index);
    bool is_valid_table_symbol(u32 index);
    bool is_valid_global_symbol(u32 index);
    bool is_valid_tag_symbol(u32 index);
    bool is_valid_data_symbol(u32 index);
    bool is_valid_section_symbol(u32 index);

    void parse(Context &ctx);
    // parse custom sections
    void parse_linking_sec(Context &ctx, const u8 *&p, const u32 size);
    void parse_reloc_sec(Context &ctx, const u8 *&p, const u32 size);
    void parse_name_sec(Context &ctx, const u8 *&p, const u32 size);
    // parse misc
    WasmInitExpr parse_init_expr(Context &ctx, const u8 *&data);

    void resolve_symbols(Context &ctx);

    void dump(Context &ctx);

    // Spans of all sections
    std::vector<InputSection *> sections;
    std::vector<InputSection *> customs;

    std::vector<WasmSignature> signatures;
    std::vector<WasmImport> imports;
    // TODO: table section
    std::vector<WasmFunction> functions;
    std::vector<WasmLimits> memories;
    std::vector<WasmGlobal> globals;
    std::vector<WasmExport> exports;
    std::vector<InputFragment *> code_ifrags;
    std::vector<WasmElemSegment> elem_segments;

    // from the "name" section
    std::string module_name;
    // from the "datacount" and "data" sections
    u32 data_count = 0;
    std::vector<WasmDataSegment> data_segments;
    std::vector<InputFragment *> data_ifrags;

    // from the "linking" section
    WasmLinkingData linking_data;

    u32 num_imported_globals = 0;
    u32 num_imported_functions = 0;
    u32 num_imported_tables = 0;
    u32 num_imported_memories = 0;

  private:
    ObjectFile(Context &ctx, const std::string &filename, MappedFile *mf);
};

} // namespace xld::wasm