  const SPLIT = 50;

  // app.js

  // Helper: Wait until rain is loaded (rain.cjs sets globalThis.rain.loaded when done)
  async function ensureRain() {
    while (!globalThis.rain || !globalThis.rain.loaded) {
      await new Promise(res => setTimeout(res, 50));
    }
    return globalThis.rain;
  }

  // --- Wrapper functions for encryption and decryption ---
  // These are modeled similar to your hash functions so that you call them as global functions.
  // Make sure that the underlying rain.cjs setup (the cwraped functions etc.) is already done.
  // --- Encryption and decryption wrappers ---
  // These functions use the emscripten-exported functions from your WASM module,
  // just like your rainstormHash and rainbowHash functions already do.

  async function encrypt({ data, key } = {}) {
    if (!data || !key) {
      throw new TypeError("Both data and key must be provided.");
    }
    // Convert inputs to Uint8Array if they aren’t already.
    const plainData = data instanceof Uint8Array ? data : new TextEncoder().encode(data);
    const keyData = key instanceof Uint8Array ? key : new TextEncoder().encode(key);

    // Get required emscripten exports from globalThis.
    const {
      _wasmBlockEncryptBuffer,
      stringToUTF8,
      lengthBytesUTF8,
      _malloc,
      _free,
      HEAPU8,
      HEAPU32,
      _wasmFreeBuffer  // if defined in your WASM exports
    } = globalThis;

    // --- Set parameters ---
    // These parameters must be adjusted to your expected behavior.
    // Here we use fixed values similar to what your C++ code uses.
    const algorithm = "rainstorm";
    const searchMode = "scatter"; // could also be "prefix", etc.
    const hashBits = 512;
    const seed = 0n;
    const saltStr = ""; // empty salt; if needed, change to a non-empty string
    const blockSize = 9;
    const nonceSize = 9;
    const outputExtension = 512;
    const deterministicNonce = 0; // 0 for false, 1 for true
    const verbose = 0;

    // --- Allocate and set up parameters in WASM memory ---

    // Plaintext data
    const dataPtr = _malloc(plainData.length);
    HEAPU8.set(plainData, dataPtr);

    // Key
    const keyPtr = _malloc(keyData.length);
    HEAPU8.set(keyData, keyPtr);

    // Algorithm (C string)
    const algoLength = lengthBytesUTF8(algorithm);
    const algoPtr = _malloc(algoLength + 1);
    stringToUTF8(algorithm, algoPtr, algoLength + 1);

    // Search mode (C string)
    const searchModeLength = lengthBytesUTF8(searchMode);
    const searchModePtr = _malloc(searchModeLength + 1);
    stringToUTF8(searchMode, searchModePtr, searchModeLength + 1);

    // Salt: if saltStr is nonempty, convert and allocate; otherwise zero.
    let saltPtr, saltLen;
    if (saltStr) {
      const saltData = new TextEncoder().encode(saltStr);
      saltLen = saltData.length;
      saltPtr = _malloc(saltLen);
      HEAPU8.set(saltData, saltPtr);
    } else {
      saltLen = 0;
      saltPtr = 0;
    }

    // Allocate memory for out_len (assume 4 bytes for a 32-bit size_t)
    const outLenPtr = _malloc(4);

    // --- Call encryption ---
    // The C++ signature for wasmBlockEncryptBuffer is:
    //   uint8_t* wasmBlockEncryptBuffer(
    //       const uint8_t* data, size_t data_len,
    //       const char* keyPtr, size_t keyLength,
    //       const char* algorithm, const char* searchMode,
    //       uint32_t hashBits, uint64_t seed,
    //       uint8_t* saltPtr, size_t saltLen,
    //       size_t blockSize, size_t nonceSize,
    //       uint32_t outputExtension,
    //       int deterministicNonce,
    //       int verbose,
    //       size_t* out_len
    //   )
    //
    // Note: For the seed parameter, if your function expects a 64-bit number,
    // you may need to convert the BigInt seed appropriately.
    const resultPtr = _wasmBlockEncryptBuffer(
      dataPtr, plainData.length,
      keyPtr, keyData.length,
      algoPtr, searchModePtr,
      hashBits,
      seed,  
      saltPtr, saltLen,
      blockSize, nonceSize,
      outputExtension,
      deterministicNonce,
      verbose,
      outLenPtr
    );

    // Read the length of the result from WASM memory.
    const resultLen = HEAPU32[outLenPtr >> 2];

    // Copy the encrypted data out of WASM memory.
    const resultView = new Uint8Array(HEAPU8.buffer, resultPtr, resultLen);
    const encryptedData = new Uint8Array(resultView); // make a copy

    // --- Free allocated WASM memory ---
    _free(dataPtr);
    _free(keyPtr);
    _free(algoPtr);
    _free(searchModePtr);
    if (saltPtr) _free(saltPtr);
    _free(outLenPtr);
    if (_wasmFreeBuffer) {
      _wasmFreeBuffer(resultPtr);
    }

    // Return the encrypted data.
    return encryptedData;
  }

  async function decrypt({ data, key, verbose = false } = {}) {
    if (!data || !key) {
      throw new TypeError("Both data and key must be provided.");
    }

    const {
      _wasmBlockDecryptBuffer,
      stringToUTF8,
      lengthBytesUTF8,
      _malloc,
      _free,
      HEAPU8,
      HEAPU32,
      _wasmFreeBuffer
    } = globalThis;

    // Ensure data and key are Uint8Arrays.
    const cipherData = data instanceof Uint8Array ? data : new TextEncoder().encode(data);
    const keyData = key instanceof Uint8Array ? key : new TextEncoder().encode(key);

    // Allocate memory for the encrypted input.
    const dataPtr = _malloc(cipherData.length);
    HEAPU8.set(cipherData, dataPtr);

    // Allocate memory for the key.
    const keyPtr = _malloc(keyData.length);
    HEAPU8.set(keyData, keyPtr);

    // Allocate memory for output pointers:
    // outBuffer pointer (4 bytes for an address) and outBuffer size (4 bytes).
    const outBufferPtrPtr = _malloc(4);
    const outBufferSizePtr = _malloc(4);

    // Call the decryption function.
    // C++ signature for wasmBlockDecryptBuffer:
    //   void wasmBlockDecryptBuffer(
    //       const uint8_t* inBufferPtr, size_t inBufferSize,
    //       const char* keyPtr, size_t keyLength,
    //       uint8_t** outBufferPtr, size_t* outBufferSizePtr,
    //       int verbose
    //   )
    _wasmBlockDecryptBuffer(
      dataPtr, cipherData.length,
      keyPtr, keyData.length,
      outBufferPtrPtr,
      outBufferSizePtr,
      verbose ? 1 : 0
    );

    // Retrieve the output pointer and size.
    const outBufferPtr = HEAPU32[outBufferPtrPtr >> 2];
    const outBufferSize = HEAPU32[outBufferSizePtr >> 2];

    if (!outBufferPtr || !outBufferSize) {
      _free(dataPtr);
      _free(keyPtr);
      _free(outBufferPtrPtr);
      _free(outBufferSizePtr);
      throw new Error("Decryption failed or returned empty buffer.");
    }

    // Copy the decrypted data.
    const decryptedView = new Uint8Array(HEAPU8.buffer, outBufferPtr, outBufferSize);
    const decryptedData = new Uint8Array(decryptedView); // make a copy

    // Free WASM memory allocations.
    _free(dataPtr);
    _free(keyPtr);
    _free(outBufferPtrPtr);
    _free(outBufferSizePtr);
    if (_wasmFreeBuffer) {
      _wasmFreeBuffer(outBufferPtr);
    }

    return decryptedData;
  }


  // Helper functions for Base64-URL encoding/decoding

  function base64UrlEncode(u8arr) {
    let binary = "";
    for (let i = 0; i < u8arr.byteLength; i++) {
      binary += String.fromCharCode(u8arr[i]);
    }
    let base64 = btoa(binary);
    // Convert base64 to base64url by replacing characters and stripping padding
    return base64.replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
  }

  function base64UrlDecode(str) {
    // Convert base64url to base64
    str = str.replace(/-/g, "+").replace(/_/g, "/");
    // Pad the string with '=' so that its length is a multiple of 4
    while (str.length % 4) {
      str += "=";
    }
    const binary = atob(str);
    const u8arr = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) {
      u8arr[i] = binary.charCodeAt(i);
    }
    return u8arr;
  }

  // --- Wrapper API ---
  //
  // Underlying encrypt({data, key}) and decrypt({data, key, verbose}) functions
  // (already built on the emcc exports) return Uint8Arrays.
  // We wrap these functions to accept and return either strings (by default)
  // or raw Uint8Array (when as = 'bin').

  async function rainEncrypt(data, key, as = 'string') {
    // If data/key are strings, encode them as UTF-8.
    const plainData =
      typeof data === "string" ? new TextEncoder().encode(data) : data;
    const keyData =
      typeof key === "string" ? new TextEncoder().encode(key) : key;

    // Call your underlying encrypt; it returns a Uint8Array.
    const encrypted = await encrypt({ data: plainData, key: keyData });

    // By default, return a base64url encoded string.
    if (as === "string") {
      return base64UrlEncode(encrypted);
    } else {
      return encrypted;
    }
  }

  async function rainDecrypt(data, key, as = "string") {
    // If the provided data is a string, assume it is base64url encoded.
    const cipherData =
      typeof data === "string" ? base64UrlDecode(data) : data;
    const keyData =
      typeof key === "string" ? new TextEncoder().encode(key) : key;

    // Call your underlying decrypt; this returns a Uint8Array.
    const decrypted = await decrypt({ data: cipherData, key: keyData });

    if (as === "string") {
      try {
        // Try to decode the Uint8Array as a UTF-8 string.
        return new TextDecoder().decode(decrypted);
      } catch (e) {
        console.warn("Text decoding failed; returning raw Uint8Array.");
        return decrypted;
      }
    } else {
      return decrypted;
    }
  }

  // --- Expose functions to global scope ---
  Object.assign(globalThis, {
    rainEncrypt,
    rainDecrypt,
    // You may still have your older functions attached as well.
  });


  // --- Expose functions to the global scope ---
  // This is similar to how you already assign testVectors, rainstormHash, and rainbowHash.
  Object.assign(globalThis, {
    encrypt,
    decrypt,
    // You may also reassign testVectors, rainstormHash, rainbowHash if needed.
    // For example:
    // testVectors, rainstormHash, rainbowHash
  });

  // --- The rest of your application code (hash, chain, nonceInc, etc.) remains unchanged ---
  //
  // For example, your existing hash() function may continue calling
  // globalThis[algo](size, seed, input) where algo might be one of
  // 'rainstormHash' or 'rainbowHash'. Just be sure that those remain assigned
  // (likely already done in your rain.cjs setup).
  //
  // Additional helper functions and UI code follow...
  //
  // Example helper for converting between hex strings and Uint8Arrays:
  function hexToU8Array(hex) {
    if (hex.length % 2 !== 0) {
      throw new Error('Invalid hex string');
    }
    const out = new Uint8Array(hex.length / 2);
    for (let i = 0; i < hex.length; i += 2) {
      out[i >> 1] = parseInt(hex.slice(i, i + 2), 16);
    }
    return out;
  }

  function concatU8Arrays(a, b) {
    const c = new Uint8Array(a.length + b.length);
    c.set(a, 0);
    c.set(b, a.length);
    return c;
  }

  function sleep(ms) {
    return new Promise(res => setTimeout(res, ms));
  }

  function nextAnimationFrame() {
    return new Promise(res => requestAnimationFrame(res));
  }

  const rapidTask = new Set([
    'chain',
    'nonceInc',
    'nonceRand',
  ]);

  const STORM_TV = [
    [ "340b44c7eee5a41f118273c6e1ec519247fa2075266423943dc86b0c8e3cceb9", "" ],
    [ "1d28374505a26fd62d688d6f67c5f99fa37f7a5dd9d534ada4cfd57a7b5e8040", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "bf8f3eb749b73705dfb9f2319e0c07a2ee4b2ae5cc36e8a08dbd2bfef7daa4e6", "The quick brown fox jumps over the lazy dog" ],
    [ "1699b38337f3eaee6043a289c365b0fa11c185dbf40601287b12c2eea74b8794", "The quick brown fox jumps over the lazy cog" ],
    [ "e39abbb45b5f0a767bb500b6a7beaaf63d1455b820f33b0239061d3049ca5e3e", "The quick brown fox jumps over the lazy dog." ],
    [ "36d61e73eaf284e20ed3de2962b4958a87b3bdab8994d7c68a3972a33529beb1", "After the rainstorm comes the rainbow." ],
    [ "0bb06835033bc5bd86ec26613a135b1abe05b3a35a3a0195ae26b36771581c53", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ],
  ];

  const BOW_TV = [
    [ "91fc76841e1431f6d58871e4c981fb37e3c0ac0f9f141c3e99b78f46c727c454", "" ],
    [ "5440efa52854703021a894a61ad25690515347014a9723880ed86854ecf2247f", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "9c72cf9f50d0f3145ad0f45cf97c09afa6f7555562358dc15c4f4bb14cf2aa85", "The quick brown fox jumps over the lazy dog" ],
    [ "d75f220dc093a669b235ae713a8ce0f7ff33a8c40357bd66f89e885d7fbcb1c9", "The quick brown fox jumps over the lazy cog" ],
    [ "6af5f348167fe18ca480b952e4ce7ed05de4bc25bdfd68a2929b7025ad784280", "The quick brown fox jumps over the lazy dog." ],
    [ "c43f32b0730fefc3a97acfbed163c649744e5a167c681bd561da15d2385425cf", "After the rainstorm comes the rainbow." ],
    [ "49b4f0ad1df0e644296592dfa55d4ad6243e746b6ffe4ece343c41102a2c7507", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ],
  ];

  class HashError extends Error {}
  let lastTask;
  let inHash = false;

  window.onunhandledrejection = (...x) => {
    inHash = false;
    if ( x[0].reason instanceof HashError ) {
      alert(x[0].reason || x[0] + '\n\n\t' + JSON.stringify(x, null, 2));
    }
    return false;
  };

  Object.assign(globalThis, {
    testVectors, 
    rainstormHash,
    rainbowHash,
    hash
  });
  
  async function hash(submission, form, firstRun = false) {
    try {
      if ( inHash ) return;
      inHash = true;
      submission?.preventDefault?.();
      submission?.stopPropagation?.();
      const task = submission.submitter ? submission.submitter.value : 'hash';
      const hashrateEl = document.getElementById('hashrate');

      if ( submission.submitter ) {
        submission.submitter.disabled = true;
      }

      if (!firstRun && rapidTask.has(task)) {
        setTimeout(() => {
          scrollIntoView(form.fileInput);
        }, 50);
      }

      if ( task == 'test' ) {
        if (rapidTask.has(lastTask)) {
          form.input.value = '';
        }
        lastTask = task;
        form.output.value = await testVectors();
      } else if ( task == 'hash' ) {
        if (rapidTask.has(lastTask)) {
          form.input.value = '';
        }
        lastTask = task;
        const algo = form.algo.value;
        const size = parseInt(form.size.value);
        const seed = BigInt(form.seed.value);
        const inputType = submission.target === form.input ? 'text' : 
            form.fileInput.files.length > 0 ? 'file' : 
        'text';
        if ( inputType == 'file' ) {
          form.input.value = "";
          const reader = new FileReader();
          reader.onload = async (event) => {
            let fileContent = event.target.result;
            if ( fileContent instanceof ArrayBuffer ) {
              fileContent = new Uint8Array(fileContent);
            }
            const hashValue = await globalThis[algo](size, seed, fileContent);
            form.output.value = hashValue;
            if ( fileContent.length < 102400 ) {
              setTimeout(async () => {
                form.input.value = new TextDecoder().decode(fileContent);
                await sleep(100);
                inHash = false;
              }, 500);
            } else {
              inHash = false;
            }
          };
          reader.readAsArrayBuffer(form.fileInput.files[0]);
          return false;
        } else {
          if ( form.fileInput.files.length > 0 ) {
            form.fileInput.value = "";
          }
          const input = form.input.value;
          const hashValue = await globalThis[algo](size, seed, input);
          form.output.value = hashValue;
        }
      } else if (task === 'chain') {
        if (rapidTask.has(lastTask)) {
          form.input.value = '';
        }
        lastTask = task;
        const algo = form.algo.value;
        const size = parseInt(form.size.value);
        const seed = BigInt(form.seed.value);
        submission.submitter.innerText = 'Chain Mining...';

        await requestAnimationFrame(() => {}); // just to yield

        const mp = form.mp.value;
        let count = 0;
        const start = Date.now();
        let lastTime = start;

        // Convert existing input to bytes
        let inputBytes = new Uint8Array();
        if (form.input.value) {
          inputBytes = new TextEncoder().encode(form.input.value);
        }

        // Start hashing
        let outputHex = await globalThis[algo](size, seed, inputBytes);
        form.output.value = outputHex;

        while (!outputHex.startsWith(mp)) {
          // Convert outputHex -> bytes
          const outputBytes = hexToU8Array(outputHex);
          // Append
          inputBytes = concatU8Arrays(inputBytes, outputBytes);

          // Next hash
          outputHex = await globalThis[algo](size, seed, inputBytes);
          form.output.value = outputHex;
          count++;

          if (count % SPLIT === 0) {
            const now = Date.now();
            const elapsed = (now - lastTime) / 1000;
            const hps = SPLIT / elapsed;
            hashrateEl.innerText = formatHashRate(hps);
            lastTime = now;
            await nextAnimationFrame();
          }
        }

        // Found
        form.output.value = outputHex;
        // Set input to the final chain (or just the final input bytes in hex, your call):
        // For memory reasons, let's just put the final input in hex:
        form.input.value = Array.from(inputBytes)
          .map((x) => x.toString(16).padStart(2, '0'))
          .join('');

        const end = Date.now();
        const duration = ((end - start) / 1000).toFixed(3);
        submission.submitter.innerText = `Found after ${count} hashes in ${duration}s.`;
      } else if (task === 'nonceInc') {
        if (rapidTask.has(lastTask)) {
          form.input.value = '';
        }
        lastTask = task;
        const algo = form.algo.value;
        const size = parseInt(form.size.value);
        const seed = BigInt(form.seed.value);
        submission.submitter.innerText = 'Nonce Inc...';
          await nextAnimationFrame();

        const mp = form.mp.value;
        const baseInput = form.input.value || "";

        let nonce = 0n;
        let count = 0;
        const start = Date.now();
        let lastTime = start;
        let outputHex = "";

        while (true) {
          const thisInput = baseInput + nonce.toString();
          outputHex = await globalThis[algo](size, seed, thisInput);
          count++;

          if (outputHex.startsWith(mp)) {
            form.output.value = outputHex;
            // put final winning input in the form input
            form.input.value = thisInput;
            break;
          }
          nonce++;

          if (count % SPLIT === 0) {
            const now = Date.now();
            const elapsed = (now - lastTime) / 1000;
            const hps = SPLIT / elapsed;
            hashrateEl.innerText = formatHashRate(hps);
            lastTime = now;
            // Show intermediate output
            form.output.value = outputHex;
            await nextAnimationFrame();
          }
        }

        const end = Date.now();
        const duration = ((end - start) / 1000).toFixed(3);
        submission.submitter.innerText = `Found after ${count} hashes in ${duration}s. (Nonce = ${nonce - 1n})`;
      } else if (task === 'nonceRand') {
        if (rapidTask.has(lastTask)) {
          form.input.value = '';
        }
        lastTask = task;
        const algo = form.algo.value;
        const size = parseInt(form.size.value);
        const seed = BigInt(form.seed.value);
        submission.submitter.innerText = 'Nonce Rand...';
        await nextAnimationFrame();

        const mp = form.mp.value;
        const baseInput = form.input.value || "";

        let count = 0;
        const start = Date.now();
        let lastTime = start;
        let outputHex = "";

        // Helper to get random bytes as a hex string
        function randomHex(len = 128) {
          const buf = new Uint8Array(len);
          crypto.getRandomValues(buf);
          return Array.from(buf)
            .map((b) => b.toString(16).padStart(2, '0'))
            .join('');
        }

        while (true) {
          const randPart = randomHex(); 
          const thisInput = baseInput + randPart;
          outputHex = await globalThis[algo](size, seed, thisInput);
          count++;

          if (outputHex.startsWith(mp)) {
            form.output.value = outputHex;
            // put final input in the form input
            form.input.value = thisInput;
            break;
          }

          if (count % SPLIT === 0) {
            const now = Date.now();
            const elapsed = (now - lastTime) / 1000;
            const hps = SPLIT / elapsed;
            hashrateEl.innerText = formatHashRate(hps);
            lastTime = now;
            // show intermediate
            form.output.value = outputHex;
            await nextAnimationFrame();
          }
        }

        const end = Date.now();
        const duration = ((end - start) / 1000).toFixed(3);
        submission.submitter.innerText = `Found after ${count} hashes in ${duration}s. (Random mode)`;
      } else {
        if (rapidTask.has(lastTask)) {
          form.input.value = '';
        }
        lastTask = task;
        alert(`Unknown task: ${task}`);
      }
      inHash = false;
      if ( submission.submitter ) {
        submission.submitter.disabled = false;
      }
      return false;
    } catch(e) {
      console.error(e);
      return false;
    }
  }

  function isMobileDevice() {
    return (typeof window.orientation !== "undefined") || (navigator.userAgent.indexOf('IEMobile') !== -1);
  };

  function formatHashRate(hps) {
    if (hps >= 1e6) {
      return (hps / 1e6).toFixed(2) + ' MH/s';
    } else if (hps >= 1e3) {
      return (hps / 1e3).toFixed(2) + ' KH/s';
    } else {
      return hps.toFixed(2) + ' H/s';
    }
  }

  function scrollIntoView(el) {
    if (el) {
      el.scrollIntoView({ behavior: 'smooth', block: 'start' });
    }
  }

  async function testVectors() {
    let comment;
    let s = '';

    s += (`Rainstorm test vectors:\n`);
    for( const [expectedHash, message] of STORM_TV ) {
      const calculatedHash = await rainstormHash(256, 0, message);
      if ( calculatedHash !== expectedHash ) {
        comment = "MISMATCH!";
        console.error(`Expected: ${expectedHash}, but got: ${calculatedHash}`);
      } else {
        comment = "";
      }
      s += (`${calculatedHash} "${message}" ${comment}\n`);
    }

    s += (`Rainbow test vectors:\n`);
    for( const [expectedHash, message] of BOW_TV ) {
      const calculatedHash = await rainbowHash(256, 0, message);
      if ( calculatedHash !== expectedHash ) {
        comment = "MISMATCH!";
        console.error(`Expected: ${expectedHash}, but got: ${calculatedHash}`);
      } else {
        comment = "";
      }
      s += (`${calculatedHash} "${message}" ${comment}\n`);
    }

    return s;
  }

  async function rainstormHash(hashSize, seed, input) {
    const {stringToUTF8, lengthBytesUTF8, _malloc, _free} = globalThis;

    const hashLength = hashSize/8;
    const hashPtr = _malloc(hashLength);

    let inputPtr;
    let inputLength;

    if ( typeof input == "string" ) {
      inputLength = lengthBytesUTF8(input);
      inputPtr = _malloc(inputLength);
      stringToUTF8(input, inputPtr, inputLength + 1);
    } else {
      inputLength = input.length;
      inputPtr = _malloc(inputLength);
      HEAPU8.set(input, inputPtr);
    }

    seed = BigInt(seed);

    // Choose the correct hash function based on the hash size
    let hashFunc;

    switch (hashSize) {
      case 64:
          hashFunc = _rainstormHash64;
          break;
      case 128:
          hashFunc = _rainstormHash128;
          break;
      case 256:
          hashFunc = _rainstormHash256;
          break;
      case 512:
          hashFunc = _rainstormHash512;
          break;
      default:
          throw new Error(`Unsupported hash size for rainstorm: ${hashSize}`);
    }

    hashFunc(inputPtr, inputLength, seed, hashPtr);

    let hash = HEAPU8.subarray(hashPtr, hashPtr + hashLength);

    // Return the hash as a Uint8Array
    const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

    // Free the memory after use
    _free(hashPtr);
    _free(inputPtr);

    return hashHex;
  }

  async function rainbowHash(hashSize, seed, input) {
    const {stringToUTF8, lengthBytesUTF8, _malloc, _free} = globalThis;

    const hashLength = hashSize/8;
    const hashPtr = _malloc(hashLength);

    let inputPtr;
    let inputLength;

    if ( typeof input == "string" ) {
      inputLength = lengthBytesUTF8(input);
      inputPtr = _malloc(inputLength);
      stringToUTF8(input, inputPtr, inputLength + 1);
    } else {
      inputLength = input.length;
      inputPtr = _malloc(inputLength);
      HEAPU8.set(input, inputPtr);
    }

    seed = BigInt(seed);

    // Choose the correct hash function based on the hash size
    let hashFunc;

    switch (hashSize) {
        case 64:
            hashFunc = _rainbowHash64;
            break;
        case 128:
            hashFunc = _rainbowHash128;
            break;
        case 256:
            hashFunc = _rainbowHash256;
            break;
        default:
            throw new HashError(`Unsupported hash size for rainbow: ${hashSize}`);
    }

    hashFunc(inputPtr, inputLength, seed, hashPtr);

    let hash = HEAPU8.subarray(hashPtr, hashPtr + hashLength);

    // Return the hash as a Uint8Array
    const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

    // Free the memory after use
    _free(hashPtr);
    _free(inputPtr);

    return hashHex;
  }


