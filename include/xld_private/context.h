#pragma once

#include "common/common.h"
#include "common/mmap.h"
#include "common/system.h"

namespace xld::wasm {

// forward-decl
class InputFile;
class ObjectFile;
class Symbol;
class Chunk;

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

    // Command-line arguments
    struct {
        bool color_diagnostics = true;
        std::string chroot;
    } arg;

    u8 *buf = nullptr;
};

} // namespace xld::wasm
