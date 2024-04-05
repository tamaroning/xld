#pragma once

#include "common.h"
#include "common/mmap.h"
#include "xld.h"
#include <filesystem>
#include <sys/stat.h>

namespace xld {

MappedFile *open_file_impl(const std::string &path, std::string &error);

template <typename T>
std::filesystem::path filepath(const T &path) {
    return {path, std::filesystem::path::format::generic_format};
}

// Removes redundant '/..' or '/.' from a given path.
// The transformation is done purely by lexical processing.
// This function does not access file system.
inline std::string path_clean(std::string_view path) {
    return filepath(path).lexically_normal().string();
}

template <typename Context>
MappedFile *open_file(Context &ctx, std::string path) {
    if (path.starts_with('/') && !ctx.arg.chroot.empty())
        path = ctx.arg.chroot + "/" + path_clean(path);

    std::string error;
    MappedFile *mf = open_file_impl(path, error);
    if (!error.empty())
        Fatal(ctx) << error;

    if (mf)
        ctx.mf_pool.push_back(std::unique_ptr<MappedFile>(mf));
    return mf;
}

template <typename Context>
MappedFile *must_open_file(Context &ctx, std::string path) {
    MappedFile *mf = open_file(ctx, path);
    if (!mf)
        Fatal(ctx) << "cannot open " << path << ": " << errno_string();
    return mf;
}

} // namespace xld