#pragma once

#include "common/common.h"
#include <iostream>
#include <mutex>
#include <sstream>

namespace xld {

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