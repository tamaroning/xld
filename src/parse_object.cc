#include "common/integers.h"
#include "common/leb128.h"
#include "common/system.h"
#include "object/wao_basic.h"
#include "object/wao_symbol.h"
#include "xld.h"

namespace xld::wasm {

u32 parse_varuint32(const u8 *&data) { return decodeULEB128AndInc(data); }

i32 parse_varint32(const u8 *&data) { return decodeSLEB128AndInc(data); }

// Parse vector of variable-length element and increment pointer
// f needs to increment pointer
template <typename T>
std::vector<T> parse_vec_varlen(const u8 *&data,
                                std::function<T(const u8 *&)> f) {
    u32 num = parse_varuint32(data);
    std::vector<T> v;
    for (int i = 0; i < num; i++) {
        v.push_back(f(data));
    }
    return v;
}

void foreach_vec(const u8 *&data, std::function<void(const u8 *&)> f) {
    u32 num = parse_varuint32(data);
    for (int i = 0; i < num; i++) {
        f(data);
    }
}

// Parse vector of fix-length element and increment pointer
template <typename T>
std::vector<T> parse_vec(const u8 *&data) {
    u32 num = parse_varuint32(data);
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
    u32 num = parse_varuint32(data);
    std::vector<u32> v;
    for (int i = 0; i < num; i++) {
        v.push_back(parse_varuint32(data));
    }
    return v;
}

// Parse name and increment pointer
std::string parse_name(const u8 *&data) {
    u64 num = parse_varuint32(data);
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
        u64 min = parse_varuint32(data);
        u64 max = parse_varuint32(data);
        limits = WasmLimits{flags, min, max};
    } else {
        u64 min = parse_varuint32(data);
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
    u32 version = parse_varuint32(p);
    if (version != 2)
        Warn(ctx) << "linking version must be 2";

    std::vector<wasm::WasmImport *> imported_globals;
    std::vector<wasm::WasmImport *> imported_functions;
    std::vector<wasm::WasmImport *> imported_tags;
    std::vector<wasm::WasmImport *> imported_tables;
    imported_globals.reserve(imports.size());
    imported_functions.reserve(imports.size());
    imported_tags.reserve(imports.size());
    imported_tables.reserve(imports.size());
    for (auto &i : imports) {
        if (i.kind == wasm::WASM_EXTERNAL_FUNCTION)
            imported_functions.emplace_back(&i);
        else if (i.kind == wasm::WASM_EXTERNAL_GLOBAL)
            imported_globals.emplace_back(&i);
        else if (i.kind == wasm::WASM_EXTERNAL_TAG)
            imported_tags.emplace_back(&i);
        else if (i.kind == wasm::WASM_EXTERNAL_TABLE)
            imported_tables.emplace_back(&i);
    }

    while (p < beg + size) {
        u8 type = *p;
        p++;
        // TODO: use result
        u32 payload_len = parse_varuint32(p);

        // TODO: refactor
        // https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/llvm/lib/Object/WasmObjectFile.cpp#L709
        switch (type) {
        case WASM_SYMBOL_TABLE: {
            u64 count = parse_varuint32(p);
            while (count--) {
                WasmSymbolType type{*p};
                p++;
                u32 flags = parse_varuint32(p);
                bool is_defined = (flags & wasm::WASM_SYMBOL_UNDEFINED) == 0;

                WasmSymbolInfo info;
                WasmGlobalType *global_type = nullptr;
                WasmTableType *table_type = nullptr;
                WasmSignature *signature = nullptr;
                switch (type) {
                case WASM_SYMBOL_TYPE_FUNCTION: {
                    u32 index = parse_varuint32(p);

                    std::string symbol_name;
                    std::optional<std::string> import_module;
                    std::optional<std::string> import_name;
                    if (is_defined) {
                        if (!is_defined_function(index))
                            Error(ctx) << "function index=" << index
                                       << " is out of range";

                        WasmFunction &func = get_defined_function(index);
                        symbol_name = parse_name(p);
                        if (func.symbol_name.empty())
                            func.symbol_name = symbol_name;
                    } else {
                        WasmImport &import = *imported_functions[index];
                        if ((flags & wasm::WASM_SYMBOL_EXPLICIT_NAME) != 0) {
                            symbol_name = parse_name(p);
                            import_name = import.field;
                            import_module = import.module;
                        } else {
                            symbol_name = import.field;
                        }
                        signature = &signatures[import.sig_index];
                    }
                    info = WasmSymbolInfo{.name = symbol_name,
                                          .kind = type,
                                          .flags = flags,
                                          .import_module = import_module,
                                          .import_name = import_name,
                                          .export_name = std::nullopt,
                                          .value = {.element_index = index}};
                } break;
                case WASM_SYMBOL_TYPE_TABLE:
                case WASM_SYMBOL_TYPE_GLOBAL: {
                    bool index_references_import =
                        (flags & WASM_SYMBOL_UNDEFINED) &&
                        !(flags & WASM_SYMBOL_EXPLICIT_NAME);
                    u32 index = parse_varuint32(p);
                    // linking name
                    std::string symbol_name;
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
                                    symbol_name = imp.field;
                                    break;
                                }
                                current_index++;
                            }
                        }
                        if (!import_module.has_value())
                            Error(ctx) << "Corrupsed index=" << index;
                    } else {
                        symbol_name = parse_name(p);
                    }
                    info = WasmSymbolInfo{.name = symbol_name,
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
                            if (is_defined_function(index)) {
                                WasmFunction &func =
                                    get_defined_function(index);
                                // Set symbol name to function
                                func.symbol_name = info.name;
                                signature = &this->signatures[func.sig_index];
                            } else {
                                Error(ctx) << "function index=" << index
                                           << " is out of range";
                            }
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
                            if (is_defined_global(index)) {
                                WasmGlobal &global = get_defined_global(index);
                                global.symbol_name = info.name;
                                global_type = &global.type;
                            } else
                                Fatal(ctx) << "global index=" << index
                                           << " is out of range";
                        } break;
                        default:
                            Fatal(ctx) << "unknown symbol type: " << (int)type;
                        }
                    }
                } break;
                case WASM_SYMBOL_TYPE_DATA: {
                    std::string name = parse_name(p);
                    u32 segment = 0;
                    u32 offset = 0;
                    u32 size = 0;
                    if (!(flags & WASM_SYMBOL_UNDEFINED)) {
                        segment = parse_varuint32(p);
                        offset = parse_varuint32(p);
                        size = parse_varuint32(p);
                    }
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
                // FIXME: name
                ASSERT(info.name.size() > 0);
                WasmSymbol sym(info, global_type, table_type, signature);
                this->symbols.push_back(sym);
            }
        } break;
        case WASM_SEGMENT_INFO: {
            u32 index = 0;
            std::function<void(const u8 *&)> f = [&](const u8 *&data) {
                this->data_segments[index].name = parse_name(data);
                this->data_segments[index].alignment = parse_varuint32(data);
                this->data_segments[index].linking_flags =
                    parse_varuint32(data);
                index++;
            };
            foreach_vec(p, f);
        } break;
        default: {
            Error(ctx) << "unknown linking entry type: " << (int)type;
            return;
        }
        }
    }
}

