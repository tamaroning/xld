#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
__attribute__((export_name("export_bar"))) 
int bar() {
    return bar();
}

__attribute__((export_name("export_foo"))) 
int foo() {
    return foo();
}

int main() {
    return foo();
}
EOF

$XLD $t/a.o --export-all -o $t/a.wasm

wasm-objdump -x $t/a.wasm | grep -q "<bar> -> \"export_bar\""
wasm-objdump -x $t/a.wasm | grep -q "<foo> -> \"export_foo\""
