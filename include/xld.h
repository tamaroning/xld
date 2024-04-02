#pragma once

#include "common/mmap.h"
#include "common/system.h"
#include "wasm.h"

namespace xld::wasm {

template <typename E>
class InputFile;

template <typename E>
class ObjectFile;

template <typename E>
class Symbol;

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

struct WASM32;
struct WASM64;

template <typename E>
int linker_main(int argc, char **argv);

template <typename E>
struct Context {
    Context() {}

    Context(const Context<E> &) = delete;

    void checkpoint() {
        if (has_error) {
            cleanup();
            _exit(1);
        }
    }

    bool has_error = false;

    tbb::concurrent_vector<std::unique_ptr<ObjectFile<E>>> obj_pool;
    tbb::concurrent_vector<std::unique_ptr<MappedFile>> mf_pool;

    // Symbol table
    // TODO: use xxHash
    tbb::concurrent_hash_map<std::string_view, Symbol<E>> symbol_map;

    // Input files
    std::vector<InputFile<E> *> files;

    // Command-line arguments
    struct {
        bool color_diagnostics = true;
        std::string chroot;
    } arg;
};

// pass.cc
template <typename E>
void resolve_symbols(Context<E> &);

template <typename E>
void create_internal_file(Context<E> &);

} // namespace xld::wasm
