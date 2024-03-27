#pragma once

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
#include <oneapi/tbb.h>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

using namespace oneapi;

#include <sys/mman.h>
#include <unistd.h>

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
