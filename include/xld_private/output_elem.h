#pragma once

#include "common/integers.h"

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

} // namespace xld::wasm
