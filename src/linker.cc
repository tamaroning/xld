#include "common/archive_file.h"
#include "common/file.h"
#include "common/filetype.h"
#include "common/mmap.h"
#include "common/output_file.h"
#include "pass.h"
#include "xld.h"

namespace xld::wasm {

int linker_main(int argc, char **argv) {
    Context ctx;

    i64 thread_count = get_default_thread_count();
    Debug(ctx) << "thread_count: " << thread_count;
    tbb::global_control tbb_cont(tbb::global_control::max_allowed_parallelism,
                                 thread_count);

    // read files
    std::vector<std::string> input_files;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--export-all") {
            ctx.arg.export_all = true;
        } else if (arg == "--allow-undefined") {
            ctx.arg.allow_undefined = true;
        } else if (arg == "-o") {
            if (i + 1 >= argc)
                Fatal(ctx) << "no output file";
            ctx.arg.output_file = argv[++i];
        } else if (arg == "--dump-input") {
            ctx.arg.dump_input = true;
        } else {
            std::string path = path_clean(argv[i]);
            input_files.push_back(path);
        }
    }

    if (input_files.empty())
        Fatal(ctx) << "no input files";

    tbb::concurrent_vector<ObjectFile *> objs;
    tbb::parallel_for_each(input_files, [&](auto &path) {
        MappedFile *mf = must_open_file(ctx, path);
        SyncOut(ctx) << "Open " << path << " (" << get_file_type(ctx, mf)
                     << ")";
        switch (get_file_type(ctx, mf)) {
        case FileType::WASM_OBJ: {
            ObjectFile *obj = ObjectFile::create(ctx, path, mf);
            obj->parse(ctx);
            objs.push_back(obj);
        } break;
        case FileType::AR: {
            for (MappedFile *f : read_archive_members(ctx, mf)) {
                ObjectFile *obj = ObjectFile::create(ctx, f->name, f);
                obj->parse(ctx);
                objs.push_back(obj);
            }
        } break;
        case FileType::THIN_AR: {
            for (MappedFile *f : read_thin_archive_members(ctx, mf)) {
                ObjectFile *obj = ObjectFile::create(ctx, f->name, f);
                obj->parse(ctx);
                objs.push_back(obj);
            }
        } break;
        default:
            Fatal(ctx) << "unknown file type: " << path;
            break;
        }
    });

    for (ObjectFile *obj : objs) {
        if (ctx.arg.dump_input)
            obj->dump(ctx);

        ctx.files.push_back(obj);
    }

    ctx.checkpoint();

    // https://github.com/llvm/llvm-project/blob/95258419f6fe2e0922c2c0916fd176b9f7361555/lld/wasm/Driver.cpp#L1152C61-L1152C64

    // create internal file containing linker-synthesized symbols
    // if (target is not relocatable)
    create_internal_file(ctx);

    // - Determine the set of object files to extract from archives.
    // - Remove redundant COMDAT sections (e.g. duplicate inline functions).
    // - Finally, the actual symbol resolution.
    // - LTO, which requires preliminary symbol resolution before running
    //   and a follow-up re-resolution after the LTO objects are emitted.
    resolve_symbols(ctx);

    check_undefined(ctx);

    calculate_imports(ctx);

    ctx.checkpoint();

    // compute_import_export(ctx);

    // Create linker-synthesized sections
    create_synthetic_sections(ctx);

    assign_index(ctx);

    calculate_types(ctx);

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

    setup_memory(ctx);

    // At this point, both memory and file layouts are fixed.

    // Compute sizes of output sections while assigning offsets
    // within an output section to input sections.
    u64 size = compute_section_sizes(ctx);

    // https://github.com/tamaroning/mold/blob/3df7c8e89c507865abe0fad4ff5355f4d328f78d/elf/main.cc#L637
    std::string filename{kDefaultFileName};
    if (!ctx.arg.output_file.empty())
        filename = ctx.arg.output_file;
    auto output_file = OutputFile<Context>::open(ctx, filename, size, 0777);
    ctx.buf = output_file->buf;

    copy_chunks(ctx);
    apply_reloc(ctx);

    output_file->close(ctx);

    Debug(ctx) << "Write to " << filename;

    return 0;
}

} // namespace xld::wasm
