  const SPLIT = 50;
  const sleep = ms => new Promise(res => setTimeout(res, ms));
  const nextAnimationFrame = () => new Promise(res => requestAnimationFrame(res));
  const rapidTask = new Set([
    'chain',
    'nonceInc',
    'nonceRand',
  ]);

  const STORM_TV = [
    [ "e3ea5f8885f7bb16468d08c578f0e7cc15febd31c27e323a79ef87c35756ce1e", "" ], 
    [ "9e07ce365903116b62ac3ac0a033167853853074313f443d5b372f0225eede50", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "f88600f4b65211a95c6817d0840e0fc2d422883ddf310f29fa8d4cbfda962626", "The quick brown fox jumps over the lazy dog" ],
    [ "ec05208dd1fbf47b9539a761af723612eaa810762ab7a77b715fcfb3bf44f04a", "The quick brown fox jumps over the lazy cog" ], 
    [ "822578f80d46184a674a6069486b4594053490de8ddf343cc1706418e527bec8", "The quick brown fox jumps over the lazy dog." ], 
    [ "410427b981efa6ef884cd1f3d812c880bc7a37abc7450dd62803a4098f28d0f1", "After the rainstorm comes the rainbow." ], 
    [ "47b5d8cb1df8d81ed23689936d2edaa7bd5c48f5bc463600a4d7a56342ac80b9", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ],
  ];

  const BOW_TV = [
    [ "b735f3165b474cf1a824a63ba18c7d087353e778b6d38bd1c26f7b027c6980d9", "" ], 
    [ "c7343ac7ee1e4990b55227b0182c41e9a6bbc295a17e2194d4e0081124657c3c", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "53efdb8f046dba30523e9004fce7d194fdf6c79a59d6e3687907652e38e53123", "The quick brown fox jumps over the lazy dog" ], 
    [ "95a3515641473aa3726bcc5f454c658bfc9b714736f3ffa8b347807775c2078e", "The quick brown fox jumps over the lazy cog" ], 
    [ "f27c10f32ae243afea08dfb15e0c86c0b601792d1cd195ca651fe5394c56f200", "The quick brown fox jumps over the lazy dog." ], 
    [ "e21780122142956ff99d560069a123b75d014f0b110d307d9b23d79f58ebeb29", "After the rainstorm comes the rainbow." ], 
    [ "a46a9e5cba400ed3e1deec852fb0667e8acbbcfeb71cf0f3a1901396aaae6e19", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ]
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
  
  async function hash(submission, form) {
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

      if (rapidTask.has(task)) {
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

  function concatU8Arrays(a, b) {
    const c = new Uint8Array(a.length + b.length + 1);
    c.set(a, 0);
    c.set(b, a.length);
    return c;
  }

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
