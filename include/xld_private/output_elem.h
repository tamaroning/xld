#pragma once

#include "common/integers.h"
#include "wasm/object.h"
#include "xld_private/input_file.h"

namespace xld::wasm {

class ObjectFile;

template <typename E>
class OutputElem {
  public:
    explicit OutputElem(E wdata, ObjectFile *file) : wdata(wdata), file(file) {}

    E wdata;
    // TODO: remove?
    ObjectFile *file = nullptr;
    // signature/global/function/table index
    u64 index = 0xdeadbeaf;
};

class OutputSegment {
  public:
    i32 virtualal_address;
    InputFragment *ifrag;

    uint32_t init_flags;
    // Present if InitFlags & WASM_DATA_SEGMENT_HAS_MEMINDEX.
    uint32_t memory_index;
    // Present if InitFlags & WASM_DATA_SEGMENT_IS_PASSIVE == 0.
    WasmInitExpr offset;

    std::string name; // from the "segment info" section
    // from the "segmentinfo" subsection in the "linking" section
    uint32_t p2align;
    uint32_t linking_flags;
};

} // namespace xld::wasm
