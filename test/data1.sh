#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int ret = 122;

int main() {
    ret += 1;
    return ret;
}
EOF

$XLD $t/a.o --export-all -o $t/a.wasm

node main.js $t/a.wasm | grep -q "123" || echo "Unexpected output"

# FIXME:
#wasm-objdump -x $t/a.wasm | grep -q "<bar> -> \"export_bar\""
#wasm-objdump -x $t/a.wasm | grep -q "<foo> -> \"export_foo\""
