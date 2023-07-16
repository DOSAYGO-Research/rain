import createRainstormModule from './../wasm/rainstorm.js';

const TV = [
  [ "e3ea5f8885f7bb16468d08c578f0e7cc15febd31c27e323a79ef87c35756ce1e", "" ], 
  [ "9e07ce365903116b62ac3ac0a033167853853074313f443d5b372f0225eede50", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
  [ "f88600f4b65211a95c6817d0840e0fc2d422883ddf310f29fa8d4cbfda962626", "The quick brown fox jumps over the lazy dog" ],
  [ "ec05208dd1fbf47b9539a761af723612eaa810762ab7a77b715fcfb3bf44f04a", "The quick brown fox jumps over the lazy cog" ], 
  [ "822578f80d46184a674a6069486b4594053490de8ddf343cc1706418e527bec8", "The quick brown fox jumps over the lazy dog." ], 
  [ "410427b981efa6ef884cd1f3d812c880bc7a37abc7450dd62803a4098f28d0f1", "After the rainstorm comes the rainbow." ], 
  [ "47b5d8cb1df8d81ed23689936d2edaa7bd5c48f5bc463600a4d7a56342ac80b9", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ]
];

const rainstorm = {
  async _untilLoaded() {
    if ( this.module ) return;
    const module = await createRainstormModule();
    this.module = module;
  },
  get untilLoaded() {
    return this._untilLoaded();
  }
}

testVectors();

async function testVectors() {
  await rainstorm.untilLoaded;
  let comment;

  for( const [hash, message] of TV ) {
    const foundHash = await rainstormHash(256, 0, message);
    if ( foundHash !== hash ) {
      comment = "MISMATCH!";
    } else {
      comment = "";
    }
    console.log(`${foundHash} "${message}" ${comment}`);
  }
}

async function rainstormHash(hashSize, seed, input) {
    // Convert the input to a bytes
    const {stringToUTF8, lengthBytesUTF8} = rainstorm.module;
    const HEAP = rainstorm.module.HEAPU8;

    const hashPtr = 0;
    const hashLength = hashSize/8;
    const inputPtr = 80;
    const inputLength = lengthBytesUTF8(input);

    let data = stringToUTF8(input, inputPtr, inputLength + 1);
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

    //console.log(`Will call hash with args for: ${input}`);
    //console.log({inputPtr, inputLength, seed, hashPtr});
    hashFunc(inputPtr, inputLength, seed, hashPtr);

    let hash = rainstorm.module.HEAPU8.subarray(hashPtr, hashPtr + hashLength);

    // Return the hash as a Uint8Array
    const hashHex = Array.from(new Uint8Array(hash)).map(x => x.toString(16).padStart(2, '0')).join('');

    // zero memory
    HEAP.set(new Uint8Array(inputPtr + inputLength + 10));

    return hashHex;
}

