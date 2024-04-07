#pragma once

#include "common/integers.h"

namespace xld::wasm {

template <typename E>
class OutputElem {
  public:
    E wdata;
    // signature/global/function/table index
    u64 index = 0xdeadbeaf;
};

} // namespace xld::wasm
