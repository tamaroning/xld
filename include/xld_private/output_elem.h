#pragma once

#include "common/integers.h"
#include "wasm/object.h"

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
    WasmDataSegment *seg;
    i32 virtualal_address;
};

} // namespace xld::wasm
