# xld: Fast WASM linker

## TODO
- [ ] Target Features Section
- [x] Merging Global Sections
- [x] Merging Function Sections
- [ ] Merging Data Sections
- [ ] Merging Custom Sections
- [ ] COMDATs
- [ ] Start Section
- [ ] Import Sectio
- [x] reloc.CODE
- [ ] reloc.DATA
- [ ] reloc custom
- [ ] ignore local symbols

> Relocation sections can only target code, data and custom sections.

https://github.com/WebAssembly/tool-conventions/blob/main/Linking.md

## References

- lld (wasm-ld): https://github.com/llvm/llvm-project/tree/main/lld/wasm
- mold: https://github.com/rui314/mold
- WebAssembly, Tool conventions: https://github.com/WebAssembly/tool-conventions/blob/main/Linking.md
- WebAssembly Specifications: https://webassembly.github.io/spec/
