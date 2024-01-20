let rain = {loaded: false};

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

export async function testVectors() {
  if ( ! rain.loaded ) {
    await loadRain();
  }
  let comment;

  console.log(`Rainstorm test vectors:`);
  for( const [expectedHash, message] of STORM_TV ) {
    const calculatedHash = await rainstormHash(256, 0, message);
    if ( calculatedHash !== expectedHash ) {
      comment = "MISMATCH!";
      console.error(`Expected: ${expectedHash}, but got: ${calculatedHash}`);
    } else {
      comment = "";
    }
    console.log(`${calculatedHash} "${message}" ${comment}`);
  }


  console.log(`Rainbow test vectors:`);
  for( const [expectedHash, message] of BOW_TV ) {
    const calculatedHash = await rainbowHash(256, 0, message);
    if ( calculatedHash !== expectedHash ) {
      comment = "MISMATCH!";
      console.error(`Expected: ${expectedHash}, but got: ${calculatedHash}`);
    } else {
      comment = "";
    }
    console.log(`${calculatedHash} "${message}" ${comment}`);
  }
}

export async function rainstormHash(hashSize, seed, input) {
  if ( ! rain.loaded ) {
    await loadRain();
  }
  // Convert the input to a bytes
  const {stringToUTF8, lengthBytesUTF8} = rain;
  const {wasmExports:{ malloc: _malloc, free:_free}} = rain;

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

  // Return the hash as a Uint8Array
  const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free the memory after use
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
}

export async function rainbowHash(hashSize, seed, input) {
  if ( ! rain.loaded ) {
    await loadRain();
  }
  // Convert the input to a bytes
  const {stringToUTF8, lengthBytesUTF8} = rain;
  const {wasmExports:{ malloc: _malloc, free:_free}} = rain;

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

  // Return the hash as a Uint8Array
  const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free the memory after use
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
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
    } catch(e) {
      console.log(`untilTrue predicate errored: ${e}`, oPred, e);
      reject();
      abort = true;
    }
  };
  const pr = new Promise((res, rej) => (resolve = res, reject = () => {
    rej();
    abort = true;
  }));
  if ( await pred() ) {
    return resolve();
  } 
  setTimeout(async () => {
    let tries = 0;
    while(tries++ < MAX) {
      await sleep(MS_BETWEEN);
      if ( await pred() ) {
        return resolve();
      }
      if ( abort ) break;
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
