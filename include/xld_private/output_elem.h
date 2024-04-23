#pragma once

#include "common/integers.h"
#include "wasm/object.h"
#include "xld_private/input_file.h"
#include <map>

namespace xld::wasm {

class ObjectFile;

class OutputSegment {
  public:
    static OutputSegment *get_or_create(Context &ctx,
                                        const std::string_view &name);
    OutputSegment(const OutputSegment &) = delete;

    OutputSegment(std::string_view name) : name(name), ifrag_map() {
        // TODO: OK?
        memory_index = 0;
        p2align = 1;
    };

    void merge(Context &ctx, const WasmDataSegment &seg, InputFragment *ifrag);

    void set_virtual_address(i32 va);

    i32 get_virtual_address() const;

    std::optional<u32> get_frag_offset(const std::string_view &frag_name) {
        auto it = ifrag_map.find(frag_name);
        if (it == ifrag_map.end())
            return std::nullopt;
        return it->second.second;
    }

    std::string_view get_name() const { return name; }

    u32 get_size() const { return size; }

    u32 get_init_flags() const { return init_flags; }
    u32 get_memory_index() const { return memory_index; }
    u32 get_linking_flags() const {
        // TODO:
        return 0;
    }

    tbb::concurrent_map<std::string_view, std::pair<InputFragment *, u32>> const
        &
        get_ifrag_map() const {
        return ifrag_map;
    }

    u32 p2align;

    u64 out_offset = 0;

  private:
    std::string_view name;
    // segment name -> (input fragment, offset)
    tbb::concurrent_map<std::string_view, std::pair<InputFragment *, u32>>
        ifrag_map;
    u32 init_flags;
    u32 memory_index;
    u32 linking_flags;
    u32 size;
    u32 va;
};

} // namespace xld::wasm
