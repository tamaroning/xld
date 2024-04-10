#include "xld_private/symbol.h"
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
    return sym->visibility != Symbol::Visibility::Hidden &&
           (sym->is_exported || ctx.arg.export_all);
}

} // namespace xld::wasm
