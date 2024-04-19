#include "xld_private/symbol.h"
#include "wasm/object.h"
#include "xld.h"

namespace xld::wasm {

u32 get_rank(const WasmSymbol &wsym) {
    if (wsym.is_undefined())
        return 0;
    else if (wsym.is_binding_weak())
        return 1;
    else if (wsym.is_binding_global())
        return 2;
    else
        return 0;
}

// If we haven't seen the same `name` before, create a new instance
// of Symbol and returns it. Otherwise, returns the previously-
// instantiated object.
Symbol *get_symbol(Context &ctx, std::string_view name) {
    typename decltype(ctx.symbol_map)::const_accessor acc;
    ctx.symbol_map.insert(acc, {name, Symbol(name, nullptr)});
    return const_cast<Symbol *>(&acc->second);
}

bool should_export_symbol(Context &ctx, Symbol *sym) {
    if (sym->visibility == Symbol::Visibility::Hidden)
        return false;

    if (ctx.arg.export_all)
        return true;

    return sym->is_exported;
}

bool should_import_symbol(Context &ctx, Symbol *sym) {
    if (ctx.arg.allow_undefined) {
        return true;
    }

    return sym->is_defined();
}

bool allow_undefined_symbol(Context &ctx, Symbol *sym) {
    return ctx.arg.allow_undefined;
}

} // namespace xld::wasm
