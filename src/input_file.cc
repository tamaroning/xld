#include "common/leb128.h"
#include "section.h"
#include "wao.h"
#include "wasm.h"
#include "xld.h"

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

template <typename T, typename E>
std::vector<T> parse_vec(Context<E> &ctx, const u8 *data) {
    u64 num = decodeULEB128AndInc(data);
    if (num == 0)
        return std::vector<T>();
    std::vector<T> vec{data, data + sizeof(T) * num};
    assert(vec.size() == num);
    return vec;
}

template <typename E>
void ObjectFile<E>::parse(Context<E> &ctx) {
    u8 *data = this->mf->data;
    const u8 *p = data + sizeof(WasmObjectHeader);

    while (p - data < this->mf->size) {
        u8 sec_id = *p;
        p += 1;
        u64 size = decodeULEB128AndInc(p);

        std::string_view name = "<unknown>";
        switch (sec_id) {
        case WASM_SEC_CUSTOM: {
            std::vector<char> name_vec = parse_vec<char>(ctx, p);
            name = std::string_view(name_vec.begin(), name_vec.end());
        } break;
        default:
            // Warn(ctx) << "section id=" << (u32)sec_id << " is not supported";
            break;
        }

        this->sections.push_back(std::unique_ptr<InputSection<E>>(
            new InputSection(sec_id, p, size, this, p - data, name)));

        p += size;
    }
}

using E = WASM32;

template class InputFile<E>;
template class ObjectFile<E>;

} // namespace xld::wasm
