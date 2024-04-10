#pragma once

#include "common/common.h"
#include "common/mmap.h"
#include "common/system.h"
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

    // Symbol table
    // TODO: use xxHash
    tbb::concurrent_hash_map<std::string_view, Symbol> symbol_map;

    // Input files
    std::vector<InputFile *> files;

    // Output chunks
    std::vector<Chunk *> chunks;
    OutputWhdr *whdr = nullptr;
    TypeSection *type = nullptr;
    FunctionSection *function = nullptr;
    TableSection *table = nullptr;
    MemorySection *memory = nullptr;
    GlobalSection *global = nullptr;
    CodeSection *code = nullptr;
    ExportSection *export_ = nullptr;
    NameSection *name = nullptr;

    // output elements
    std::vector<OutputElem<WasmSignature>> signatures;
    std::vector<OutputElem<WasmFunction>> functions;
    std::vector<WasmExport> exports;
    std::vector<OutputElem<WasmGlobal>> globals;
    std::vector<InputSection> codes;

    // Command-line arguments
    struct {
        bool color_diagnostics = true;
        std::string chroot;
    } arg;

    u8 *buf = nullptr;
};

} // namespace xld::wasm
