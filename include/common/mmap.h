#pragma once

#include "common/integers.h"
#include <memory>
#include <string>
#include <sys/mman.h>

namespace xld {

class MappedFile {
  public:
    ~MappedFile() { unmap(); }
    void unmap() {
        if (size == 0 || !data)
            return;

        munmap(data, size);
        data = nullptr;
    }

    template <typename Context>
    MappedFile *slice(Context &ctx, std::string name, u64 start, u64 size) {
        MappedFile *mf = new MappedFile;
        mf->name = name;
        mf->data = data + start;
        mf->size = size;

        ctx.mf_pool.push_back(std::unique_ptr<MappedFile>(mf));
        return mf;
    }

    std::string_view get_contents() {
        return std::string_view((char *)data, size);
    }

    std::string name;
    u8 *data = nullptr;
    i64 size = 0;

    MappedFile *thin_parent = nullptr;

    int fd = -1;
};

} // namespace xld