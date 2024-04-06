#pragma once

#include "common/integers.h"
#include "common/system.h"

namespace xld::wasm {

class InputFile;
class ObjectFile;

struct InputFragment {
    std::span<const u8> data;
    wasm::InputFile *file;
    u64 file_ofs;
};

} // namespace xld::wasm
