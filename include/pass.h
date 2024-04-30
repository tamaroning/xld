#pragma once
#include "xld.h"

namespace xld::wasm {

void create_internal_file(Context &);

void resolve_symbols(Context &);

void check_undefined(Context &);

void calculate_imports(Context &);

void create_synthetic_sections(Context &);

void add_definitions(Context &);

void assign_index(Context &);

void calculate_types(Context &);

void setup_indirect_functions(Context &);

void setup_memory(Context &);

u64 compute_section_sizes(Context &);

void copy_chunks(Context &);

void apply_reloc(Context &);

} // namespace xld::wasm
