#include "common/integers.h"
#include "common/leb128.h"
#include "common/system.h"
#include "object/wao_symbol.h"
#include "xld.h"

namespace xld::wasm {

// Parse vector of variable-length element and increment pointer
// f needs to increment pointer
template <typename T>
std::vector<T> parse_vec_varlen(const u8 *&data,
                                std::function<T(const u8 *&)> f) {
    u32 num = decodeULEB128AndInc(data);
    std::vector<T> v;
    for (int i = 0; i < num; i++) {
        v.push_back(f(data));
    }
    return v;
}

// Parse vector of fix-length element and increment pointer
template <typename T>
std::vector<T> parse_vec(const u8 *&data) {
    u32 num = decodeULEB128AndInc(data);
    std::vector<T> vec;
    if constexpr (std::is_same<T, u8>::value) {
        vec = std::vector<T>{data, data + sizeof(T) * num};
    } else {
        const T *p = reinterpret_cast<const T *>(data);
        vec = std::vector<T>{p, p + sizeof(T) * num};
    }
    data += sizeof(T) * num;
    return vec;
}

std::vector<u32> parse_uleb128_vec(Context &ctx, const u8 *&data) {
    u32 num = decodeULEB128AndInc(data);
    std::vector<u32> v;
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

void ObjectFile::parse_linking_sec(Context &ctx, const u8 *&p, const u32 size) {
    const u8 *const beg = p;
    u32 version = decodeULEB128AndInc(p);
    if (version != 2)
        Warn(ctx) << "linking version must be 2";

    while (p < beg + size) {
        u8 type = *p;
        p++;
        // TODO: use result
        u32 payload_len = decodeULEB128AndInc(p);

        switch (type) {
        case WASM_SYMBOL_TABLE: {
            u64 count = decodeULEB128AndInc(p);
            for (int j = 0; j < count; j++) {
                WasmSymbolType type{*p};
                p++;
                u32 flags = decodeULEB128AndInc(p);

                WasmSymbolInfo info;
                WasmGlobalType *global_type = nullptr;
                WasmTableType *table_type = nullptr;
                WasmSignature *signature = nullptr;
                switch (type) {
                case WASM_SYMBOL_TYPE_FUNCTION:
                case WASM_SYMBOL_TYPE_TABLE:
                case WASM_SYMBOL_TYPE_GLOBAL: {
                    bool index_references_import =
                        (flags & WASM_SYMBOL_UNDEFINED) &&
                        !(flags & WASM_SYMBOL_EXPLICIT_NAME);
                    u32 index = decodeULEB128AndInc(p);
                    std::string name;
                    std::optional<std::string> import_module;
                    std::optional<std::string> import_name;

                    if (index_references_import) {
                        // name is taken from the import
                        // linear search in all imports
                        int current_index = 0;
                        for (int k = 0;; k++) {
                            if (k >= this->imports.size())
                                Error(ctx) << "index=" << index
                                           << " in syminfo is corrupsed";
                            WasmImport imp = this->imports[k];
                            if (import_kind_eq_symtype(imp.kind, type)) {
                                if (current_index == index) {
                                    import_module = imp.module;
                                    import_name = imp.field;
                                    name = imp.module + '.' + imp.field;
                                    break;
                                }
                                current_index++;
                            }
                        }
                        if (!import_module.has_value())
                            Error(ctx) << "Corrupsed index=" << index;
                    } else {
                        name = parse_name(p);
                    }
                    info = WasmSymbolInfo{.name = name,
                                          .kind = type,
                                          .flags = flags,
                                          .import_module = import_module,
                                          .import_name = import_name,
                                          .export_name = std::nullopt,
                                          .value = {.element_index = index}};

                    // if the symbol is defined in this module, get reference to
                    // it.
                    if (!(info.flags & wasm::WASM_SYMBOL_UNDEFINED)) {
                        switch (type) {
                        case WASM_SYMBOL_TYPE_FUNCTION: {
                            if (index >= this->functions.size())
                                Error(ctx) << "function index=" << index
                                           << " is out of range";
                            signature = &this->signatures[this->functions[index]
                                                              .sig_index];
                        } break;
                        case WASM_SYMBOL_TYPE_TABLE: {
                            /*
                            if (index >= this->tables.size())
                                Fatal(ctx) << "table index=" << index
                                           << " is out of range";
                            table_type = &this->tables[index];
                            */
                            Fatal(ctx) << "TODO: handle table in symbol table";
                        } break;
                        case WASM_SYMBOL_TYPE_GLOBAL: {
                            if (index >= this->globals.size())
                                Fatal(ctx) << "global index=" << index
                                           << " is out of range";
                            global_type = &this->globals[index].type;
                        } break;
                        default:
                            unreachable();
                        }
                    }
                } break;
                case WASM_SYMBOL_TYPE_DATA: {
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
                } break;
                default: {
                    Error(ctx)
                        << "unknown symbol type: " << (int)type << ", ignored";
                }
                }
                WasmSymbol sym{info, nullptr, nullptr, nullptr};
                this->symbols.push_back(sym);
            }
        } break;
        default: {
            Error(ctx) << "unknown linking entry type: " << (int)type;
            return;
        }
        }
    }
}

void ObjectFile::parse_reloc_sec(Context &ctx, const u8 *&p, const u32 size) {
    u32 sec_idx = decodeULEB128AndInc(p);
    if (sec_idx >= this->sections.size())
        Fatal(ctx) << "section index in WasmRelocation is corrupsed";
    u32 count = decodeULEB128AndInc(p);
    for (int i = 0; i < count; i++) {
        u8 type = *p;
        p++;
        u32 offset = decodeULEB128AndInc(p);
        u32 index = decodeULEB128AndInc(p);
        this->sections[sec_idx]->relocs.push_back(WasmRelocation{
            type,
            offset,
            index,
        });
    }
}

void ObjectFile::parse_name_sec(Context &ctx, const u8 *&p, const u32 size) {
    const u8 *const beg = p;
    for (int i = 0; i < 3; i++) {
        if (p >= beg + size)
            break;

        u8 subsec_kind = *p;
        p++;
        u32 subsec_size = decodeULEB128AndInc(p);

        switch (subsec_kind) {
        case WASM_NAMES_MODULE: {
            // parse a single name
            this->module_name = parse_name(p);
        } break;
        case WASM_NAMES_FUNCTION: {
            // indeirectnamemap
            u32 count = decodeULEB128AndInc(p);
            for (int j = 0; j < count; j++) {
                u32 func_index = decodeULEB128AndInc(p);
                std::string name = parse_name(p);
                if (func_index >= this->functions.size())
                    Fatal(ctx)
                        << "func_index=" << func_index << " is out of range";

                this->functions[func_index].name = name;
            }
        } break;
        case WASM_NAMES_LOCAL: {
            // indirectnamemap
            u32 count = decodeULEB128AndInc(p);
            for (int j = 0; j < count; j++) {
                u32 local_index = decodeULEB128AndInc(p);
                std::string name = parse_name(p);
                // TODO:
                Debug(ctx) << "local[" << local_index << "]=" << name;
            }
        } break;
        case WASM_NAMES_GLOBAL: {
            // indirectnamemap
            u32 count = decodeULEB128AndInc(p);
            for (int j = 0; j < count; j++) {
                u32 global_index = decodeULEB128AndInc(p);
                std::string name = parse_name(p);
                if (global_index >= this->globals.size())
                    Fatal(ctx) << "global_index=" << global_index
                               << " is out of range";

                this->globals[global_index].symbol_name = name;
            }
        } break;
        default:
            Fatal(ctx) << "unknown name subsec kind: " << (int)subsec_kind;
        }
    }
}

// parse initExpr
// https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/llvm/lib/Object/WasmObjectFile.cpp#L198

WasmInitExpr ObjectFile::parse_init_expr(Context &ctx, const u8 *&data) {
    const u8 *const data_beg = data;

    const u8 op = *data;
    data++;

    bool extended = false;
    switch (op) {
    case wasm::WASM_OPCODE_I32_CONST:
        decodeSLEB128AndInc(data);
        break;
    case wasm::WASM_OPCODE_I64_CONST:
        decodeSLEB128AndInc(data);
        break;
    case wasm::WASM_OPCODE_F32_CONST:
        // IEEE754 encoded 32bit value
        data += 4;
        break;
    case wasm::WASM_OPCODE_F64_CONST:
        // IEEE754 encoded 64bit value
        data += 8;
        break;
    case wasm::WASM_OPCODE_GLOBAL_GET:
        decodeULEB128AndInc(data);
        break;
    case wasm::WASM_OPCODE_REF_NULL: {
        // https://webassembly.github.io/spec/core/binary/instructions.html#reference-instructions
        data++;
        break;
    }
    default:
        extended = true;
    }

    if (!extended) {
        const u8 op = *data;
        if (op == wasm::WASM_OPCODE_END) {
            data++;
            return WasmInitExpr{extended, std::span<const u8>(data_beg, data)};
        } else
            extended = true;
    }

    if (extended) {
        while (true) {
            u8 op = *data;
            data++;

            switch (op) {
            case wasm::WASM_OPCODE_I32_CONST:
            case wasm::WASM_OPCODE_GLOBAL_GET:
            case wasm::WASM_OPCODE_REF_NULL:
            case wasm::WASM_OPCODE_REF_FUNC:
            case wasm::WASM_OPCODE_I64_CONST:
                decodeULEB128AndInc(data);
                break;
            case wasm::WASM_OPCODE_F32_CONST:
                data += 4;
                break;
            case wasm::WASM_OPCODE_F64_CONST:
                data += 8;
                break;
            case wasm::WASM_OPCODE_I32_ADD:
            case wasm::WASM_OPCODE_I32_SUB:
            case wasm::WASM_OPCODE_I32_MUL:
            case wasm::WASM_OPCODE_I64_ADD:
            case wasm::WASM_OPCODE_I64_SUB:
            case wasm::WASM_OPCODE_I64_MUL:
                break;
            case wasm::WASM_OPCODE_GC_PREFIX:
                break;
            // The GC opcodes are in a separate (prefixed space). This flat
            // switch structure works as long as there is no overlap between the
            // GC and general opcodes used in init exprs.
            case wasm::WASM_OPCODE_STRUCT_NEW:
            case wasm::WASM_OPCODE_STRUCT_NEW_DEFAULT:
            case wasm::WASM_OPCODE_ARRAY_NEW:
            case wasm::WASM_OPCODE_ARRAY_NEW_DEFAULT:
                decodeULEB128AndInc(data);
                break;
            case wasm::WASM_OPCODE_ARRAY_NEW_FIXED:
                decodeULEB128AndInc(data); // heap type index
                decodeULEB128AndInc(data); // array size
                break;
            case wasm::WASM_OPCODE_REF_I31:
                break;
            case wasm::WASM_OPCODE_END:
                break;
            default:
                Fatal(ctx) << "invalid opcode in init_expr: 0x" << std::hex
                           << (u32)op;
            }
        }
    }

    Debug(ctx) << "p:" << (u64)data << "c;" << (u64)(*data);
    return WasmInitExpr{extended, std::span<const u8>(data_beg, data)};
}

void ObjectFile::parse(Context &ctx) {
    u8 *data = this->mf->data;
    const u8 *p = data + sizeof(WasmObjectHeader);

    while (p - data < this->mf->size) {
        u8 sec_id = *p;
        p++;
        u32 content_size = decodeULEB128AndInc(p);

        std::string sec_name{sec_id_as_str(sec_id)};
        Debug(ctx) << "parsing " << sec_name;

        const u8 *content_beg = p;
        u32 content_ofs = p - data;
        std::span<const u8> content{content_beg, content_beg + content_size};

        switch (sec_id) {
        case WASM_SEC_CUSTOM: {
            const u8 *cont_begin = p;
            sec_name = parse_name(p);
            u32 custom_content_size = (content_beg + content_size) - p;
            Debug(ctx) << "parsing " << sec_name;
            if (sec_name == "linking") {
                parse_linking_sec(ctx, p, custom_content_size);
            } else if (sec_name.starts_with("reloc.")) {
                parse_reloc_sec(ctx, p, custom_content_size);
            } else if (sec_name == "name") {
                parse_name_sec(ctx, p, custom_content_size);
            } else {
                Warn(ctx) << "custom section: " << sec_name << " ignored";
                // Skip
                p = content_beg + content_size;
            }
        } break;
        case WASM_SEC_TYPE: {
            std::function<WasmSignature(const u8 *&)> f = [&](const u8 *&data) {
                const u8 *type_begin = data;
                ASSERT(*data == 0x60 && "type section must start with 0x60");
                data++;
                // discard results
                std::vector<ValType> params = parse_vec<ValType>(data);
                std::vector<ValType> returns = parse_vec<ValType>(data);
                return WasmSignature(returns, params);
            };
            this->signatures = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_IMPORT: {
            std::function<WasmImport(const u8 *&)> f = [&](const u8 *&data) {
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
                    WasmLimits limits = parse_limits(data);
                    WasmTableType table{.elem_type = reftype, .limits = limits};
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
                    Fatal(ctx) << "unknown import kind: " << (int)kind;
                    unreachable();
                }
            };
            this->imports = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_FUNCTION: {
            u32 index = 0;
            std::function<WasmFunction(const u8 *&)> f = [&](const u8 *&data) {
                u32 sig_index = decodeULEB128AndInc(p);
                // TODO: check range?
                if (sig_index >= this->signatures.size())
                    Fatal(ctx)
                        << "sig_index=" << sig_index << " is out of range";

                return WasmFunction{
                    .index = index, .sig_index = sig_index, .name = ""};
                index++;
            };
            this->functions = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_EXPORT: {
            std::function<WasmExport(const u8 *&)> f = [&](const u8 *&data) {
                std::string name = parse_name(data);
                u8 kind = *data;
                data++;
                u32 index = decodeULEB128AndInc(data);
                return WasmExport{name, kind, index};
            };
            this->exports = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_MEMORY: {
            std::function<WasmLimits(const u8 *&)> f = parse_limits;
            this->memories = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_GLOBAL: {
            std::function<WasmGlobal(const u8 *&)> f = [&](const u8 *&data) {
                const ValType val_type{*data};
                data++;
                const bool mut = *data;
                data++;
                WasmInitExpr init_expr = parse_init_expr(ctx, data);
                return WasmGlobal{
                    .type = {.type = val_type, .mut = mut},
                    .init_expr = init_expr,
                    .symbol_name = "",
                };
            };
            this->globals = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_CODE: {
            std::function<std::span<const u8>(const u8 *&)> f =
                [&](const u8 *&data) {
                    const u8 *code_start = data;
                    u32 size = decodeULEB128AndInc(data);
                    std::span<const u8> code{code_start, data + size};
                    data += size;
                    return code;
                };
            this->codes = parse_vec_varlen(p, f);
        } break;
        default:
            Fatal(ctx) << "section: " << sec_id_as_str(sec_id) << "("
                       << sec_name << ") ignored";
            // Skip
            p = content_beg + content_size;
            break;
        }

        if (p - content_beg != content_size) {
            Fatal(ctx)
                << "bug! offset=0x" << (p - content_beg)
                << " after parsing this section differs from content_size=0x"
                << content_size;
        }

        this->sections.push_back(std::unique_ptr<InputSection>(
            new InputSection(sec_id, content, this, content_ofs, sec_name)));
    }

    this->dump(ctx);
}

void ObjectFile::dump(Context &ctx) {
    Debug(ctx) << "=== " << this->mf->name << " ===";
    for (auto &sec : this->sections) {
        Debug(ctx) << std::hex << "Section: " << sec_id_as_str(sec->sec_id)
                   << "(name=" << sec->name << ", offset=0x" << sec->file_ofs
                   << ", size=0x" << sec->content.size() << ")";
        switch (sec->sec_id) {
        case WASM_SEC_TYPE: {
            for (int i = 0; i < this->signatures.size(); i++) {
                Debug(ctx) << "  - type[" << i << "]";
            }
        } break;
        case WASM_SEC_IMPORT: {
            for (int i = 0; i < this->imports.size(); i++) {
                auto import = this->imports[i];
                Debug(ctx) << "  - import[" << i << "]: " << import.module
                           << "." << import.field;
            }
        } break;
        case WASM_SEC_FUNCTION: {
            for (int i = 0; i < this->functions.size(); i++) {
                const WasmFunction &func = this->functions[i];
                Debug(ctx) << "  - func[" << i << "]: " << func.name
                           << " (type[" << func.sig_index << "])";
            }
        } break;
        case WASM_SEC_MEMORY: {
            // TODO:
        }; break;
        case WASM_SEC_GLOBAL: {
            for (int i = 0; i < this->globals.size(); i++) {
                const WasmGlobal &global = this->globals[i];
                Debug(ctx) << "  - global[" << i << "]: " << global.symbol_name;
            }
        } break;
        case WASM_SEC_EXPORT: {
            for (int i = 0; i < this->exports.size(); i++) {
                Debug(ctx) << "  - export[" << i
                           << "]: " << this->exports[i].name;
            }
        } break;
        case WASM_SEC_CODE: {
            for (int i = 0; i < this->codes.size(); i++) {
                Debug(ctx) << "  - code[" << i << "]";
            }
        } break;
        case WASM_SEC_CUSTOM: {
            if (sec->name == "linking") {
                for (int i = 0; i < this->symbols.size(); i++) {
                    WasmSymbol sym = this->symbols[i];
                    std::string symname =
                        sym.info.import_module.has_value()
                            ? (sym.info.import_module.value() + "." +
                               sym.info.import_name.value())
                            : sym.info.name;
                    Debug(ctx) << "  - symbol[" << i << "]: " << symname;
                }
            }
        }
        }
        if (sec->relocs.size())
            Debug(ctx) << "  - num relocs: " << sec->relocs.size();
    }

    for (auto &sym : this->symbols) {
        Debug(ctx) << "  - symbol: " << sym.info.name;
    }
    Debug(ctx) << "=== Dump Ends ===";
}

} // namespace xld::wasm
