const filename = process.argv[2];

const fs = require('node:fs');
const wasmBuffer = fs.readFileSync(filename);
WebAssembly.instantiate(wasmBuffer).then(wasmModule => {
    const ins = wasmModule.instance;
    console.log(ins);
    console.log(ins.exports);
    console.log("Executing main");
    console.log(ins.exports.main());
});