#pragma once

#include "common/integers.h"
#include <span>

namespace xld::wasm {

class InputFile;

struct Span {
    std::span<const u8> data;
    wasm::InputFile *file;
    u64 file_ofs;
};

} // namespace xld::wasm
