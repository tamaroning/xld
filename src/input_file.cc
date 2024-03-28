#include "common/leb128.h"
#include "common/system.h"
#include "section.h"
#include "wao.h"
#include "wasm.h"
#include "xld.h"
#include <functional>

namespace xld::wasm {

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

// Can be used for variable-length elements
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

// Only can be used for fix-length elements
template <typename T>
std::vector<T> parse_vec(const u8 *data) {
    u64 num = decodeULEB128AndInc(data);
    std::vector<T> vec{data, data + sizeof(T) * num};
    return vec;
}

template <typename E>
std::vector<u64> parse_uleb128_vec(Context<E> &ctx, const u8 *data) {
    u64 num = decodeULEB128AndInc(data);
    std::vector<u64> v;
    for (int i = 0; i < num; i++) {
        v.push_back(decodeULEB128AndInc(data));
    }
    return v;
}

std::string parse_name(const u8 *data) {
    u64 num = decodeULEB128AndInc(data);
    std::string name{data, data + num};
    return name;
}

WasmLimits parse_limits(const u8 *data) {
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
    Warn(ctx) << "TODO: parse linking";
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
            for (auto &ft : this->func_types) {
                const u8 *data = ft.data();
                ASSERT(*data == 0x60 && "type section must start with 0x60");
                data++;
                std::vector<u8> param_types = parse_vec<u8>(data);
                std::vector<u8> result_types = parse_vec<u8>(data);
                std::cout << "param_types: ";
                for (auto &t : param_types) {
                    std::cout << (int)t << " ";
                }
                std::cout << "\n";
                std::cout << "result_types: ";
                for (auto &t : result_types) {
                    std::cout << (int)t << " ";
                }
                std::cout << "\n";
            }
        } break;
        }
    }
    Debug(ctx) << "=== Dump Ends ===";
}

using E = WASM32;

template class InputFile<E>;
template class ObjectFile<E>;

} // namespace xld::wasm
