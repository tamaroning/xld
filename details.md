# Linking WASM modules together

## Output Symbols

Symbolを保持する基準=isLive

https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/SymbolTable.cpp#L736

Symbol::isLive() => InputGlobal/Tag/Table::live
https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/Symbols.cpp#L148

InputGlobal, ...はInputElementを継承している
https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/InputElement.h#L38

Marker::run() isLiveなシンボルをenqueue()する

1. isNoStrip or isExported
2. Relocationによりreachableなもの
https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/MarkLive.cpp#L105

isNoStrip: flags & WASM_SYMBOL_NO_STRIP
Indicates that the symbol is used in an __attribute__((used)) directive or similar.
isExported: flags & WASM_SYMBOL_EXPORTED

https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/MarkLive.cpp#L135

```
// If the function has been assigned the special index zero in the table,
// the relocation doesn't pull in the function body, since the function
// won't actually go in the table (the runtime will trap attempts to call
// that index, since we don't use it).  A function with a table index of
// zero is only reachable via "call", not via "call_indirect".  The stub
// functions used for weak-undefined symbols have this behaviour (compare
// equal to null pointer, only reachable via direct call).

reloc.Type == R_WASM_TABLE_INDEX_SLEB ||
          reloc.Type == R_WASM_TABLE_INDEX_SLEB64 ||
          reloc.Type == R_WASM_TABLE_INDEX_I32 ||
          reloc.Type == R_WASM_TABLE_INDEX_I64
```


Enqueue()
inputChunkをmarkする
https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/MarkLive.cpp#L79

=> 関数単位でliveではなく、liveな関数が属するセクションをretainする
e.g. グローバルセクションで、liveなものが一つ、liveでないものが一つあったとすると、どちらもretainする

## Garbege collected sections

https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/MarkLive.cpp#L166

## Synthetic Symbols    

### Stack Pointer

no-PICの場合: 普通にグローバルで定義する
PICの場合: 未定義のシンボルとして定義する(imported?)さらに、__memory_baseと__table_baseをインポートする必要がある。


https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/Driver.cpp#L871

### Indirect Function Table

基本的に入力モジュールが要求する場合に作成する必要がある。
GOTが絡むケースでは入力モジュールが要求しないのに必要になることがある。

https://github.com/llvm/llvm-project/blob/e6f63a942a45e3545332cd9a43982a69a4d5667b/lld/wasm/SymbolTable.cpp#L706
