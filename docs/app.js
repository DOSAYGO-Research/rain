  const SPLIT = 50;
  const sleep = ms => new Promise(res => setTimeout(res, ms));
  const nextAnimationFrame = () => new Promise(res => requestAnimationFrame(res));
  const rapidTask = new Set([
    'chain',
    'nonceInc',
    'nonceRand',
  ]);

  const STORM_TV = [
    [ "0ec74bfcbbc1e74f7e3f6adc47dc267644a2071f2d8f4fc931adb96b864c0a5a", "" ],
    [ "379bd2823607188d1a2f8c5621feda9002dc8ff1f0cc9902d55d6c99a6488240", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "600e741274ca196710064e3d8e892bebce5c9ec47944dbf0f56608a5cb21748a", "The quick brown fox jumps over the lazy dog" ],
    [ "cfb961820b823c889e03ca79d130eb2d919a38f947e68745349a73e2f43f7392", "The quick brown fox jumps over the lazy cog" ],
    [ "9161d399ef638b837b821631847aa6603edb66ad16e14e25f9f96d119bebca1b", "The quick brown fox jumps over the lazy dog." ],
    [ "eaf60e7a4817e110d21b5d7888e0993f7ce4c4317ad168e47370cb67d053e41a", "After the rainstorm comes the rainbow." ],
    [ "e2490c856a9f5086a896d2eba9b640e38c8ac8c234e769fb7b8ddd9f112a1727", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ]
  ];

  const BOW_TV = [
    [ "9af7f7b2faaf87e9da4fe493916a78567ec2284018028a5df78968e351cc6dda", "" ],
    [ "4bb4b7dbc4bd78f011c9f41293564abb79f12da16bac056c98aa68fe1c3eed8f", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "f652f20dbd02240238372c8eace0215549a02552bb2eac31bf802fbfa58f5546", "The quick brown fox jumps over the lazy dog" ],
    [ "da30e89eab4bd6300211d53d9edd5a8bb523528b722b7db46e8876550cdc95f7", "The quick brown fox jumps over the lazy cog" ],
    [ "b669467111c6fda5b5aadb8c290ef033e66be5241276a199cb10fc442c34cbe7", "The quick brown fox jumps over the lazy dog." ],
    [ "f7f217751f5940df46fafd1b1edbd1fc8d1398d3bac00385582cefce03016ab2", "After the rainstorm comes the rainbow." ],
    [ "5b6217ce8fff616a75207982644a8dbbe5f235370e9178b801f329eac511c8f6", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ]
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
