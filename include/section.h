#pragma once

#include "xld.h"

namespace xld::wasm {

template <typename E>
class InputSection {
  public:
    InputSection(u8 sec_id, const u8 *cont, u64 size, ObjectFile<E> *file,
                 u64 file_ofs, std::string_view name = "<unknown>")
        : sec_id(sec_id), cont(cont), size(size), file(file),
          file_ofs(file_ofs), name(name) {}

    u8 sec_id = 0;
    std::string_view name;
    // pointer to the content
    const u8 *cont = nullptr;
    // size of the content
    u64 size = 0;
    ObjectFile<E> *file;
    // offset to the secton content
    u64 file_ofs = 0;
};

} // namespace xld::wasm
