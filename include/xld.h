#pragma once

#include "common/system.h"

#include "xld_private/chunk.h"
#include "xld_private/context.h"
#include "xld_private/input_file.h"
#include "xld_private/output_elem.h"
#include "xld_private/symbol.h"

namespace xld::wasm {

const u32 kPageSize = 65536;
const u32 kStackSize = kPageSize;
const u32 kStackAlign = 16;
const u32 kHeapAlign = 16;
const u32 kMinMemoryPages = 2;
const u32 kMaxMemoryPages = 2;

const std::string_view kDefaultMemoryName = "memory";

const std::string_view kDefaultFileName = "a.wasm";

int linker_main(int argc, char **argv);

} // namespace xld::wasm
