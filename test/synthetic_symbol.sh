#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int main() {
    return 0;
}
EOF

$XLD $t/a.o --export-all -o $t/a.wasm

wasm-objdump -x $t/a.wasm | grep -q "table\[0\] type=funcref initial=1 max=1"
wasm-objdump -x $t/a.wasm | grep -q "memory\[0\] pages"
wasm-objdump -x $t/a.wasm | grep -q "global\[0\] i32 mutable=1 <__stack_pointer>"
# memory export
wasm-objdump -x $t/a.wasm | grep -q "memory\[0\] -> \"memory\""
