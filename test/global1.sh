#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int g1 = 1;
int g2 = 2;
int g3 = 3;
EOF

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/b.o -
extern int g1, g2, g3;
int g4 = 4;

int main() {
    return g1 + g2 + g3 + g4;
}
EOF

$XLD $t/a.o $t/b.o --export-all -o $t/a.wasm

node main.js $t/a.wasm | grep -q "10"
