#include "xld.h"

namespace xld::wasm {

// If we haven't seen the same `name` before, create a new instance
// of Symbol and returns it. Otherwise, returns the previously-
// instantiated object.
Symbol *get_symbol(Context &ctx, std::string_view name) {
    typename decltype(ctx.symbol_map)::const_accessor acc;
    ctx.symbol_map.insert(acc, {name, Symbol(name, nullptr)});
    return const_cast<Symbol *>(&acc->second);
}

bool should_export_symbol(Context &ctx, Symbol *sym) {
    if (ctx.arg.export_all) {
        return true;
    }

    return sym->visibility != Symbol::Visibility::Hidden && sym->is_exported;
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
