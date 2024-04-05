#pragma once

#include "common/mmap.h"
#include "common/system.h"
#include "object/wao_basic.h"
#include "object/wao_symbol.h"

namespace xld::wasm {

class InputFile;
class ObjectFile;
class Symbol;
class InputSection;

} // namespace xld::wasm

namespace xld {

inline char *output_tmpfile;
inline thread_local bool opt_demangle;

inline void cleanup() {
    if (output_tmpfile)
        unlink(output_tmpfile);
}

//
// Error output
//

template <typename Context>
class SyncOut {
  public:
    SyncOut(Context &ctx, std::ostream *out = &std::cout) : out(out) {}

    ~SyncOut() {
        if (out) {
            std::scoped_lock lock(mu);
            *out << ss.str() << "\n";
        }
    }

    template <class T>
    SyncOut &operator<<(T &&val) {
        if (out)
            ss << std::forward<T>(val);
        return *this;
    }

    static inline std::mutex mu;

  private:
    std::ostream *out;
    std::stringstream ss;
};

template <typename Context>
class Debug {
  public:
    Debug(Context &ctx, std::ostream *out = &std::cout) : out(out) {}

    ~Debug() {
        if (out) {
            std::scoped_lock lock(mu);
            *out << "xld: \033[0;1;34mdebug\033[0m: " << ss.str() << "\n";
        }
    }

    template <class T>
    Debug &operator<<(T &&val) {
        if (out)
            ss << std::forward<T>(val);
        return *this;
    }

    static inline std::mutex mu;

  private:
    std::ostream *out;
    std::stringstream ss;
};

template <typename Context>
static std::string add_color(Context &ctx, std::string msg) {
    if (ctx.arg.color_diagnostics)
        return "xld: \033[0;1;31m" + msg + ":\033[0m ";
    return "xld: " + msg + ": ";
}

template <typename Context>
class Fatal {
  public:
    Fatal(Context &ctx) : out(ctx, &std::cerr) {
        out << add_color(ctx, "fatal");
    }

    [[noreturn]] ~Fatal() {
        out.~SyncOut();
        cleanup();
        _exit(1);
    }

    template <class T>
    Fatal &operator<<(T &&val) {
        out << std::forward<T>(val);
        return *this;
    }

  private:
    SyncOut<Context> out;
};

template <typename Context>
class Error {
  public:
    Error(Context &ctx) : out(ctx, &std::cerr) {
        out << add_color(ctx, "error");
        ctx.has_error = true;
    }

    template <class T>
    Error &operator<<(T &&val) {
        out << std::forward<T>(val);
        return *this;
    }

  private:
    SyncOut<Context> out;
};

template <typename Context>
class Warn {
  public:
    Warn(Context &ctx) : out(ctx, &std::cerr) {
        out << add_color(ctx, "warning");
    }

    template <class T>
    Warn &operator<<(T &&val) {
        out << std::forward<T>(val);
        return *this;
    }

  private:
    SyncOut<Context> out;
};

} // namespace xld

namespace xld::wasm {

int linker_main(int argc, char **argv);

struct Context {
    Context() {}

    Context(const Context &) = delete;

    void checkpoint() {
        if (has_error) {
            cleanup();
            _exit(1);
        }
    }

    bool has_error = false;

    // object pools
    tbb::concurrent_vector<std::unique_ptr<ObjectFile>> obj_pool;
    tbb::concurrent_vector<std::unique_ptr<MappedFile>> mf_pool;
    tbb::concurrent_vector<std::unique_ptr<u8[]>> string_pool;

    // Symbol table
    // TODO: use xxHash
    tbb::concurrent_hash_map<std::string_view, Symbol> symbol_map;

    // Input files
    std::vector<InputFile *> files;

    // Command-line arguments
    struct {
        bool color_diagnostics = true;
        std::string chroot;
    } arg;

    u8 *buf = nullptr;
};

class Symbol {
  public:
    Symbol(std::string_view name, InputFile *file) : name(name), file(file) {}

    // The name of the symbol.
    std::string_view name;

    // A symbol is owned (defined) by a file. If multiple files define the
    // symbol, the strongest binding is chosen.
    InputFile *file = nullptr;

    bool is_defined() const { return file != nullptr; }
    bool is_undefined() const { return file == nullptr; }

    // bool is_imported = false;
    // bool is_exported = false;

    bool is_used_in_regular_obj = false;

    enum class Binding {
        Weak,
        Global,
    } binding = Binding::Weak;
};

// If we haven't seen the same `name` before, create a new instance
// of Symbol and returns it. Otherwise, returns the previously-
// instantiated object.
inline Symbol *get_symbol(Context &ctx, std::string_view name) {
    typename decltype(ctx.symbol_map)::const_accessor acc;
    ctx.symbol_map.insert(acc, {name, Symbol(name, nullptr)});
    return const_cast<Symbol *>(&acc->second);
}

// pass.cc

void resolve_symbols(Context &);

void create_internal_file(Context &);

// input_file.cc parse_object.cc

class InputFile {
  public:
    // Only object file is supported
    enum Kind {
        Object,
    };

