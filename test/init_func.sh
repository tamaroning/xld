#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC --target=wasm32 -xc++ -c -o $t/a.o -
class C {
  public:
    C() {
        a = 42;
    }
    int a = 0;
};

C c {};

int main() {
    return c.a;
}
EOF

$XLD $t/a.o --export-all -o $t/a.wasm

node main.js $t/a.wasm | grep -q "42"
