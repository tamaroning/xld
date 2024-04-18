#pragma once
#include "xld.h"
#include "xld_private/chunk.h"

namespace xld::wasm {

void create_internal_file(Context &);

void resolve_symbols(Context &);

void calculate_imports(Context &);

void create_synthetic_sections(Context &);

void assign_index(Context &);

void calculate_types(Context &);

u64 compute_section_sizes(Context &);

void copy_chunks(Context &);

void apply_reloc(Context &);

} // namespace xld::wasm
