// lib/api.mjs

export let rain = { loaded: false };


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
  const { stringToUTF8, lengthBytesUTF8 } = rain;
  const { malloc: _malloc, free: _free } = rain.wasmExports;

  const hashLength = hashSize / 8;
  const hashPtr = _malloc(hashLength);

  let inputPtr;
  let inputLength;

  if (typeof input === "string") {
    inputLength = lengthBytesUTF8(input);
    inputPtr = _malloc(inputLength);
    stringToUTF8(input, inputPtr, inputLength + 1);
  } else {
    inputLength = input.length;
    inputPtr = _malloc(inputLength);
    rain.HEAPU8.set(input, inputPtr);
  }

  // Select the appropriate hash function based on hashSize
  let hashFunc;
  switch (hashSize) {
    case 64:
      hashFunc = rain.rainstormHash64;
      break;
    case 128:
      hashFunc = rain.rainstormHash128;
      break;
    case 256:
      hashFunc = rain.rainstormHash256;
      break;
    case 512:
      hashFunc = rain.rainstormHash512;
      break;
    default:
      throw new Error(`Unsupported hash size for rainstorm: ${hashSize}`);
  }

  // Call the WASM hash function
  hashFunc(inputPtr, inputLength, seed, hashPtr);

  // Retrieve the hash from WASM memory
  const hash = new Uint8Array(rain.HEAPU8.buffer, hashPtr, hashLength);
  const hashHex = Array.from(hash).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free allocated memory
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
}

export async function rainbowHash(hashSize, seed, input) {
  if (!rain.loaded) {
    await loadRain();
  }
  const { stringToUTF8, lengthBytesUTF8 } = rain;
  const { malloc: _malloc, free: _free } = rain.wasmExports;

  const hashLength = hashSize / 8;
  const hashPtr = _malloc(hashLength);

  let inputPtr;
  let inputLength;

  if (typeof input === "string") {
    inputLength = lengthBytesUTF8(input);
    inputPtr = _malloc(inputLength);
    stringToUTF8(input, inputPtr, inputLength + 1);
  } else {
    inputLength = input.length;
    inputPtr = _malloc(inputLength);
    rain.HEAPU8.set(input, inputPtr);
  }

  // Select the appropriate hash function based on hashSize
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

  // Call the WASM hash function
  hashFunc(inputPtr, inputLength, seed, hashPtr);

  // Retrieve the hash from WASM memory
  const hash = new Uint8Array(rain.HEAPU8.buffer, hashPtr, hashLength);
  const hashHex = Array.from(hash).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free allocated memory
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
}

