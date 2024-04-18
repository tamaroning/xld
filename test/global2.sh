#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int g1 = 20;
EOF

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/b.o -
extern int g1;
int g2 = 22;

int main() {
    return g1 + g2;
}
EOF

$XLD $t/a.o $t/b.o --export-all -o $t/a.wasm

node main.js $t/a.wasm | grep -q "42"
