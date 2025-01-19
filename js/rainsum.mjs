#!/usr/bin/env node
import fs from 'fs';
import process from 'process';
import yargs from 'yargs';
import crypto from 'crypto';
import { hideBin } from 'yargs/helpers';
import {
  rainbowHash,
  rainstormHash,
  getFileHeaderInfo,
  streamEncryptBuffer,
  streamDecryptBuffer,
  blockEncryptBuffer,      // NEW
  blockDecryptBuffer,      // NEW
  rain,
  verifyHMAC
} from './lib/api.mjs';

const testVectors = {
  rainstorm: [
    [ "340b44c7eee5a41f118273c6e1ec519247fa2075266423943dc86b0c8e3cceb9", "" ],
    [ "1d28374505a26fd62d688d6f67c5f99fa37f7a5dd9d534ada4cfd57a7b5e8040", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "bf8f3eb749b73705dfb9f2319e0c07a2ee4b2ae5cc36e8a08dbd2bfef7daa4e6", "The quick brown fox jumps over the lazy dog" ],
    [ "1699b38337f3eaee6043a289c365b0fa11c185dbf40601287b12c2eea74b8794", "The quick brown fox jumps over the lazy cog" ],
    [ "e39abbb45b5f0a767bb500b6a7beaaf63d1455b820f33b0239061d3049ca5e3e", "The quick brown fox jumps over the lazy dog." ],
    [ "36d61e73eaf284e20ed3de2962b4958a87b3bdab8994d7c68a3972a33529beb1", "After the rainstorm comes the rainbow." ],
    [ "0bb06835033bc5bd86ec26613a135b1abe05b3a35a3a0195ae26b36771581c53", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ],
  ],
  rainbow: [
    [ "91fc76841e1431f6d58871e4c981fb37e3c0ac0f9f141c3e99b78f46c727c454", "" ],
    [ "5440efa52854703021a894a61ad25690515347014a9723880ed86854ecf2247f", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "9c72cf9f50d0f3145ad0f45cf97c09afa6f7555562358dc15c4f4bb14cf2aa85", "The quick brown fox jumps over the lazy dog" ],
    [ "d75f220dc093a669b235ae713a8ce0f7ff33a8c40357bd66f89e885d7fbcb1c9", "The quick brown fox jumps over the lazy cog" ],
    [ "6af5f348167fe18ca480b952e4ce7ed05de4bc25bdfd68a2929b7025ad784280", "The quick brown fox jumps over the lazy dog." ],
    [ "c43f32b0730fefc3a97acfbed163c649744e5a167c681bd561da15d2385425cf", "After the rainstorm comes the rainbow." ],
    [ "49b4f0ad1df0e644296592dfa55d4ad6243e746b6ffe4ece343c41102a2c7507", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ],
  ]
};

const argv = yargs(hideBin(process.argv))
  .usage('Usage: $0 [options] [file]')
  .option('mode', {
    alias: 'm',
    description: 'Mode: digest, stream, info, stream-enc, block-enc, or dec',
    type: 'string',
    choices: ['digest', 'stream', 'info', 'stream-enc', 'block-enc', 'dec'],
    default: 'digest',
  })
  .option('algorithm', {
    alias: 'a',
    description: 'Specify the hash algorithm to use (rainbow or rainstorm)',
    type: 'string',
    choices: ['rainbow', 'rainstorm'],
    default: 'rainbow',
  })
  .option('size', {
    alias: 's',
    description: 'Specify the size of the hash in bits (e.g., 64, 128, 256)',
    type: 'number',
    default: 256,
  })
  .option('output-file', {
    alias: 'o',
    description: 'Output file for the hash (or ciphertext/plaintext if enc/dec)',
    type: 'string',
    default: '/dev/stdout',
  })
  .option('seed', {
    alias: 'i',
    description: 'Seed value (64-bit number or string)',
    type: 'string',
  })
  .option('test-vectors', {
    alias: 't',
    description: 'Calculate the hash of the standard test vectors',
    type: 'boolean',
    default: false,
  })
  // Additional options for encryption/decryption
  .option('password', {
    alias: 'P',
    description: 'Password for encryption or decryption',
    type: 'string',
    default: '',
  })
  .option('salt', {
    alias: 'c',
    description: 'Salt value (string) for encryption with stream-enc mode',
    type: 'string',
  })
  .option('output-extension', {
    alias: 'x',
    description: 'Output extension in bytes for stream-enc mode',
    type: 'number',
    demandOption: (argv) => argv.mode.includes('enc'),
    default: 128,
  })
  .option('block-size', {
    alias: 'b',
    description: 'Size of block in bytes',
    type: 'number',
    demandOption: (argv) => argv.mode === 'block-enc',
    default: 6,
  })
  .option('nonce-size', {
    alias: 'n',
    description: 'Nonce size in bytes for block-enc mode',
    type: 'number',
    demandOption: (argv) => argv.mode === 'block-enc',
    default: 9,
  })
  .option('search-mode', {
    alias: 'f',
    description: 'Search mode: prefix, series, scatter',
    type: 'string',
    demandOption: (argv) => argv.mode === 'block-enc',
    default: 'scatter',
  })
  .option('verbose', {
    alias: 'vv',
    description: 'Enable verbose logs for encryption/decryption',
    type: 'boolean',
    default: false,
  })
  .option('deterministic-nonce', {
    alias: 'k',
    description: 'Use a deterministic counter for nonce generation',
    type: 'boolean',
    default: false,
  })
  // NEW: key-material option (exclusive to --password)
  .option('key-material', {
    description: 'Path to a file whose contents will be hashed to derive the encryption/decryption key',
    type: 'string',
    default: ''
  })
  .option('noop', {
    description: 'No op for testing, useful as a placeholder',
    type: 'boolean',
    default: false
  })
  .help()
  .alias('help', 'h')
  .demandCommand(0, 1) // Accepts 0 or 1 positional arguments
  .argv;

