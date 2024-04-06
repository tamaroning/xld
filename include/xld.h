#pragma once

#include "xld_private/context.h"
#include "xld_private/input_file.h"
#include "xld_private/input_span.h"
#include "xld_private/object_file.h"
#include "xld_private/output_chunk.h"
#include "xld_private/symbol.h"
#include "xld_private/wasm_object.h"
#include "xld_private/wasm_symbol.h"

namespace xld::wasm {

int linker_main(int argc, char **argv);

} // namespace xld::wasm
