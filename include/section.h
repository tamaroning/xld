#pragma once

#include "wao.h"
#include "xld.h"

namespace xld::wasm {

template <typename E>
class InputSection {
  public:
    InputSection(u8 sec_id, std::span<const u8> content, ObjectFile<E> *file,
                 u64 file_ofs, std::string_view name = "<unknown>")
        : sec_id(sec_id), content(content), file(file), file_ofs(file_ofs),
          name(name) {}

    u8 sec_id = 0;
    std::string_view name;
    std::span<const u8> content;
    ObjectFile<E> *file;
    // offset to the secton content
    u64 file_ofs = 0;
};

} // namespace xld::wasm
