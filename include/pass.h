#pragma once
#include "xld.h"

namespace xld::wasm {

void resolve_symbols(Context &);

void create_internal_file(Context &);

void create_synthetic_sections(Context &);

u64 compute_section_sizes(Context &);

void copy_chunks(Context &);

void apply_reloc(Context &);

} // namespace xld::wasm
