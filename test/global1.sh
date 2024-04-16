#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int ret = 42;

int main() {
    return ret;
}
EOF

$XLD $t/a.o --export-all -o $t/a.wasm

node main.js $t/a.wasm | grep -q "42"