    InputFile(Context &ctx, const std::string &filename, MappedFile *mf);

    virtual ~InputFile() = default;

    // template <typename T>
    // std::span<T> get_data(Context &ctx, i64 idx);

    virtual void resolve_symbols(Context &ctx) = 0;
    // void clear_symbols();

    // This pointer can be nullptr for the internal files
    MappedFile *mf = nullptr;
    std::string filename;
    Kind kind = Object;

    std::vector<WasmSymbol> symbols;
};

class ObjectFile : public InputFile {
  public:
    static ObjectFile *create(Context &ctx, const std::string &filename,
                              MappedFile *mf = nullptr);

    bool is_defined_function(u32 index);
    WasmFunction &get_defined_function(u32 index);
    bool is_defined_global(u32 index);
    WasmGlobal &get_defined_global(u32 index);
    bool is_defined_memories(u32 index);
    WasmLimits &get_defined_memories(u32 index);

    bool is_valid_function_symbol(u32 Index);
    bool is_valid_table_symbol(u32 Index);
    bool is_valid_global_symbol(u32 Index);
    bool is_valid_tag_symbol(u32 Index);
    bool is_valid_data_symbol(u32 Index);
    bool is_valid_section_symbol(u32 Index);

    void parse(Context &ctx);
    // parse custom sections
    void parse_linking_sec(Context &ctx, const u8 *&p, const u32 size);
    void parse_reloc_sec(Context &ctx, const u8 *&p, const u32 size);
    void parse_name_sec(Context &ctx, const u8 *&p, const u32 size);
    // parse misc
    WasmInitExpr parse_init_expr(Context &ctx, const u8 *&data);

    void resolve_symbols(Context &ctx);

    void dump(Context &ctx);

    std::vector<std::unique_ptr<InputSection>> sections;

    std::vector<WasmSignature> signatures;

    std::vector<WasmImport> imports;

    std::vector<WasmFunction> functions;

    // table section

    std::vector<WasmLimits> memories;

    std::vector<WasmGlobal> globals;

    std::vector<WasmExport> exports;

    // start section

    std::vector<WasmElemSegment> elem_segments;

    std::vector<std::span<const u8>> codes;

    // linking section

    // reloc.* section
    // stored in InputSection

    // name section
    std::string module_name;

    // datacount section
    u32 data_count = 0;
    std::vector<WasmDataSegment> data_segments;

    WasmLinkingData linking_data;

    u32 num_imported_globals = 0;
    u32 num_imported_functions = 0;
    u32 num_imported_tables = 0;
    u32 num_imported_memories = 0;

  private:
    ObjectFile(Context &ctx, const std::string &filename, MappedFile *mf);
};

class InputSection {
  public:
    InputSection(u8 sec_id, std::span<const u8> content, InputFile *file,
                 u64 file_ofs, std::string name)
        : sec_id(sec_id), content(content), file(file), file_ofs(file_ofs),
          name(name) {}

    void write_to(Context &ctx, u8 *buf);

    u8 sec_id = 0;
    std::string name;
    std::span<const u8> content;
    InputFile *file;
    // offset to the secton content
    u64 file_ofs = 0;
    std::vector<WasmRelocation> relocs;
};

// output_file.cc

template <typename Context>
class OutputFile {
  public:
    static std::unique_ptr<OutputFile<Context>>
    open(Context &ctx, std::string path, i64 filesize, i64 perm);

    virtual void close(Context &ctx) = 0;
    virtual ~OutputFile() = default;

    u8 *buf = nullptr;
    std::vector<u8> buf2;
    std::string path;
    i64 fd = -1;
    i64 filesize = 0;
    bool is_mmapped = false;
    bool is_unmapped = false;

  protected:
    OutputFile(std::string path, i64 filesize, bool is_mmapped)
        : path(path), filesize(filesize), is_mmapped(is_mmapped) {}
};

// chunk.cc

class OutputSection;

class Chunk {
  public:
    virtual ~Chunk() = default;
    // virtual OutputSection *to_osec() { return nullptr; }
    virtual void copy_buf(Context &ctx) {}
    virtual void write_to(Context &ctx, u8 *buf) { unreachable(); }
    virtual void update_shdr(Context &ctx) {}

    std::string_view name;
};

class OutputWhdr : public Chunk {
  public:
    OutputWhdr() { this->name = "WHDR"; }

    void copy_buf(Context &ctx) override;
};

class OutputSection : public Chunk {
  public:
    OutputSection(std::string_view name) { this->name = name; }

    // OutputSection *to_osec() override { return this; }
    void copy_buf(Context &ctx) override;
    void write_to(Context &ctx, u8 *buf) override;

    std::vector<InputSection *> members;
};

} // namespace xld::wasm
