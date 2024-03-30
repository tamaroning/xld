#include "common/file.h"
#include "wasm.h"
#include "xld.h"

namespace xld::wasm {

template <typename E>
int wasm_main(int argc, char **argv) {
    Context<E> ctx;

    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];
        SyncOut(ctx) << "Open " << path_clean(path) << "\n";
        ObjectFile<E> *obj =
            ObjectFile<E>::create(ctx, must_open_file(ctx, path));
        obj->parse(ctx);
        ctx.objs.push_back(obj);
    }

    if (ctx.objs.empty())
        Fatal(ctx) << "no input files\n";

    // https://github.com/llvm/llvm-project/blob/95258419f6fe2e0922c2c0916fd176b9f7361555/lld/wasm/Driver.cpp#L1152C61-L1152C64

    // create internal file containing linker-synthesized symbols
    // if (target is not relocatable)
    create_internal_file(ctx);

    resolve_symbols(ctx);

    // compute_import_export(ctx);

    // Create linker-synthesized sections
    // create_synthetic_sections(ctx);

    // Make sure that there's no duplicate symbol
    // if (!ctx.arg.allow_multiple_definition)
    // check_duplicate_symbols(ctx);

    // Warn if symbols with different types are defined under the same name.
    // check_symbol_types(ctx);

    // Bin input sections into output sections.
    // create_output_sections(ctx);

    // Add synthetic symbols such as __ehdr_start or __end.
    // add_synthetic_symbols(ctx);

    // Compute sizes of output sections while assigning offsets
    // within an output section to input sections.
    // compute_section_sizes(ctx);

    // Sort sections by section attributes so that we'll have to
    // create as few segments as possible.
    // sort_output_sections(ctx);

    // Print reports about undefined symbols, if needed.
    // if (ctx.arg.unresolved_symbols == UNRESOLVED_ERROR)
    //    report_undef_errors(ctx);

    // Compute .symtab and .strtab sizes for each file.
    // create_output_symtab(ctx);

    // Compute the section header values for all sections.
    // compute_section_headers(ctx);

    // Set actual addresses to linker-synthesized symbols.
    // fix_synthetic_symbols(ctx);

    // At this point, both memory and file layouts are fixed.

    return 0;
}

template int wasm_main<WASM32>(int, char **);
// template int wasm_main<WASM64>(int, char **);

} // namespace xld::wasm