if ( argv.mode.match(/enc|dec/g) ) {
  argv.algorithm = 'rainstorm';
  argv.size = 512;
  if ( ! argv.password && ! argv.keyMaterial ) {
    throw new TypeError(`Either password or key-material are required.`);
  }
}

/**
 * Hashes a buffer and writes the result to the output stream.
 * @param {string} mode - The hashing mode ('digest' or 'stream').
 * @param {string} algorithm - The hashing algorithm ('bow' or 'storm').
 * @param {BigInt} seed - The seed value.
 * @param {Buffer} buffer - The input buffer.
 * @param {fs.WriteStream} outputStream - The output stream.
 * @param {number} hashSize - The size of the hash in bits.
 * @param {string} inputName - The name of the input file.
 */
async function hashBuffer(mode, algorithm, seed, buffer, outputStream, hashSize, inputName) {
  try {
    let hash;
    if (algorithm.endsWith('storm')) {
      hash = await rainstormHash(hashSize, seed, buffer);
    } else if (algorithm.endsWith('bow')) {
      hash = await rainbowHash(hashSize, seed, buffer);
    }
    outputStream.write(`${hash} ${inputName}\n`);
  } catch (e) {
    console.log('error', e, e.stack);
  }
}

/**
 * Handles hashing, info retrieval, encryption, or decryption based on the mode.
 * @param {string} mode - The mode ('digest', 'stream', 'info', 'stream-enc', 'block-enc', or 'dec').
 * @param {string} algorithm - The hashing algorithm ('bow' or 'storm').
 * @param {BigInt} seed - The seed value.
 * @param {string} inputPath - The input file path.
 * @param {string} outputPath - The output file path.
 * @param {number} size - The size of the hash in bits.
 */
