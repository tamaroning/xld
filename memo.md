# 高速化

## LLDの並列化手法
https://llvm.org/devmtg/2017-10/slides/Ueyama-lld.pdf

入力ファイルから出力ファイルのコピーと再配置: 並列化可能
文字列定数のマージ: 並列化可能
シンボルをシンボルテーブルに挿入: 並列化難しい

## moldの高速化手法
https://logmi.jp/tech/articles/325776

並行ハッシュマップ
https://www.youtube.com/watch?v=_LQ6jvB7sq8

## zld
https://github.com/michaeleisel/zld

https://github.com/michaeleisel/zld?tab=readme-ov-file#why-is-it-faster


# wasmリンカの出力

Executableを出力する場合

最低限必要なセクション
- type: function signatureを置く
- function: typeセクションのindexを指定
- memory: 1つでいい?よくわからん
- global: __stack_pointerなど
- export: memory[0]とfunctionを指定する
- code: functionのコードを置く
- name(custom): module, funcs, globalの名前を置く
- start: エントリーポイント

なくても動くが必要なやつ
- target_features(custom)


Codeセクション→OutputSection
https://github.com/tamaroning/mold/blob/3df7c8e89c507865abe0fad4ff5355f4d328f78d/elf/mold.h#L472

wasi/libc
https://github.com/bytecodealliance/wasmtime/blob/main/docs/WASI-documents.md


/home/tamaron/work/wasi-libc/sysroot

lld
https://lld.llvm.org/WebAssembly.html

Emscripten
https://emscripten.org/docs/compiling/Dynamic-Linking.html

https://emscripten.org/docs/optimizing/Optimizing-Code.html

Wasmtime
https://docs.rs/wasmtime/latest/wasmtime/struct.Linker.html

libc
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-21/libclang_rt.builtins-wasm32-wasi-21.0.tar.gz
tar xvf libclang_rt.builtins-wasm32-wasi-21.0.tar.gz
// lib/wasi/libclang_rt.builtins-wasm32.a

extract libc members
ar -x ../lib/wasm32-wasi/libc.a

wasm-ld-18 hello.o $WASI_SDK_SYSROOT/lib/wasm32-wasi/libc.a -o hello.wasm --entry=main
