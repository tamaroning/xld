#pragma once

#include "common/integers.h"
#include "common/system.h"
#include "wasm/object.h"

namespace xld::wasm {

// forward-decl
class Context;

class OutputLocation {
  public:
    OutputLocation(u32 offset, u32 size) : offset(offset), size(size) {}

    // The offset of the data in the output file.
    u64 offset;
    // The size of the data in the output file.
    u64 size;
};

class Chunk {
  public:
    Chunk() : name("<unknown>"), loc(OutputLocation(0, 0)) {}
    virtual ~Chunk() = default;

    virtual u64 compute_section_size(Context &ctx) = 0;
    virtual void copy_buf(Context &ctx) = 0;

    std::string_view name;
    OutputLocation loc;
};

class OutputWhdr : public Chunk {
  public:
    OutputWhdr() { this->name = "header"; }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;
};

class TypeSection : public Chunk {
  public:
    TypeSection() { this->name = "type"; }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;

    std::vector<WasmSignature *> signatures;
    std::vector<WasmFunction> functions;
};

/*
class FunctionSection : public Chunk {
  public:
    FunctionSection() { this->name = "function"; }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;
};
*/

class GlobalSection : public Chunk {
  public:
    GlobalSection() { this->name = "global"; }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;

    std::vector<WasmGlobal *> globals;
};

// https://github.com/WebAssembly/design/blob/main/BinaryEncoding.md#name-section
class NameSection : public Chunk {
  public:
    NameSection() { this->name = "name"; }

    void copy_buf(Context &ctx) override;

    u64 compute_section_size(Context &ctx) override;

    u64 global_subsec_size = 0;
};

} // namespace xld::wasm