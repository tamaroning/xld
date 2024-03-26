#pragma once

// Copy from mold

#include <array>
#include <atomic>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
//#include <tbb/concurrent_vector.h>
//#include <tbb/enumerable_thread_specific.h>
//#include <tbb/parallel_for.h>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef NDEBUG
#define unreachable() __builtin_unreachable()
#else
#define unreachable() assert(0 && "unreachable")
#endif

// __builtin_assume() is supported only by clang, and [[assume]] is
// available only in C++23, so we use this macro when giving a hint to
// the compiler's optimizer what's true.
#define ASSUME(x)                                                              \
    do {                                                                       \
        if (!(x))                                                              \
            __builtin_unreachable();                                           \
    } while (0)

// This is an assert() that is enabled even in the release build.
#define ASSERT(x)                                                              \
    do {                                                                       \
        if (!(x)) {                                                            \
            std::cerr << "Assertion failed: (" << #x << "), function "         \
                      << __FUNCTION__ << ", file " << __FILE__ << ", line "    \
                      << __LINE__ << ".\n";                                    \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

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
    SyncOut(Context &ctx, std::ostream *out = &std::cout) : out(out) {
        opt_demangle = ctx.arg.demangle;
    }

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
static std::string add_color(Context &ctx, std::string msg) {
    if (ctx.arg.color_diagnostics)
        return "mold: \033[0;1;31m" + msg + ":\033[0m ";
    return "mold: " + msg + ": ";
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
        if (ctx.arg.noinhibit_exec) {
            out << add_color(ctx, "warning");
        } else {
            out << add_color(ctx, "error");
            ctx.has_error = true;
        }
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
    Warn(Context &ctx)
        : out(ctx, ctx.arg.suppress_warnings ? nullptr : &std::cerr) {
        if (ctx.arg.fatal_warnings) {
            out << add_color(ctx, "error");
            ctx.has_error = true;
        } else {
            out << add_color(ctx, "warning");
        }
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
int wasm_main(int argc, char **argv);

} // namespace xld::wasm
