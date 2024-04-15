const filename = process.argv[2];
function foo() {
    return 42;
}
const imports = {
    env: {
        foo: foo
    }
}

const fs = require('node:fs');
const wasmBuffer = fs.readFileSync(filename);
WebAssembly.instantiate(wasmBuffer, imports).then(wasmModule => {
    const ins = wasmModule.instance;
    console.log(ins.exports.main());
});
