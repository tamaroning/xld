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
#include <functional>
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
#include <type_traits>
#include <vector>

using namespace oneapi;

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

// TODO: Use xxHash
/*
inline uint64_t hash_string(std::string_view str) {
    return XXH3_64bits(str.data(), str.size());
}

class HashCmp {
  public:
    static size_t hash(const std::string_view &k) { return hash_string(k); }

    static bool equal(const std::string_view &k1, const std::string_view &k2) {
        return k1 == k2;
    }
};

*/
