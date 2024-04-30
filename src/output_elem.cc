#include "common/log.h"
#include "wasm/object.h"
#include "xld.h"
#include <map>
#include <utility>

namespace xld::wasm {

OutputSegment *OutputSegment::get_or_create(Context &ctx,
                                            const std::string_view &name) {
    std::string_view name_;
    if (name.starts_with(".data."))
        name_ = ".data";
    else if (name.starts_with(".bss."))
        name_ = ".bss";
    else
        Fatal(ctx) << "Unknown segment name: " << name;

    auto it = ctx.segments.find(name_);
    if (it == ctx.segments.end()) {
        auto it = ctx.segments.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(name_),
                                       std::forward_as_tuple(name_));
        return &it.first->second;
    } else {
        return &it->second;
    }
}

void OutputSegment::merge(Context &ctx, const WasmDataSegment &seg,
                          InputFragment *ifrag) {
    if (ifrag_map.empty())
        init_flags = seg.init_flags;
    else if (init_flags != seg.init_flags)
        Fatal(ctx) << "Incompatible init flags for segment: " << name;

    size = align(size, seg.p2align);
    p2align = std::max(size, seg.p2align);

    ifrag_map.insert({seg.name, {ifrag, size}});
    size += ifrag->get_size();
}

void OutputSegment::set_virtual_address(i32 va) { this->va = va; }

i32 OutputSegment::get_virtual_address() const { return va; }

} // namespace xld::wasm