#pragma once

#include "common/mmap.h"
#include "section.h"
#include "wao.h"
#include "xld.h"

namespace xld::wasm {

struct WASM32 {};
struct WASM64 {};

template <typename E>
class InputFile {
  public:
    InputFile(Context<E> &ctx, MappedFile *mf);

    virtual ~InputFile() = default;

    // template <typename T>
    // std::span<T> get_data(Context<E> &ctx, i64 idx);

    // virtual void resolve_symbols(Context<E> &ctx) = 0;
    // void clear_symbols();

    MappedFile *mf = nullptr;

    std::string filename;
};

template <typename E>
class ObjectFile : public InputFile<E> {
  public:
    static ObjectFile<E> *create(Context<E> &ctx, MappedFile *mf);

    void parse(Context<E> &ctx);

    // parse custom sections
    void parse_linking_sec(Context<E> &ctx, const u8 *&p, const u32 size);
    void parse_reloc_sec(Context<E> &ctx, const u8 *&p, const u32 size);
    void parse_name_sec(Context<E> &ctx, const u8 *&p, const u32 size);
    // parse misc
    WasmInitExpr parse_init_expr(Context<E> &ctx, const u8 *&data);

    void dump(Context<E> &ctx);

  private:
    ObjectFile(Context<E> &ctx, MappedFile *mf);

    std::vector<std::unique_ptr<InputSection<E>>> sections;
    // type (func signature) section
    // this span contains the first 0x60
    std::vector<std::span<const u8>> func_types;
    // import section
    std::vector<WasmImport> imports;

    // func section
    std::vector<u32> func_sec_type_indices;

    // table section

    // memory section
    std::vector<WasmLimits> memories;

    // global section
    std::vector<WasmGlobal> globals;

    // export section
    std::vector<WasmExport> exports;

    // start section

    // elem section

    // data count section

    // code section
    std::vector<std::span<const u8>> codes;

    // data section

    // linking section
    std::vector<WasmSymbolInfo> symbols;

    // reloc.* section
    // stored in InputSection

    // name section
    std::string module_name;
};

} // namespace xld::wasm
