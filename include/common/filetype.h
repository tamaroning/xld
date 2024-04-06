#pragma once

#include "common/integers.h"
#include "common/mmap.h"
#include "common/system.h"

namespace xld {

enum class FileType {
    UNKNOWN,
    EMPTY,
    WASM_OBJ,
    AR,
    THIN_AR,
    TEXT,
};

inline bool is_text_file(MappedFile *mf) {
    u8 *data = mf->data;
    return mf->size >= 4 && isprint(data[0]) && isprint(data[1]) &&
           isprint(data[2]) && isprint(data[3]);
}

template <typename Context>
FileType get_file_type(Context &ctx, MappedFile *mf) {
    std::string_view data = mf->get_contents();

    if (data.empty())
        return FileType::EMPTY;

    if (data[0] == 0x00 && data[1] == 0x61 && data[2] == 0x73 &&
        data[3] == 0x6D)
        return FileType::WASM_OBJ;
    if (data.starts_with("!<arch>\n"))
        return FileType::AR;
    if (data.starts_with("!<thin>\n"))
        return FileType::THIN_AR;
    if (is_text_file(mf))
        return FileType::TEXT;
    return FileType::UNKNOWN;
}

inline std::string filetype_to_string(FileType type) {
    switch (type) {
    case FileType::UNKNOWN:
        return "UNKNOWN";
    case FileType::EMPTY:
        return "EMPTY";
    case FileType::WASM_OBJ:
        return "WASM_OBJ";
    case FileType::AR:
        return "AR";
    case FileType::THIN_AR:
        return "THIN_AR";
    case FileType::TEXT:
        return "TEXT";
    }
    return "UNKNOWN";
}

inline std::ostream &operator<<(std::ostream &out, FileType type) {
    out << filetype_to_string(type);
    return out;
}

} // namespace xld
