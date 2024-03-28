#include "common/integers.h"
#include "common/leb128.h"
#include "common/system.h"
#include "section.h"
#include "wao.h"
#include "wasm.h"
#include "xld.h"
#include <functional>
#include <optional>

namespace xld::wasm {

// If we haven't seen the same `key` before, create a new instance
// of Symbol and returns it. Otherwise, returns the previously-
// instantiated object. `key` is usually the same as `name`.
template <typename E>
Symbol<E> *get_symbol(Context<E> &ctx, std::string_view key,
                      std::string_view name) {
    typename decltype(ctx.symbol_map)::const_accessor acc;
    ctx.symbol_map.insert(acc, {key, Symbol<E>(name)});
    return const_cast<Symbol<E> *>(&acc->second);
}

template <typename E>
InputFile<E>::InputFile(Context<E> &ctx, MappedFile *mf)
    : mf(mf), filename(mf->name) {

    if (mf->size < sizeof(WasmObjectHeader))
        Fatal(ctx) << filename << ": file too small\n";

    const WasmObjectHeader *ohdr =
        reinterpret_cast<WasmObjectHeader *>(mf->data);
    if (!(ohdr->magic[0] == WASM_MAGIC[0] && ohdr->magic[1] == WASM_MAGIC[1] &&
          ohdr->magic[2] == WASM_MAGIC[2] && ohdr->magic[3] == WASM_MAGIC[3]))
        Fatal(ctx) << filename << ": bad magic\n";

    // Should check version?
}

template <typename E>
ObjectFile<E>::ObjectFile(Context<E> &ctx, MappedFile *mf)
    : InputFile<E>(ctx, mf) {}

template <typename E>
ObjectFile<E> *ObjectFile<E>::create(Context<E> &ctx, MappedFile *mf) {
    ObjectFile<E> *obj = new ObjectFile<E>(ctx, mf);
    ctx.obj_pool.push_back(std::unique_ptr<ObjectFile<E>>(obj));
    return obj;
}

// Parse vector of variable-length element and increment pointer
// f needs to increment pointer
template <typename T>
std::vector<T> parse_vec_varlen(const u8 *data,
                                std::function<T(const u8 *)> f) {
    u64 num = decodeULEB128AndInc(data);
    std::vector<T> v;
    for (int i = 0; i < num; i++) {
        v.push_back(f(data));
    }
    return v;
}

// Parse vector of fix-length element and increment pointer
template <typename T>
std::vector<T> parse_vec(const u8 *&data) {
    u64 num = decodeULEB128AndInc(data);
    std::vector<T> vec{data, data + sizeof(T) * num};
    data += sizeof(T) * num;
    return vec;
}

template <typename E>
std::vector<u64> parse_uleb128_vec(Context<E> &ctx, const u8 *&data) {
    u64 num = decodeULEB128AndInc(data);
    std::vector<u64> v;
    for (int i = 0; i < num; i++) {
        v.push_back(decodeULEB128AndInc(data));
    }
    return v;
}

// Parse name and increment pointer
std::string parse_name(const u8 *&data) {
    u64 num = decodeULEB128AndInc(data);
    std::string name{data, data + num};
    data += num;
    return name;
}

// Parse limits and increment pointer
WasmLimits parse_limits(const u8 *&data) {
    const u8 flags = *data;
    data++;
    WasmLimits limits;
    if (flags & WASM_LIMITS_FLAG_HAS_MAX) {
        u64 min = decodeULEB128AndInc(data);
        u64 max = decodeULEB128AndInc(data);
        limits = WasmLimits{flags, min, max};
    } else {
        u64 min = decodeULEB128AndInc(data);
        limits = WasmLimits{flags, min, 0};
    }
    return limits;
}

std::string_view sec_id_as_str(u8 sec_id) {
    switch (sec_id) {
    case WASM_SEC_CUSTOM:
        return "custom";
    case WASM_SEC_TYPE:
        return "type";
    case WASM_SEC_IMPORT:
        return "import";
    case WASM_SEC_FUNCTION:
        return "function";
    case WASM_SEC_TABLE:
        return "table";
    case WASM_SEC_MEMORY:
        return "memory";
    case WASM_SEC_GLOBAL:
        return "global";
    case WASM_SEC_EXPORT:
        return "export";
    case WASM_SEC_START:
        return "start";
    case WASM_SEC_ELEM:
        return "elem";
    case WASM_SEC_CODE:
        return "code";
    case WASM_SEC_DATA:
        return "data";
    case WASM_SEC_DATACOUNT:
        return "datacount";
    case WASM_SEC_TAG:
        return "tag";
    default:
        return "<unknown>";
    }
}

template <typename E>
void ObjectFile<E>::parse_linking_sec(Context<E> &ctx,
                                      std::span<const u8> bytes) {
    const u8 *p = bytes.data();
    u64 version = decodeULEB128AndInc(p);
    if (version != 2)
        Fatal(ctx) << "linking version must be 2";

    while (p < bytes.data() + bytes.size()) {
        u8 type = *p;
        p++;
        u64 payload_len = decodeULEB128AndInc(p);
        Debug(ctx) << "linking entry type: " << (int)type;

        switch (type) {
        case WASM_SYMBOL_TABLE: {
            u64 count = decodeULEB128AndInc(p);
            Debug(ctx) << "symbol table count: " << count;
            for (int j = 0; j < count; j++) {
                WasmSymbolType type{*p};
                p++;
                u32 flags = decodeULEB128AndInc(p);

                // FIXME: broken pointer

                WasmSymbolInfo info;
                if (flags & WASM_SYMBOL_UNDEFINED) {
                    // this symbol references import
                    // index of import object
                    u32 index = decodeULEB128AndInc(p);
                    std::string name = parse_name(p);
                    info = WasmSymbolInfo{name,
                                          type,
                                          flags,
                                          std::nullopt,
                                          std::nullopt,
                                          std::nullopt,
                                          {.element_index = index}};
                } else if (type == WASM_SYMBOL_TYPE_DATA) {
                    std::string name = parse_name(p);
                    // index of segment
                    u32 segment = decodeULEB128AndInc(p);
                    u32 offset = decodeULEB128AndInc(p);
                    u32 size = decodeULEB128AndInc(p);
                    info =
                        WasmSymbolInfo{name,
                                       type,
                                       flags,
                                       std::nullopt,
                                       std::nullopt,
                                       std::nullopt,
                                       {.data_ref = {segment, offset, size}}};
                } else if (type == WASM_SYMBOL_TYPE_SECTION) {
                    u32 section = decodeULEB128AndInc(p);
                    Error(ctx) << "TODO: parse section symbol table";
                    return;
                } else {
                    Error(ctx)
                        << "unknown symbol type: " << (int)type << ", ignored";
                }
                Debug(ctx) << "push symbol: " << info.name;
                this->symbols.push_back(Symbol(info, this));
            }
        } break;
        default: {
            Error(ctx) << "unknown linking entry type: " << (int)type;
            return;
        }
        }
    }
}

template <typename E>
void ObjectFile<E>::parse_reloc_sec(Context<E> &ctx,
                                    std::span<const u8> bytes) {
    Warn(ctx) << "TODO: parse reloc";
}

template <typename E>
void ObjectFile<E>::parse(Context<E> &ctx) {
    u8 *data = this->mf->data;
    const u8 *p = data + sizeof(WasmObjectHeader);

    while (p - data < this->mf->size) {
        u8 sec_id = *p;
        p++;
        u64 content_size = decodeULEB128AndInc(p);

        std::string name{"<unknown>"};
        const u8 *content_beg = p;
        std::span<const u8> content{content_beg, content_beg + content_size};
        u64 content_ofs = p - data;

        switch (sec_id) {
        case WASM_SEC_CUSTOM: {
            const u8 *cont_begin = p;
            name = parse_name(p);
            // parse byte:
            u64 byte_size = content_size - (p - content_beg);
            std::span<const u8> bytes{p, p + byte_size};
            // TODO: use result
            if (name == "linking") {
                parse_linking_sec(ctx, bytes);
            } else if (name.starts_with("reloc")) {
                parse_reloc_sec(ctx, bytes);
            } else {
                Warn(ctx) << "custom section: " << name << " ignored";
            }
        } break;
        case WASM_SEC_TYPE: {
            std::function<std::span<const u8>(const u8 *)> f =
                [](const u8 *data) {
                    const u8 *type_begin = data;
                    ASSERT(*data == 0x60 &&
                           "type section must start with 0x60");
                    data++;
                    // discard results
                    std::vector<u8> param_types = parse_vec<u8>(data);
                    std::vector<u8> result_types = parse_vec<u8>(data);
                    return std::span<const u8>(type_begin, data);
                };
            this->func_types = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_IMPORT: {
            std::function<WasmImport(const u8 *)> f = [](const u8 *data) {
                std::string module = parse_name(data);
                std::string field = parse_name(data);
                u8 kind = *data;
                data++;
                switch (kind) {
                case WASM_EXTERNAL_FUNCTION: {
                    u32 sig_index = decodeULEB128AndInc(data);
                    return WasmImport{
                        module, field, kind, {.sig_index = sig_index}};
                } break;
                case WASM_EXTERNAL_TABLE: {
                    // parse reftype
                    ValType reftype{*data};
                    data++;
                    // parse limits
                    const u8 flags = *data;
                    data++;
                    WasmLimits limits = parse_limits(data);
                    WasmTableType table{reftype, limits};
                    return WasmImport{module, field, kind, {.table = table}};
                } break;
                case WASM_EXTERNAL_MEMORY: {
                    WasmLimits limits = parse_limits(data);
                    return WasmImport{module, field, kind, {.memory = limits}};
                } break;
                case WASM_EXTERNAL_GLOBAL: {
                    const ValType val_type{*data};
                    data++;
                    const bool mut = *data;
                    data++;
                    WasmGlobalType global{val_type, mut};
                    return WasmImport{module, field, kind, {.global = global}};
                } break;
                default:
                    ASSERT(0 && "unknown import kind");
                    unreachable();
                }
            };
            this->imports = parse_vec_varlen(data, f);
        } break;
        case WASM_SEC_FUNCTION: {
            this->func_sec_type_indices = parse_uleb128_vec(ctx, p);
        } break;
        case WASM_SEC_CODE: {
            std::function<std::span<const u8>(const u8 *)> f =
                [](const u8 *data) {
                    const u8 *code_start = data;
                    u64 size = decodeULEB128AndInc(data);
                    std::span<const u8> code{code_start, data + size};
                    return code;
                };
            this->codes = parse_vec_varlen(p, f);
        } break;
        default:
            Fatal(ctx) << "section: " << sec_id_as_str(sec_id) << "(" << name
                       << ") ignored";

            break;
        }
        this->sections.push_back(std::unique_ptr<InputSection<E>>(
            new InputSection(sec_id, content, this, content_ofs, name)));
        p = content_beg + content_size;
    }

    this->dump(ctx);
}

template <typename E>
void ObjectFile<E>::dump(Context<E> &ctx) {
    Debug(ctx) << "=== Dump Starts ===";
    for (auto &sec : this->sections) {
        Debug(ctx) << std::hex << "Section: " << sec_id_as_str(sec->sec_id)
                   << "(name=" << sec->name << ", offset=0x" << sec->file_ofs
                   << ", size=0x" << sec->content.size() << ")";
        switch (sec->sec_id) {
        case WASM_SEC_TYPE: {
            for (int i = 0; i < this->func_types.size(); i++) {
                Debug(ctx) << "  - type[" << i << "]";
            }
        } break;
        case WASM_SEC_IMPORT: {
            for (int i = 0; i < this->imports.size(); i++) {
                Debug(ctx) << "  - import[" << i << "]";
            }
        } break;
        case WASM_SEC_CUSTOM: {
            if (sec->name == "linking") {
                for (int i = 0; i < this->symbols.size(); i++) {
                    auto sym = this->symbols[i];
                    Debug(ctx) << "  - symbol[" << i << "]: " << sym.info.name;
                }
            }
        }
        }
    }
    Debug(ctx) << "=== Dump Ends ===";
}

using E = WASM32;

template class InputFile<E>;
template class ObjectFile<E>;

} // namespace xld::wasm
