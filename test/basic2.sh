#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/a.o -
int foo() {
    return 0;
}

int bar() {
    return 0;
}

int baz() {
    return 42;
}
EOF

cat <<EOF | $CC --target=wasm32 -xc -c -o $t/b.o -
int baz();

int main() {
    return baz();
}
EOF

$XLD $t/a.o $t/b.o --export-all -o $t/a.wasm

node main.js $t/a.wasm | grep -q "42" || echo "Unexpected output"