async function handleMode(mode, algorithm, seed, inputPath, outputPath, size, argv) {
  try {
    let buffer;
    let inputName;

    if (inputPath === '/dev/stdin') {
      // Read from stdin
      buffer = [];
      for await (const chunk of process.stdin) {
        buffer.push(chunk);
      }
      buffer = Buffer.concat(buffer);
      inputName = 'stdin';
    } else {
      // Read from file
      buffer = fs.readFileSync(inputPath);
      inputName = inputPath;
    }

    if (mode === 'info') {
      // Handle info mode
      const info = await getFileHeaderInfo(buffer);
      try {
        const parsedInfo = JSON.parse(info);
        console.log("=== File Header Info ===");
        for (const [key, value] of Object.entries(parsedInfo)) {
          if (typeof value === 'string' && value.startsWith('0x')) {
            if (value.length < 8) {
              parsedInfo[key] = parseInt(value, 16);
            }
          }
        }
        console.log(JSON.stringify(parsedInfo, null, 2));
      } catch (e) {
        console.warn(e);
        console.log("Error parsing header info:", info);
      }
    }
    else if (mode === 'stream-enc') {
      // Handle stream encryption
      const saltStr = argv.salt || '';
      const verbose = argv.verbose;
      const outputExtension = argv['output-extension'] || 128;
      let key;

      // --- Key Material Handling ---
      if (argv['key-material'] && argv['key-material'].length > 0) {
        const keyFileBuffer = fs.readFileSync(argv['key-material']);
        let derivedHex = await rainstormHash(512, BigInt(0), keyFileBuffer);
        key = Buffer.from(derivedHex, 'hex');
        if (verbose) {
          console.log(`[Info] Derived 512-bit key from file: ${argv['key-material']}`);
        }
      } else {
        const password = argv.password || '';
        if (!password) {
          throw new Error("Password is required for stream encryption.");
        }
        key = Buffer.from(password, 'utf8');
      }

      const encryptedBuffer = await streamEncryptBuffer(
        buffer,          // Plain data buffer
        key,             // Encryption key (either derived from key-material or from password)
        Buffer.from(algorithm, 'utf8'), // 'bow' or 'storm'
        size,            // Hash size in bits
        seed,            // Seed as BigInt
        Buffer.from(saltStr, 'utf8'),   // Salt as Buffer
        outputExtension, // Output extension
        verbose          // Verbose flag
      );

      fs.writeFileSync(outputPath, encryptedBuffer);
      if (verbose) {
        console.log(`[stream-enc] Encrypted data written to: ${outputPath}`);
      }
    }
    else if (mode === 'block-enc') {
      // Handle block encryption
      if (argv.searchMode === 'parascatter') {
        throw new TypeError(`parallel scatter mode is not supported in JS/WASM at this time! Use the C++ version for that feature.`);
      }
      let key;
      if (argv['key-material'] && argv['key-material'].length > 0) {
        const keyFileBuffer = fs.readFileSync(argv['key-material']);
        let derivedHex = await rainstormHash(512, BigInt(0), keyFileBuffer);
        key = Buffer.from(derivedHex, 'hex');
        if (argv.verbose) {
          console.log(`[Info] Derived 512-bit key from file: ${argv['key-material']}`);
        }
      } else {
        const password = argv.password || '';
        if (!password) {
          throw new Error("Password is required for block encryption.");
        }
        key = Buffer.from(password, 'utf8');
      }

      const encryptedBuffer = await blockEncryptBuffer(
        buffer,              // Plain data
        key,                 // Encryption key (derived or from password)
        argv.algorithm,
        argv.searchMode,
        argv.size,
        argv.blockSize,
        argv.nonceSize,
        argv.seed,
        argv.salt,
        argv.outputExtension,
        argv.deterministicNonce,
        argv.verbose
      );

      fs.writeFileSync(outputPath, encryptedBuffer);
      if (argv.verbose) {
        console.log(`[block-enc] Block-based encryption written to: ${outputPath}`);
      }
    }
    else if (mode === 'dec') {
      let key;
      const verbose = argv.verbose;
      if (argv['key-material'] && argv['key-material'].length > 0) {
        const keyFileBuffer = fs.readFileSync(argv['key-material']);
        let derivedHex = await rainstormHash(512, BigInt(0), keyFileBuffer);
        key = Buffer.from(derivedHex, 'hex');
        if (verbose) {
          console.log(`[Info] Derived 512-bit key from file: ${argv['key-material']}`);
        }
      } else {
        const password = argv.password || '';
        if (!password) {
          throw new Error("Password is required for decryption.");
        }
        key = Buffer.from(password, 'utf8');
      }

      // --- HMAC Verification Step ---
      // Get header info (as JSON) from the encrypted file.
      const headerInfoStr = await getFileHeaderInfo(buffer);
      let headerJson = {};
      try {
        headerJson = JSON.parse(headerInfoStr);
      } catch (e) {
        console.warn("Error parsing header info in decryption:", e);
      }
      // Extract the stored HMAC (hex string) from the header.
      const storedHMACHex = headerJson.hmac;
      if (!storedHMACHex) {
        throw new Error("No HMAC found in the file header for verification.");
      }
      // Convert the hex string to a Uint8Array.
      const storedHMAC = Uint8Array.from(
        storedHMACHex.match(/.{1,2}/g).map(byte => parseInt(byte, 16))
      );

      // Use WASM to obtain a zeroed header.
      const { malloc: _malloc, free: _free, HEAPU8, wasmSerializeHeader, getValue } = rain.wasmExports;
      const bufPtr = rain._malloc(buffer.length);
      rain.HEAPU8.set(buffer, bufPtr);
      const outHeaderSizePtr = rain._malloc(4);
      const headerPtr = wasmSerializeHeader(bufPtr, buffer.length, outHeaderSizePtr);
      const headerSize = rain.getValue(outHeaderSizePtr, 'i32');
      // Create header view from WASM memory.
      const headerBytes = new Uint8Array(rain.HEAPU8.buffer, headerPtr, headerSize);
      // The ciphertext is the remainder of the file.
      const ciphertextBuffer = buffer.slice(headerSize);
      // Free temporary pointers.
      _free(bufPtr);
      _free(outHeaderSizePtr);

      // Verify the HMAC.
      const isHMACValid = await verifyHMAC(headerBytes, ciphertextBuffer, key, storedHMAC);
      if (!isHMACValid) {
        console.error("[dec] HMAC verification failed! File may be corrupted or tampered with.");
        throw new Error("[dec] HMAC verification failed!");
      } else if (verbose) {
        console.log("[dec] HMAC verification succeeded.");
      }

      // Continue with decryption.
      let decryptedBuffer;
      // Check the header info to decide whether to use block or stream decryption.
      let isBlockMode = false;
      try {
        const headerJson = JSON.parse(headerInfoStr);
        if (headerJson.cipherMode === '0x11') {
          isBlockMode = true;
        }
      } catch (e) { }
      if (isBlockMode) {
        decryptedBuffer = await blockDecryptBuffer(buffer, key);
        if (verbose) {
          console.log(`[dec] Detected block cipher mode, using block decryption.`);
        }
      } else {
        decryptedBuffer = await streamDecryptBuffer(buffer, key, verbose);
        if (verbose) {
          console.log(`[dec] No block cipher header found, using stream decryption.`);
        }
      }
      fs.writeFileSync(outputPath, decryptedBuffer);
      if (verbose) {
        console.log(`[dec] Decrypted data written to: ${outputPath}`);
      }
    }
    else {
      // Handle hashing modes (digest, stream)
      const outputStream = fs.createWriteStream(outputPath, { flags: 'a' });
      outputStream.on('error', err => err.code == "EPIPE" && process.abort());
      await hashBuffer(mode, algorithm, seed, buffer, outputStream, size, inputName);
    }
  } catch (e) {
    console.error('Error:', e);
    console.error(e.stack);
    process.exit(1);
  }
}

