#pragma once

#include "common/integers.h"
#include "common/system.h"
#include "wasm/object.h"

namespace xld::wasm {

// forward-decl
class Context;

class OutputLocation {
  public:
    OutputLocation() {}

    // The offset of the data in the output file.
    u64 offset = 0;
    // The size of the data in the output file.
    u64 size = 0;
    u64 content_size = 0;
};

class Chunk {
  public:
    Chunk() : name("<unknown>"), loc(OutputLocation()) {}
    virtual ~Chunk() = default;

    virtual u64 compute_section_size(Context &ctx) = 0;
    virtual void copy_buf(Context &ctx) = 0;

    u8 sec_id = 0;
    std::string_view name;
    OutputLocation loc;
};

class OutputWhdr : public Chunk {
  public:
    OutputWhdr() {
        name = "header";
        sec_id = 0xff;
    }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;
};

class TypeSection : public Chunk {
  public:
    TypeSection() {
        name = "type";
        sec_id = WASM_SEC_TYPE;
    }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;
};

class FunctionSection : public Chunk {
  public:
    FunctionSection() {
        name = "function";
        sec_id = WASM_SEC_FUNCTION;
    }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;
};

class GlobalSection : public Chunk {
  public:
    GlobalSection() {
        this->name = "global";
        sec_id = WASM_SEC_GLOBAL;
    }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;
};

class CodeSection : public Chunk {
  public:
    CodeSection() {
        this->name = "code";
        sec_id = WASM_SEC_CODE;
    }

    u64 compute_section_size(Context &ctx) override;
    void copy_buf(Context &ctx) override;
};

// https://github.com/WebAssembly/design/blob/main/BinaryEncoding.md#name-section
class NameSection : public Chunk {
  public:
    NameSection() {
        this->name = "name";
        sec_id = WASM_SEC_CUSTOM;
    }

    void copy_buf(Context &ctx) override;

    u64 compute_section_size(Context &ctx) override;

    u64 global_subsec_size = 0;
    u64 function_subsec_size = 0;
};

} // namespace xld::wasm