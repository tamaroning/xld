#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int foo();

int main() {
    return foo();
}
EOF

$XLD $t/a.o --export-all --allow-undefined -o $t/a.wasm

node allow_undefined.js $t/a.wasm | grep -q "42" || echo "Unexpected output"