/**
 * Entry point of the CLI tool.
 */
async function main() {
  try {
    const algorithm = (argv.algorithm || 'rainstorm').toLowerCase();

    if (argv['test-vectors']) {
      // Handle test vectors
      const hashFun = algorithm.endsWith('bow') ? rainbowHash : rainstormHash;
      for (const [expectedHash, message] of testVectors[algorithm]) {
        const hash = await hashFun(256, BigInt(0), Buffer.from(message));
        let comment = '';
        if (hash !== expectedHash) {
          comment = "MISMATCH!";
          console.error(`Expected: ${expectedHash}, but got: ${hash}`);
        }
        console.log(`${hash} "${message}" ${comment}`);
      }
      return;
    }

    const mode = argv.mode || 'digest';
    const size = argv.size || 256;
    const seedInput = argv.seed;
    let seed;

    if ( seedInput === null || seedInput === undefined ) {
      if ( argv.mode.match(/enc|dec/g) ) {
        seed = crypto.randomBytes(8).readBigInt64BE(0);
      } else {
        seed = 0n;
      }
    } else {
      // Determine if seed is a number or string
      if (/^\d+$/.test(seedInput)) {
        seed = BigInt(seedInput);
      } else {
        // If seed is a string, hash it using rainstormHash to derive a 64-bit seed
        const seedBuffer = Buffer.from(seedInput, 'utf-8');
        const seedHash = await rainstormHash(64, BigInt(0), seedBuffer);
        seed = BigInt(`0x${seedHash.slice(0, 16)}`); // Take first 8 bytes (16 hex characters)
      }
    }

    argv.seed = seed;

    if ( argv.mode.includes('enc') ) {
      if ( argv.salt === null || argv.salt === undefined ) {
        argv.salt = crypto.randomBytes(32);
      } else if ( typeof argv.salt === 'string' && argv.salt.startsWith('0x') ) {
        argv.salt = Buffer.from(argv.salt.slice(2), 'hex');
      } else if (typeof argv.salt === 'string') {
        argv.salt = Buffer.from(argv.salt, 'utf8');
      }
    }

    const inputPath = argv._[0] || '/dev/stdin';
    const outputPath = argv.mode.includes('enc') ? inputPath + '.rc' : 
      argv.mode.includes('dec') ? inputPath + '.dec' : argv.outputFile;

    await handleMode(mode, algorithm, seed, inputPath, outputPath, size, argv);
  } catch (e) {
    console.error('Fatal Error:', e);
    console.error(e.stack);
    process.exit(1);
  }
}

main();

