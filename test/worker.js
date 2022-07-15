// polyfill for safari
if (!WebAssembly.instantiateStreaming) {
    WebAssembly.instantiateStreaming = async (resp, importObject) => {
            const source = await (await resp).arrayBuffer();
            return await WebAssembly.instantiate(source, importObject);
    };
}

// Construct webassembly module, memory (RAM), and environment (function imports).
// This is also the place to add any other WASM function imports that
// are included in the module but not imported by IOBL.
var wasmEnvironment = BLOB_IO.wasmImports;
var wasmMemory = new WebAssembly.Memory({initial:2600, maximum:2600});
wasmEnvironment.env.memory = wasmMemory;


self.addEventListener('message', function(event) {
    const {eventType, eventData, eventId } = event.data;
    if (eventType == "SUBMIT"){
        WebAssembly.instantiateStreaming(fetch('tests.wasm'), wasmEnvironment)
        .then(wasmModule => {
            TESTCASES.run(wasmModule);
        });
    }
}, false);