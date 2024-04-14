#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int foo() {
    return 42;
}

int main() {
    return foo();
}
EOF

$XLD $t/a.o --export-all --allow-undefined -o $t/a.wasm

node main.js $t/a.wasm | grep -q "41"
