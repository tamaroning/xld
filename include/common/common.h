#pragma once

#include "common/integers.h"
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace xld {

inline char *output_tmpfile;

std::string errno_string();

template <typename Context>
std::string_view save_string(Context &ctx, const std::string &str) {
    u8 *buf = new u8[str.size() + 1];
    memcpy(buf, str.data(), str.size());
    buf[str.size()] = '\0';
    ctx.string_pool.push_back(std::unique_ptr<u8[]>(buf));
    return {(char *)buf, str.size()};
}

inline void cleanup() {
    if (output_tmpfile)
        unlink(output_tmpfile);
}

} // namespace xld
