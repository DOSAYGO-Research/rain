// lib/api.mjs

let rain = { loaded: false };

const STORM_TV = [
  ["0ec74bfcbbc1e74f7e3f6adc47dc267644a2071f2d8f4fc931adb96b864c0a5a", ""],
  ["379bd2823607188d1a2f8c5621feda9002dc8ff1f0cc9902d55d6c99a6488240", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"],
  ["600e741274ca196710064e3d8e892bebce5c9ec47944dbf0f56608a5cb21748a", "The quick brown fox jumps over the lazy dog"],
  ["cfb961820b823c889e03ca79d130eb2d919a38f947e68745349a73e2f43f7392", "The quick brown fox jumps over the lazy cog"],
  ["9161d399ef638b837b821631847aa6603edb66ad16e14e25f9f96d119bebca1b", "The quick brown fox jumps over the lazy dog."],
  ["eaf60e7a4817e110d21b5d7888e0993f7ce4c4317ad168e47370cb67d053e41a", "After the rainstorm comes the rainbow."],
  ["e2490c856a9f5086a896d2eba9b640e38c8ac8c234e769fb7b8ddd9f112a1727", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"],
];

const BOW_TV = [
  ["9af7f7b2faaf87e9da4fe493916a78567ec2284018028a5df78968e351cc6dda", ""],
  ["4bb4b7dbc4bd78f011c9f41293564abb79f12da16bac056c98aa68fe1c3eed8f", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"],
  ["f652f20dbd02240238372c8eace0215549a02552bb2eac31bf802fbfa58f5546", "The quick brown fox jumps over the lazy dog"],
  ["da30e89eab4bd6300211d53d9edd5a8bb523528b722b7db46e8876550cdc95f7", "The quick brown fox jumps over the lazy cog"],
  ["b669467111c6fda5b5aadb8c290ef033e66be5241276a199cb10fc442c34cbe7", "The quick brown fox jumps over the lazy dog."],
  ["f7f217751f5940df46fafd1b1edbd1fc8d1398d3bac00385582cefce03016ab2", "After the rainstorm comes the rainbow."],
  ["5b6217ce8fff616a75207982644a8dbbe5f235370e9178b801f329eac511c8f6", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"],
];

export async function testVectors() {
  if (!rain.loaded) {
    await loadRain();
  }
  let comment;

  console.log(`Rainstorm test vectors:`);
  for (const [expectedHash, message] of STORM_TV) {
    const calculatedHash = await rainstormHash(256, 0, message);
    if (calculatedHash !== expectedHash) {
      comment = "MISMATCH!";
      console.error(`Expected: ${expectedHash}, but got: ${calculatedHash}`);
    } else {
      comment = "";
    }
    console.log(`${calculatedHash} "${message}" ${comment}`);
  }

  console.log(`Rainbow test vectors:`);
  for (const [expectedHash, message] of BOW_TV) {
    const calculatedHash = await rainbowHash(256, 0, message);
    if (calculatedHash !== expectedHash) {
      comment = "MISMATCH!";
      console.error(`Expected: ${expectedHash}, but got: ${calculatedHash}`);
    } else {
      comment = "";
    }
    console.log(`${calculatedHash} "${message}" ${comment}`);
  }
}

export async function rainstormHash(hashSize, seed, input) {
  if (!rain.loaded) {
    await loadRain();
  }
  // Convert the input to a bytes
  const { stringToUTF8, lengthBytesUTF8 } = rain;
  const { wasmExports: { malloc: _malloc, free: _free } } = rain;

  const hashLength = hashSize / 8;
  const hashPtr = _malloc(hashLength);

  let inputPtr;
  let inputLength;

  if (typeof input == "string") {
    inputLength = lengthBytesUTF8(input);
    inputPtr = _malloc(inputLength);
    stringToUTF8(input, inputPtr, inputLength + 1);
  } else {
    inputLength = input.length;
    inputPtr = _malloc(inputLength);
    rain.HEAPU8.set(input, inputPtr);
  }

  seed = BigInt(seed);

  // Choose the correct hash function based on the hash size
  let hashFunc;

  switch (hashSize) {
    case 64:
      hashFunc = rain._rainstormHash64;
      break;
    case 128:
      hashFunc = rain._rainstormHash128;
      break;
    case 256:
      hashFunc = rain._rainstormHash256;
      break;
    case 512:
      hashFunc = rain._rainstormHash512;
      break;
    default:
      throw new Error(`Unsupported hash size for rainstorm: ${hashSize}`);
  }

  hashFunc(inputPtr, inputLength, seed, hashPtr);

  let hash = rain.HEAPU8.subarray(hashPtr, hashPtr + hashLength);

  // Return the hash as a hex string
  const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free the memory after use
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
}

export async function rainbowHash(hashSize, seed, input) {
  if (!rain.loaded) {
    await loadRain();
  }
  // Convert the input to a bytes
  const { stringToUTF8, lengthBytesUTF8 } = rain;
  const { wasmExports: { malloc: _malloc, free: _free } } = rain;

  const hashLength = hashSize / 8;
  const hashPtr = _malloc(hashLength);

  let inputPtr;
  let inputLength;

  if (typeof input == "string") {
    inputLength = lengthBytesUTF8(input);
    inputPtr = _malloc(inputLength);
    stringToUTF8(input, inputPtr, inputLength + 1);
  } else {
    inputLength = input.length;
    inputPtr = _malloc(inputLength);
    rain.HEAPU8.set(input, inputPtr);
  }

  seed = BigInt(seed);

  // Choose the correct hash function based on the hash size
  let hashFunc;

  switch (hashSize) {
    case 64:
      hashFunc = rain._rainbowHash64;
      break;
    case 128:
      hashFunc = rain._rainbowHash128;
      break;
    case 256:
      hashFunc = rain._rainbowHash256;
      break;
    default:
      throw new Error(`Unsupported hash size for rainbow: ${hashSize}`);
  }

  hashFunc(inputPtr, inputLength, seed, hashPtr);

  let hash = rain.HEAPU8.subarray(hashPtr, hashPtr + hashLength);

  // Return the hash as a hex string
  const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free the memory after use
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
}

export async function getFileHeaderInfo(buffer) {
  if (!rain.loaded) {
    await loadRain();
  }
  const { wasmExports: { wasmGetFileHeaderInfo, wasmFree, malloc: _malloc, free: _free }, UTF8ToString } = rain;

  // Allocate memory in WASM for the buffer
  const bufferPtr = _malloc(buffer.length);
  rain.HEAPU8.set(buffer, bufferPtr);

  // Call the WASM function to get header info
  const infoPtr = wasmGetFileHeaderInfo(bufferPtr, buffer.length);

  // Retrieve the string from WASM memory
  const info = UTF8ToString(infoPtr);

  // Free the allocated memory in WASM
  wasmFree(infoPtr);
  _free(bufferPtr);

  return info;
}

async function loadRain() {
  let resolve;
  const pr = new Promise(res => resolve = res);
  import('./../wasm/rain.cjs').then(async x => {
    await untilTrue(() => {
      return !!x?.default?.wasmExports?.memory;
    });
    rain = x.default;
    rain.loaded = true;

    // Wrap the WASM functions using cwrap
    const { cwrap } = rain; // Assuming cwrap is available via rain object

    // Wrap rainstormHash functions
    rain._rainstormHash64 = cwrap('rainstormHash64', null, ['number', 'number', 'bigint', 'number']);
    rain._rainstormHash128 = cwrap('rainstormHash128', null, ['number', 'number', 'bigint', 'number']);
    rain._rainstormHash256 = cwrap('rainstormHash256', null, ['number', 'number', 'bigint', 'number']);
    rain._rainstormHash512 = cwrap('rainstormHash512', null, ['number', 'number', 'bigint', 'number']);

    // Wrap rainbowHash functions
    rain._rainbowHash64 = cwrap('rainbowHash64', null, ['number', 'number', 'bigint', 'number']);
    rain._rainbowHash128 = cwrap('rainbowHash128', null, ['number', 'number', 'bigint', 'number']);
    rain._rainbowHash256 = cwrap('rainbowHash256', null, ['number', 'number', 'bigint', 'number']);

    // Wrap WASM header info functions
    rain.wasmGetFileHeaderInfo = cwrap('wasmGetFileHeaderInfo', 'number', ['array', 'number']);
    rain.wasmFree = cwrap('wasmFree', null, ['number']);

    resolve();
  });
  return pr;
}

async function untilTrue(pred, MAX = 1000, MS_BETWEEN = 50) {
  let resolve, reject, abort = false;
  const oPred = pred;
  pred = async () => {
    try {
      return await oPred();
    } catch (e) {
      console.log(`untilTrue predicate errored: ${e}`, oPred, e);
      reject();
      abort = true;
    }
  };
  const pr = new Promise((res, rej) => (resolve = res, reject = () => {
    rej();
    abort = true;
  }));
  if (await pred()) {
    return resolve();
  }
  setTimeout(async () => {
    let tries = 0;
    while (tries++ < MAX) {
      await sleep(MS_BETWEEN);
      if (await pred()) {
        return resolve();
      }
      if (abort) break;
    }
    return reject();
  }, 0);
  return pr;
}

async function sleep(ms) {
  let resolve;
  const pr = new Promise(res => resolve = res);
  setTimeout(resolve, ms);
  return pr;
}

