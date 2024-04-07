#pragma once

#include "common/integers.h"

namespace xld::wasm {

template <typename E>
class OutputElem {
  public:
    explicit OutputElem(E wdata) : wdata(wdata) {}

    E wdata;
    // signature/global/function/table index
    u64 index = 0xdeadbeaf;
};

} // namespace xld::wasm
