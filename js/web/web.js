async function rainstormHash(hashSize, seed, input) {
  //await rainstorm.untilLoaded;

  // Convert the input to a bytes
  const {stringToUTF8, lengthBytesUTF8, _malloc, _free} = rainstorm.module;
  console.log(rainstorm.module);

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
    rainstorm.module.HEAPU8.set(input, inputPtr);
  }

  seed = BigInt(seed);

  // Choose the correct hash function based on the hash size
  let hashFunc;

  switch (hashSize) {
      case 64:
          hashFunc = rainstorm.module._rainstormHash64;
          break;
      case 128:
          hashFunc = rainstorm.module._rainstormHash128;
          break;
      case 256:
          hashFunc = rainstorm.module._rainstormHash256;
          break;
      case 512:
          hashFunc = rainstorm.module._rainstormHash512;
          break;
      default:
          throw new Error(`Unsupported hash size: ${hashSize}`);
  }

  hashFunc(inputPtr, inputLength, seed, hashPtr);

  let hash = rainstorm.module.HEAPU8.subarray(hashPtr, hashPtr + hashLength);

  // Return the hash as a Uint8Array
  const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free the memory after use
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
}

async function rainbowHash(hashSize, seed, input) {
  //await rainbow.untilLoaded;

  // Convert the input to a bytes
  const {stringToUTF8, lengthBytesUTF8, _malloc, _free} = rainbow.module;

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
    rainbow.module.HEAPU8.set(input, inputPtr);
  }

  seed = BigInt(seed);

  // Choose the correct hash function based on the hash size
  let hashFunc;

  switch (hashSize) {
      case 64:
          hashFunc = rainbow.module._rainbowHash64;
          break;
      case 128:
          hashFunc = rainbow.module._rainbowHash128;
          break;
      case 256:
          hashFunc = rainbow.module._rainbowHash256;
          break;
      default:
          throw new Error(`Unsupported hash size: ${hashSize}`);
  }

  hashFunc(inputPtr, inputLength, seed, hashPtr);

  let hash = rainbow.module.HEAPU8.subarray(hashPtr, hashPtr + hashLength);

  // Return the hash as a Uint8Array
  const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

  // Free the memory after use
  _free(hashPtr);
  _free(inputPtr);

  return hashHex;
}

