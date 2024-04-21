# xld: Fast WebAssembly linker

xld is a linker for WebAssembly object files.
It takes advantage of the parallelism provided by multi-threading for fast linking. Currently, the goal is to support compatibility with the LLVM toolchain.

## TODO

- [ ] Target Features Section
- [x] Merging Global Sections
- [x] Merging Function Sections
- [x] Merging Data Sections
- [ ] Merging Custom Sections
- [ ] COMDATs
- [ ] Start Section
- [x] Import Section
- [x] reloc.CODE
- [ ] reloc.DATA
- [ ] reloc custom

> Relocation sections can only target code, data and custom sections.

https://github.com/WebAssembly/tool-conventions/blob/main/Linking.md

## References

- lld (wasm-ld): https://github.com/llvm/llvm-project/tree/main/lld/wasm
- mold: https://github.com/rui314/mold
- WebAssembly, Tool conventions: https://github.com/WebAssembly/tool-conventions/blob/main/Linking.md
- WebAssembly Specifications: https://webassembly.github.io/spec/
