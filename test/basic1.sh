#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int foo_internal() {
    return 42;
}
EOF

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/b.o -
int foo_internal();

int main() {
    return foo_internal();
}
EOF

$XLD $t/a.o $t/b.o --export-all --allow-undefined -o $t/a.wasm

node main.js $t/a.wasm | grep -q "42" || echo "Unexpected output"