export async function getFileHeaderInfo(buffer) {
  if (!rain.loaded) {
    await loadRain();
  }
  const { UTF8ToString } = rain;
  const { wasmGetFileHeaderInfo, wasmFree, malloc: _malloc, free: _free } = rain.wasmExports;

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

/**
 * Decrypts a buffer using the WASM buffer-based decryption function.
 * @param {Buffer} encryptedData - The encrypted data [FileHeader + XOR'd data].
 * @param {string} password - User password.
 * @param {boolean} verbose - Verbose flag.
 * @returns {Buffer} - The decrypted plaintext data.
 */
export async function streamDecryptBuffer(
  encryptedData,
  password,
  verbose
) {
  try {
    if (!rain.loaded) {
      await loadRain();
    }

    const { wasmExports: { wasmStreamDecryptBuffer, wasmFreeBuffer, wasmFreeString, malloc: _malloc, free: _free }, HEAPU8, HEAPU32, UTF8ToString } = rain;

    // Allocate memory for encrypted buffer
    const encryptedPtr = _malloc(encryptedData.length);
    HEAPU8.set(encryptedData, encryptedPtr);

    // Allocate memory for password string (+1 for null-terminator)
    const passwordLength = Buffer.byteLength(password, 'utf-8');
    const passwordPtr = _malloc(passwordLength + 1);
    HEAPU8.set(Buffer.from(password, 'utf-8'), passwordPtr);
    HEAPU8[passwordPtr + passwordLength] = 0; // Null-terminate

    // Allocate memory for output pointers (uint8_t** outBufferPtr and size_t* outBufferSizePtr)
    const outBufferPtr = _malloc(4); // Assuming 32-bit pointers; adjust if using 64-bit
    const outBufferSizePtr = _malloc(4); // Assuming 32-bit size_t; adjust if using 64-bit

    // Allocate memory for error message pointer (char** errorPtr)
    const errorPtr = _malloc(4); // Pointer to error message string

    // Debug logs
    if (verbose) {
      console.log({
        encryptedPtr,
        length: encryptedData.length,
        passwordPtr,
        passwordLength,
        verbose,
        outBufferPtr,
        outBufferSizePtr,
        errorPtr
      });
    }

    // Call the WASM decryption function
    wasmStreamDecryptBuffer(
      encryptedPtr,
      encryptedData.length,
      passwordPtr,
      passwordLength,
      verbose ? 1 : 0,
      outBufferPtr,
      outBufferSizePtr,
      errorPtr // Pass the error pointer
    );

    // Retrieve the decrypted buffer pointer and size from WASM memory
    const decryptedPtr = HEAPU8.length >= (outBufferPtr >> 2) + 1 ? HEAPU32[outBufferPtr >> 2] : 0;
    const decryptedSize = HEAPU32[outBufferSizePtr >> 2];
    const errorMessageAddress = HEAPU32[errorPtr >> 2];

    // Read the error message if present
    let errorMessage = null;
    if (errorMessageAddress !== 0) {
      errorMessage = UTF8ToString(errorMessageAddress);
      // Free the error message string after reading
      wasmFreeString(errorMessageAddress);
    }

    // Free allocated memory for error pointer
    _free(errorPtr);

    // Check for errors
    if (!decryptedPtr || !decryptedSize) {
      // If an error message exists, throw it; otherwise, use a generic message
      throw new Error(errorMessage || "Decryption failed or returned empty buffer.");
    }

    // Copy decrypted data from WASM memory to Node.js Buffer
    const decryptedData = Buffer.from(HEAPU8.buffer, decryptedPtr, decryptedSize);

    // Free allocated memory in WASM
    wasmFreeBuffer(decryptedPtr);
    _free(encryptedPtr);
    _free(passwordPtr);
    _free(outBufferPtr);
    _free(outBufferSizePtr);

    // Return the decrypted data
    return decryptedData;
  } catch(e) {
    console.warn("Error", e);
  }
}

/**
 * Encrypts a buffer using the WASM buffer-based encryption function.
 * @param {Buffer} plainData - The plaintext data (already compressed).
 * @param {string} password - User password/IKM.
 * @param {string} algorithm - 'rainbow' or 'rainstorm'.
 * @param {number} hashBits - Hash size in bits.
 * @param {BigInt} seed - 64-bit seed (IV).
 * @param {Buffer} salt - Additional salt.
 * @param {number} outputExtension - Extra bytes of keystream offset.
 * @param {boolean} verbose - Verbose flag.
 * @returns {Buffer} - The encrypted data [FileHeader + XOR'd data].
 */
export async function streamEncryptBuffer(
  plainData,
  password,
  algorithm,
  hashBits,
  seed,
  salt,
  outputExtension,
  verbose
) {
  try {
    if (!rain.loaded) {
      await loadRain();
    }

    const { wasmExports: { wasmStreamEncryptBuffer, wasmFreeBuffer, malloc: _malloc, free: _free }, HEAPU8, HEAPU32 } = rain;

    // Allocate memory for input buffer (plainData)
    const plainDataPtr = _malloc(plainData.length);
    HEAPU8.set(plainData, plainDataPtr);

    // Allocate memory for password string
    const passwordLength = Buffer.byteLength(password, 'utf-8');
    const passwordPtr = _malloc(passwordLength + 1); // +1 for null-terminator
    HEAPU8.set(Buffer.from(password, 'utf-8'), passwordPtr);
    HEAPU8[passwordPtr + passwordLength] = 0; // Null-terminate

    // Allocate memory for algorithm string
    const algorithmLength = Buffer.byteLength(algorithm, 'utf-8');
    const algorithmPtr = _malloc(algorithmLength + 1); // +1 for null-terminator
    HEAPU8.set(Buffer.from(algorithm, 'utf-8'), algorithmPtr);
    HEAPU8[algorithmPtr + algorithmLength] = 0; // Null-terminate

    // Allocate memory for salt
    const saltLen = salt.length;
    const saltPtr = _malloc(saltLen);
    HEAPU8.set(salt, saltPtr);

    // Allocate memory for output pointers (uint8_t** outBufferPtr and size_t* outBufferSizePtr)
    const outBufferPtr = _malloc(4); // Assuming 32-bit pointers; adjust if using 64-bit
    const outBufferSizePtr = _malloc(4); // Assuming 32-bit size_t; adjust if using 64-bit

    // Debug logs
    if (verbose) {
      console.log({
        plainDataPtr,
        length: plainData.length,
        passwordPtr,
        passwordLength,
        algorithmPtr,
        algorithmLength,
        hashBits,
        seed,
        saltPtr,
        saltLen,
        outputExtension,
        verbose,
        outBufferPtr,
        outBufferSizePtr
      });
    }

    // Call the WASM encryption function
    wasmStreamEncryptBuffer(
      plainDataPtr,
      plainData.length,
      passwordPtr,
      passwordLength,
      algorithmPtr,
      algorithmLength,
      hashBits,
      seed,
      saltPtr,
      saltLen,
      outputExtension,
      verbose ? 1 : 0,
      outBufferPtr,
      outBufferSizePtr
    );

    // Retrieve the encrypted buffer pointer and size from WASM memory
    const encryptedPtr = HEAPU32[outBufferPtr >> 2];
    const encryptedSize = HEAPU32[outBufferSizePtr >> 2];

    if (!encryptedPtr || !encryptedSize) {
      throw new Error("Encryption failed or returned empty buffer.");
    }

    // Copy encrypted data from WASM memory to Node.js Buffer
    const encryptedData = Buffer.from(HEAPU8.buffer, encryptedPtr, encryptedSize);

    // Free allocated memory in WASM
    wasmFreeBuffer(encryptedPtr);
    _free(plainDataPtr);
    _free(passwordPtr);
    _free(algorithmPtr);
    _free(saltPtr);
    _free(outBufferPtr);
    _free(outBufferSizePtr);

    return encryptedData;
  } catch (err) {
    console.warn("Wasm error", err);
    throw err; // Re-throw to be handled by higher-level functions
  }
}

// Just the snippet showing the new block-based API wrappers in api.mjs:

/**
 * Encrypts a buffer using the WASM block-based encryption function.
 */
export async function blockEncryptBuffer(
  plainData,
  password,
  algorithm,      // "rainbow" or "rainstorm"
  hashBits,
  seed,           // 64-bit seed (as BigInt or number)
  salt,
  outputExtension,
  blockSize,
  nonceSize,
  searchMode,     // "prefix", "sequence", "scatter", etc.
  deterministicNonce,
  verbose
) {
  if (!rain.loaded) {
    await loadRain();
  }

  const {
    wasmExports: { wasmBlockEncryptBuffer, wasmFreeBuffer, malloc: _malloc, free: _free },
    HEAPU8,
    HEAPU32
  } = rain;

  // Allocate memory for input plaintext
  const plainDataPtr = _malloc(plainData.length);
  HEAPU8.set(plainData, plainDataPtr);

  // Allocate memory for password
  const passwordLength = Buffer.byteLength(password, 'utf-8');
  const passwordPtr = _malloc(passwordLength + 1);
  HEAPU8.set(Buffer.from(password, 'utf-8'), passwordPtr);
  HEAPU8[passwordPtr + passwordLength] = 0;

  // Allocate memory for algorithm
  const algorithmLength = Buffer.byteLength(algorithm, 'utf-8');
  const algorithmPtr = _malloc(algorithmLength + 1);
  HEAPU8.set(Buffer.from(algorithm, 'utf-8'), algorithmPtr);
  HEAPU8[algorithmPtr + algorithmLength] = 0;

  // Allocate memory for salt
  const saltPtr = _malloc(salt.length);
  HEAPU8.set(salt, saltPtr);

  // Allocate memory for output pointers
  const outBufferPtr = _malloc(4);   // Assuming 32-bit
  const outBufferSizePtr = _malloc(4);

  try {
    // Call the WASM function
    wasmBlockEncryptBuffer(
      plainDataPtr, plainData.length,
      passwordPtr, passwordLength,
      algorithmPtr, algorithmLength,
      hashBits,
      seed,
      saltPtr, salt.length,
      outputExtension,
      blockSize,
      nonceSize,
      verbose ? 1 : 0,
      deterministicNonce ? 1 : 0,
      outBufferPtr,
      outBufferSizePtr
    );

    // Grab the result
    const encryptedPtr = HEAPU32[outBufferPtr >> 2];
    const encryptedSize = HEAPU32[outBufferSizePtr >> 2];

    if (!encryptedPtr || !encryptedSize) {
      throw new Error("Block encryption returned empty buffer.");
    }

    // Copy out the result
    const encryptedData = Buffer.from(HEAPU8.buffer, encryptedPtr, encryptedSize);

    // Free the result buffer
    wasmFreeBuffer(encryptedPtr);

    return encryptedData;
  } finally {
    // Clean up
    _free(plainDataPtr);
    _free(passwordPtr);
    _free(algorithmPtr);
    _free(saltPtr);
    _free(outBufferPtr);
    _free(outBufferSizePtr);
  }
}

/**
 * Decrypts a buffer using the WASM block-based decryption function.
 */
export async function blockDecryptBuffer(
  cipherData,
  password
) {
  if (!rain.loaded) {
    await loadRain();
  }

  const {
    wasmExports: { wasmBlockDecryptBuffer, wasmFreeBuffer, malloc: _malloc, free: _free },
    HEAPU8,
    HEAPU32
  } = rain;

  // Allocate memory for ciphertext
  const cipherPtr = _malloc(cipherData.length);
  HEAPU8.set(cipherData, cipherPtr);

  // Allocate memory for password
  const passwordLength = Buffer.byteLength(password, 'utf-8');
  const passwordPtr = _malloc(passwordLength + 1);
  HEAPU8.set(Buffer.from(password, 'utf-8'), passwordPtr);
  HEAPU8[passwordPtr + passwordLength] = 0;

  // Allocate output pointer
  const outBufferPtr = _malloc(4);  // 32-bit
  const outBufferSizePtr = _malloc(4);

  try {
    // Call the WASM function
    wasmBlockDecryptBuffer(
      cipherPtr,
      cipherData.length,
      passwordPtr,
      passwordLength,
      outBufferPtr,
      outBufferSizePtr
    );

    // Grab the result
    const decryptedPtr = HEAPU32[outBufferPtr >> 2];
    const decryptedSize = HEAPU32[outBufferSizePtr >> 2];

    if (!decryptedPtr || !decryptedSize) {
      throw new Error("Block decryption returned empty buffer.");
    }

    // Copy out the result
    const decryptedData = Buffer.from(HEAPU8.buffer, decryptedPtr, decryptedSize);

    // Free the result buffer
    wasmFreeBuffer(decryptedPtr);

    return decryptedData;
  } finally {
    // Clean up
    _free(cipherPtr);
    _free(passwordPtr);
    _free(outBufferPtr);
    _free(outBufferSizePtr);
  }
}


/**
 * Loads the WASM module and wraps the necessary functions.
 */
async function loadRain() {
  let resolve;
  const pr = new Promise(res => resolve = res);
  import('./../wasm/rain.cjs').then(async x => {
    await untilTrue(() => {
      return !!x?.default?.wasmExports?.memory;
    });
    rain = x.default;
    rain.loaded = true;

    const { cwrap } = rain;

    // Wrap rainstormHash functions
    rain.rainstormHash64 = cwrap('rainstormHash64', null, ['number', 'number', 'bigint', 'number']);
    rain.rainstormHash128 = cwrap('rainstormHash128', null, ['number', 'number', 'bigint', 'number']);
    rain.rainstormHash256 = cwrap('rainstormHash256', null, ['number', 'number', 'bigint', 'number']);
    rain.rainstormHash512 = cwrap('rainstormHash512', null, ['number', 'number', 'bigint', 'number']);

    // Wrap rainbowHash functions
    rain.rainbowHash64 = cwrap('rainbowHash64', null, ['number', 'number', 'bigint', 'number']);
    rain.rainbowHash128 = cwrap('rainbowHash128', null, ['number', 'number', 'bigint', 'number']);
    rain.rainbowHash256 = cwrap('rainbowHash256', null, ['number', 'number', 'bigint', 'number']);

    // Wrap WASM header info functions
    rain.wasmGetFileHeaderInfo = cwrap('wasmGetFileHeaderInfo', 'number', ['number', 'number']);
    rain.wasmFree = cwrap('wasmFree', null, ['number']);

    // Wrap buffer-based encryption/decryption functions with updated signatures
    // Note: Update the argument types to include the new errorPtr parameter
    rain.wasmStreamEncryptBuffer = cwrap('wasmStreamEncryptBuffer', null, [
      'number', 'number', 'number', 'number', 'number', 'number',
      'number', 'number', 'number', 'number',
      'number', 'number'
    ]);
    rain.wasmStreamDecryptBuffer = cwrap('wasmStreamDecryptBuffer', null, [
      'number', 'number', 'number', 'number',
      'number', 'number', 'number', 'number'
    ]);

    // Inside loadRain(), once the wasm is loaded:
    rain.wasmBlockEncryptBuffer = cwrap('wasmBlockEncryptBuffer', null, [
      'number','number','number','number','number','number',
      'number','number','number','number','number','number',
      'number','number','number','number','number','number'
    ]);
    rain.wasmBlockDecryptBuffer = cwrap('wasmBlockDecryptBuffer', null, [
      'number','number','number','number','number','number'
    ]);
 
    rain.wasmFreeBuffer = cwrap('wasmFreeBuffer', null, ['number']);

    // Wrap the new wasmFreeString function
    rain.wasmFreeString = cwrap('wasmFreeString', null, ['number']);

    resolve();
  }).catch(err => {
    console.error('Failed to load WASM module:', err);
    process.exit(1);
  });
  return pr;
}

/**
 * Utility function to wait until a condition is true, polling periodically.
 * @param {Function} pred - The predicate function to evaluate.
 * @param {number} MAX - Maximum number of attempts.
 * @param {number} MS_BETWEEN - Milliseconds between attempts.
 */
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

/**
 * Sleeps for a given number of milliseconds.
 * @param {number} ms - Milliseconds to sleep.
 */
async function sleep(ms) {
  let resolve;
  const pr = new Promise(res => resolve = res);
  setTimeout(resolve, ms);
  return pr;
}


