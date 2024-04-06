#pragma once

#include "context.h"
#include "wasm_object.h"

namespace xld::wasm {

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

    // virtual OutputSection *to_osec() { return nullptr; }
    virtual void copy_buf(Context &ctx) = 0;
    // virtual void write_to(Context &ctx, u8 *buf) { unreachable(); }
    // virtual u64 calculate_size(Context &ctx) = 0;

    std::string_view name;
    OutputLocation loc;
};

class OutputWhdr : public Chunk {
  public:
    OutputWhdr() { this->name = "WHDR"; }

    void copy_buf(Context &ctx) override;
    /*
    u64 calculate_size(Context &ctx) override {
        return sizeof(WasmObjectHeader);
    }*/
};

class GlobalSection : public Chunk {
  public:
    GlobalSection() { this->name = "GLOBAL"; }

    void copy_buf(Context &ctx) override;
    // u64 calculate_size(Context &ctx) override { return 0; }

    std::vector<WasmGlobal *> globals;
};

} // namespace xld::wasm