void ObjectFile::parse_reloc_sec(Context &ctx, const u8 *&p, const u32 size) {
    u32 sec_idx = parse_varuint32(p);
    if (sec_idx >= this->sections.size())
        Fatal(ctx) << "section index in WasmRelocation is corrupsed";
    u32 count = parse_varuint32(p);
    for (int i = 0; i < count; i++) {
        u8 type = *p;
        p++;
        u32 offset = parse_varuint32(p);
        u32 index = parse_varuint32(p);
        i32 addend = 0;
        switch (type) {
        // R_WASM_MEMORY_*
        case R_WASM_MEMORY_ADDR_LEB:
        case R_WASM_MEMORY_ADDR_SLEB:
        case R_WASM_MEMORY_ADDR_I32:
        case R_WASM_MEMORY_ADDR_REL_SLEB:
        case R_WASM_MEMORY_ADDR_LEB64:
        case R_WASM_MEMORY_ADDR_SLEB64:
        case R_WASM_MEMORY_ADDR_I64:
        case R_WASM_MEMORY_ADDR_REL_SLEB64:
        case R_WASM_MEMORY_ADDR_TLS_SLEB:
        case R_WASM_MEMORY_ADDR_LOCREL_I32:
        case R_WASM_MEMORY_ADDR_TLS_SLEB64:
        // R_WASM_FUNCTION_OFFSET
        case R_WASM_FUNCTION_OFFSET_I32:
        case R_WASM_FUNCTION_OFFSET_I64:
        // R_WASM_SECTION_OFFSET_I64 does not exist
        case R_WASM_SECTION_OFFSET_I32:
            addend = parse_varint32(p);
            break;
        }
        this->sections[sec_idx]->relocs.push_back(WasmRelocation{
            .type = type,
            .index = index,
            .offset = offset,
            .addend = addend,
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
        u32 subsec_size = parse_varuint32(p);

        switch (subsec_kind) {
        case WASM_NAMES_MODULE: {
            // parse a single name
            this->module_name = parse_name(p);
        } break;
        case WASM_NAMES_FUNCTION: {
            // indeirectnamemap
            u32 count = parse_varuint32(p);
            while (count--) {
                u32 func_index = parse_varuint32(p);
                std::string name = parse_name(p);
                if (is_defined_function(func_index)) {
                    WasmFunction &func = get_defined_function(func_index);
                    func.debug_name = name;
                }
            }
        } break;
        case WASM_NAMES_LOCAL: {
            // indirectnamemap
            u32 count = parse_varuint32(p);
            while (count--) {
                parse_varuint32(p);
                parse_name(p);
                // ignore local names for now
            }
        } break;
        case WASM_NAMES_GLOBAL: {
            // indirectnamemap
            u32 count = parse_varuint32(p);
            for (int j = 0; j < count; j++) {
                u32 global_index = parse_varuint32(p);
                std::string name = parse_name(p);
                // TODO:
                Debug(ctx) << "global[" << global_index
                           << "].debug_name=" << name;
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
        parse_varint32(data);
        break;
    case wasm::WASM_OPCODE_I64_CONST:
        parse_varint32(data);
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
        parse_varuint32(data);
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
                parse_varuint32(data);
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
                parse_varuint32(data);
                break;
            case wasm::WASM_OPCODE_ARRAY_NEW_FIXED:
                parse_varuint32(data); // heap type index
                parse_varuint32(data); // array size
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
    if (mf == nullptr)
        return;

    u8 *data = this->mf->data;
    const u8 *p = data + sizeof(WasmObjectHeader);

    while (p - data < this->mf->size) {
        u8 sec_id = *p;
        p++;
        u32 content_size = parse_varuint32(p);

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
                WasmImportKind kind = WasmImportKind(*data);
                data++;
                switch (kind) {
                case WASM_EXTERNAL_FUNCTION: {
                    this->num_imported_functions++;
                    u32 sig_index = parse_varuint32(data);
                    return WasmImport{
                        module, field, kind, {.sig_index = sig_index}};
                } break;
                case WASM_EXTERNAL_TABLE: {
                    this->num_imported_tables++;
                    ValType reftype{*data};
                    data++;
                    WasmLimits limits = parse_limits(data);
                    WasmTableType table{.elem_type = reftype, .limits = limits};
                    return WasmImport{module, field, kind, {.table = table}};
                } break;
                case WASM_EXTERNAL_MEMORY: {
                    this->num_imported_memories++;
                    WasmLimits limits = parse_limits(data);
                    return WasmImport{module, field, kind, {.memory = limits}};
                } break;
                case WASM_EXTERNAL_GLOBAL: {
                    this->num_imported_globals++;
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
            std::function<void(const u8 *&)> f = [&](const u8 *&data) {
                u32 sig_index = parse_varuint32(p);
                if (sig_index >= this->signatures.size())
                    Fatal(ctx)
                        << "sig_index=" << sig_index << " is out of range";

                u32 index = this->functions.size();
                this->functions.push_back(WasmFunction{
                    .index = index, .sig_index = sig_index, .symbol_name = ""});
            };
            foreach_vec(p, f);
        } break;
        case WASM_SEC_EXPORT: {
            std::function<WasmExport(const u8 *&)> f = [&](const u8 *&data) {
                std::string name = parse_name(data);
                WasmImportKind kind = WasmImportKind(*data);
                data++;
                u32 index = parse_varuint32(data);
                switch (kind) {
                case WasmImportKind::FUNCTION:
                    if (is_defined_function(index)) {
                        WasmFunction &func = get_defined_function(index);
                        func.export_name = name;
                    } else {
                        Fatal(ctx)
                            << "function index=" << index << " is out of range";
                    }
                    break;
                case WasmImportKind::GLOBAL:
                    if (!is_defined_global(index)) {
                        Fatal(ctx)
                            << "global index=" << index << " is out of range";
                    }
                    break;
                case WasmImportKind::MEMORY:
                    if (!is_defined_memories(index)) {
                        Fatal(ctx)
                            << "memory index=" << index << " is out of range";
                    }
                    break;
                case WasmImportKind::TABLE:
                    // TODO: table
                default:
                    break;
                }
                return WasmExport{name, kind, index};
            };
            this->exports = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_MEMORY: {
            std::function<void(const u8 *&)> f = [&](const u8 *&data) {
                WasmLimits limits = parse_limits(data);
                this->memories.push_back(limits);
            };
            foreach_vec(p, f);
        } break;
        case WASM_SEC_GLOBAL: {
            std::function<void(const u8 *&)> f = [&](const u8 *&data) {
                const ValType val_type{*data};
                data++;
                const bool mut = *data;
                data++;
                WasmInitExpr init_expr = parse_init_expr(ctx, data);
                this->globals.push_back(WasmGlobal{
                    .type = {.type = val_type, .mut = mut},
                    .init_expr = init_expr,
                    .symbol_name = "",
                });
            };
            foreach_vec(p, f);
        } break;
        case WASM_SEC_CODE: {
            std::function<std::span<const u8>(const u8 *&)> f =
                [&](const u8 *&data) {
                    const u8 *code_start = data;
                    u32 size = parse_varuint32(data);
                    std::span<const u8> code{code_start, data + size};
                    data += size;
                    return code;
                };
            this->codes = parse_vec_varlen(p, f);
        } break;
        case WASM_SEC_DATACOUNT: {
            this->data_count = parse_varuint32(p);
        } break;
        case WASM_SEC_DATA: {
            u32 datacount = parse_varuint32(p);
            if (datacount != this->data_count)
                Fatal(ctx) << "datacount section is not equal to data section";

            for (int i = 0; i < datacount; i++) {
                u32 flags = parse_varuint32(p);
                WasmDataSegment data;
                switch (flags) {
                case 0: {
                    WasmInitExpr offset = parse_init_expr(ctx, p);
                    std::vector<u8> content = parse_vec<u8>(p);
                    data = WasmDataSegment{.init_flags = flags,
                                           .offset = offset,
                                           .content = content};
                } break;
                case WASM_DATA_SEGMENT_IS_PASSIVE: {
                    std::vector<u8> content = parse_vec<u8>(p);
                    data = WasmDataSegment{.init_flags = flags,
                                           .content = content};
                } break;
                case WASM_DATA_SEGMENT_HAS_MEMINDEX: {
                    u32 mem_index = parse_varuint32(p);
                    WasmInitExpr offset = parse_init_expr(ctx, p);
                    std::vector<u8> content = parse_vec<u8>(p);
                    data = WasmDataSegment{.init_flags = flags,
                                           .memory_index = mem_index,
                                           .offset = offset,
                                           .content = content};
                } break;
                default:
                    Fatal(ctx) << "unknown data segment flags: " << flags;
                }
                this->data_segments.push_back(data);
            }
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
            for (WasmImport &import : this->imports) {
                u32 func_index = 0;
                u32 table_index = 0;
                u32 memory_index = 0;
                u32 global_index = 0;
                switch (import.kind) {
                case WASM_EXTERNAL_FUNCTION:
                    Debug(ctx) << "  - func[" << func_index
                               << "]: " << import.module << "." << import.field;
                    func_index++;
                    break;
                case WASM_EXTERNAL_TABLE:
                    Debug(ctx) << "  - table[" << table_index
                               << "]: " << import.module << "." << import.field;
                    table_index++;
                    break;
                case WASM_EXTERNAL_MEMORY:
                    Debug(ctx) << "  - memory[" << memory_index
                               << "]: " << import.module << "." << import.field;
                    memory_index++;
                    break;
                case WASM_EXTERNAL_GLOBAL:
                    Debug(ctx) << "  - global[" << global_index
                               << "]: " << import.module << "." << import.field;
                    global_index++;
                    break;
                }
            }
        } break;
        case WASM_SEC_FUNCTION: {
            for (int i = 0; i < this->functions.size(); i++) {
                const WasmFunction &func = this->functions[i];
                Debug(ctx) << "  - func[" << i + num_imported_functions
                           << "]: " << func.symbol_name << " (type["
                           << func.sig_index
                           << "], export_name=" << func.export_name << ")";
            }
        } break;
        case WASM_SEC_MEMORY: {
            for (int i = 0; i < this->memories.size(); i++) {
                const WasmLimits &mem = this->memories[i];
                Debug(ctx) << "  - memory[" << i + num_imported_memories
                           << "]: min=" << mem.minimum
                           << ", max=" << mem.maximum;
            }
        } break;
        case WASM_SEC_GLOBAL: {
            for (int i = 0; i < this->globals.size(); i++) {
                const WasmGlobal &global = this->globals[i];
                Debug(ctx) << "  - global[" << i + num_imported_globals
                           << "]: " << global.symbol_name;
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
        case WASM_SEC_DATA: {
            for (int i = 0; i < this->data_segments.size(); i++) {
                WasmDataSegment &data_seg = this->data_segments[i];
                Debug(ctx) << "  - data[" << i << "]";
            }
        } break;
        case WASM_SEC_CUSTOM: {
            if (sec->name == "linking") {
                for (int i = 0; i < this->symbols.size(); i++) {
                    WasmSymbol sym = this->symbols[i];
                    std::string symname =
                        sym.info.import_name.has_value()
                            ? (sym.info.import_module.value() + "." +
                               sym.info.import_name.value())
                            : sym.info.name;
                    Debug(ctx) << "  - symbol[" << i << "]: " << symname;
                }
                for (int i = 0; i < this->data_segments.size(); i++) {
                    WasmDataSegment &data_seg = this->data_segments[i];
                    Debug(ctx) << "  - data[" << i << "]: " << data_seg.name;
                }
            }
        } break;
        }
        if (sec->relocs.size())
            Debug(ctx) << "  - num relocs: " << sec->relocs.size();
    }

    /*
    for (auto &sym : this->symbols) {
        Debug(ctx) << "  - symbol: " << sym.info.name;
    }
    */

    Debug(ctx) << "=== Dump Ends ===";
}

} // namespace xld::wasm
