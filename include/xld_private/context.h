#pragma once

#include "common/common.h"
#include "common/mmap.h"
#include "common/system.h"
#include "oneapi/tbb/concurrent_set.h"
#include "output_elem.h"
#include "wasm/object.h"
#include "xld_private/chunk.h"
#include "xld_private/input_file.h"

namespace xld::wasm {

// forward-decl
class InputFile;
class ObjectFile;
class Symbol;

struct Context {
    Context() {}

    Context(const Context &) = delete;

    void checkpoint() {
        if (has_error) {
            cleanup();
            _exit(1);
        }
    }

    bool has_error = false;

    // object pools
    tbb::concurrent_vector<std::unique_ptr<ObjectFile>> obj_pool;
    tbb::concurrent_vector<std::unique_ptr<MappedFile>> mf_pool;
    tbb::concurrent_vector<std::unique_ptr<u8[]>> string_pool;
    tbb::concurrent_vector<std::unique_ptr<Chunk>> chunk_pool;
    tbb::concurrent_vector<std::unique_ptr<InputSection>> isec_pool;

    // Symbol table
    // TODO: use xxHash
    tbb::concurrent_hash_map<std::string_view, Symbol> symbol_map;

    // Input files
    std::vector<InputFile *> files;

    // Output chunks
    std::vector<Chunk *> chunks;
    OutputWhdr *whdr = nullptr;
    TypeSection *type = nullptr;
    ImportSection *import = nullptr;
    FunctionSection *function = nullptr;
    TableSection *table = nullptr;
    MemorySection *memory = nullptr;
    GlobalSection *global = nullptr;
    CodeSection *code = nullptr;
    ExportSection *export_ = nullptr;
    NameSection *name = nullptr;

    // Linker synthesized symbols which needs special handling.
    // Other linker synthesized symbols reside in the internal object file.
    WasmTableType indirect_function_table;
    WasmLimits output_memory;
    WasmExport output_memory_export;

    // output imports
    tbb::concurrent_set<Symbol *> import_functions_;
    tbb::concurrent_set<Symbol *> import_globals_;
    tbb::concurrent_vector<Symbol *> functions_;
    tbb::concurrent_vector<Symbol *> globals_;

    tbb::concurrent_vector<Symbol *> export_functions;
    tbb::concurrent_vector<Symbol *> export_globals;

    std::vector<WasmSignature> signatures_;

    tbb::concurrent_vector<InputSection *> codes;
    // TODO: DATA

    // Command-line arguments
    struct {
        bool export_all = false;
        bool allow_undefined = false;
        std::string output_file;
        bool dump_input = false;

        bool color_diagnostics = true;
        std::string chroot;
    } arg;

    u8 *buf = nullptr;
};

} // namespace xld::wasm
