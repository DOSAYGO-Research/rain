let rainstormModulePromise = import('./../wasm/rainstorm.js');

let moduleReady = new Promise((resolve, reject) => {
    rainstormModulePromise.then(rainstormModule => {
        rainstormModule.onRuntimeInitialized = () => {
            resolve(rainstormModule);
        };
        rainstormModule.onAbort = reject;
    });
});

test();

async function test() {
    let rainstorm = await moduleReady;
    const result = await rainstormHash(rainstorm, 256, 0, "");
    console.log("Result", result);
}

async function rainstormHash(rainstorm, hashSize, seed, input) {
    // Convert the input to a bytes
    let data = new TextEncoder().encode(input);

    // Allocate memory in the WASM module for the input data and output hash
    let dataPtr = rainstorm._malloc(data.length);
    let hashPtr = rainstorm._malloc(hashSize / 8);  // convert bit size to byte size

    // Copy the data into the allocated WASM memory
    rainstorm.HEAPU8.set(data, dataPtr);

    // Choose the correct hash function based on the hash size
    let hashFunc;
    switch (hashSize) {
        case 64:
            hashFunc = rainstorm._rainstormHash64;
            break;
        case 128:
            hashFunc = rainstorm._rainstormHash128;
            break;
        case 256:
            hashFunc = rainstorm._rainstormHash256;
            break;
        case 512:
            hashFunc = rainstorm._rainstormHash512;
            break;
        default:
            throw new Error(`Unsupported hash size: ${hashSize}`);
    }

    // Compute the hash
    hashFunc(dataPtr, data.length, seed, hashPtr);

    // Retrieve the hash from the WASM memory
    let hash = rainstorm.HEAPU8.subarray(hashPtr, hashPtr + hashSize / 8);

    // Don't forget to free the memory when you're done!
    rainstorm._free(dataPtr);
    rainstorm._free(hashPtr);

    // Return the hash as a Uint8Array
    return new Uint8Array(hash);
}

