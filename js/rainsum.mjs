#!/usr/bin/env node
import fs from 'fs';
import process from 'process';
import yargs from 'yargs';
import { hideBin } from 'yargs/helpers';
import {
  rainbowHash,
  rainstormHash,
  getFileHeaderInfo,
  streamEncryptBuffer,
  streamDecryptBuffer,
  blockEncryptBuffer,      // NEW
  blockDecryptBuffer,      // NEW
  rain
} from './lib/api.mjs';

const testVectors = {
  rainstorm: [
    [ "0ec74bfcbbc1e74f7e3f6adc47dc267644a2071f2d8f4fc931adb96b864c0a5a", "" ],
    [ "379bd2823607188d1a2f8c5621feda9002dc8ff1f0cc9902d55d6c99a6488240", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "600e741274ca196710064e3d8e892bebce5c9ec47944dbf0f56608a5cb21748a", "The quick brown fox jumps over the lazy dog" ],
    [ "cfb961820b823c889e03ca79d130eb2d919a38f947e68745349a73e2f43f7392", "The quick brown fox jumps over the lazy cog" ],
    [ "9161d399ef638b837b821631847aa6603edb66ad16e14e25f9f96d119bebca1b", "The quick brown fox jumps over the lazy dog." ],
    [ "eaf60e7a4817e110d21b5d7888e0993f7ce4c4317ad168e47370cb67d053e41a", "After the rainstorm comes the rainbow." ],
    [ "e2490c856a9f5086a896d2eba9b640e38c8ac8c234e769fb7b8ddd9f112a1727", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ]
  ],
  rainbow: [
    [ "9af7f7b2faaf87e9da4fe493916a78567ec2284018028a5df78968e351cc6dda", "" ],
    [ "4bb4b7dbc4bd78f011c9f41293564abb79f12da16bac056c98aa68fe1c3eed8f", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ],
    [ "f652f20dbd02240238372c8eace0215549a02552bb2eac31bf802fbfa58f5546", "The quick brown fox jumps over the lazy dog" ],
    [ "da30e89eab4bd6300211d53d9edd5a8bb523528b722b7db46e8876550cdc95f7", "The quick brown fox jumps over the lazy cog" ],
    [ "b669467111c6fda5b5aadb8c290ef033e66be5241276a199cb10fc442c34cbe7", "The quick brown fox jumps over the lazy dog." ],
    [ "f7f217751f5940df46fafd1b1edbd1fc8d1398d3bac00385582cefce03016ab2", "After the rainstorm comes the rainbow." ],
    [ "5b6217ce8fff616a75207982644a8dbbe5f235370e9178b801f329eac511c8f6", "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" ]
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
    default: 'rainstorm',
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
    description: 'Seed value (64-bit number or string)',
    type: 'string',
    default: '0',
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
    demandOption: (argv) => argv.mode === 'stream-enc' || argv.mode === 'dec',
    default: '',
  })
  .option('salt', {
    description: 'Salt value (string) for encryption with stream-enc mode',
    type: 'string',
    demandOption: (argv) => argv.mode === 'stream-enc',
    default: '',
  })
  .option('output-extension', {
    alias: 'x',
    description: 'Output extension in bytes for stream-enc mode',
    type: 'number',
    demandOption: (argv) => argv.mode === 'stream-enc',
    default: 128,
  })
  .option('verbose', {
    alias: 'vv',
    description: 'Enable verbose logs for encryption/decryption',
    type: 'boolean',
    default: false,
  })
  .help()
  .alias('help', 'h')
  .demandCommand(0, 1) // Accepts 0 or 1 positional arguments
  .argv;


  if ( argv.mode.match(/enc|dec/g) ) {
    argv.algorithm = 'rainstorm';
    argv.size = 512;
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
 * @param {string} mode - The mode ('digest', 'stream', 'info', 'stream-enc', or 'dec').
 * @param {string} algorithm - The hashing algorithm ('bow' or 'storm').
 * @param {BigInt} seed - The seed value.
 * @param {string} inputPath - The input file path.
 * @param {string} outputPath - The output file path.
 * @param {number} size - The size of the hash in bits.
 */
async function handleMode(mode, algorithm, seed, inputPath, outputPath, size) {
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
      const password = argv.password || '';
      const saltStr = argv.salt || '';
      const verbose = argv.verbose;
      const outputExtension = argv['output-extension'] || 128;

      // Validate password and salt
      if (!password) {
        throw new Error("Password is required for stream encryption.");
      }

      // Call the buffer-based encryption function
      const encryptedBuffer = await streamEncryptBuffer(
        buffer,                         // Plain data buffer
        Buffer.from(password, 'utf-8'), // Encryption key
        Buffer.from(algorithm, 'utf-8'), // 'bow' or 'storm'
        size,                           // Hash size in bits
        seed,                           // Seed as BigInt
        Buffer.from(saltStr, 'utf-8'),  // Salt as Buffer
        outputExtension,                // Output extension
        verbose                         // Verbose flag
      );

      // Write encrypted data to output file
      fs.writeFileSync(outputPath, encryptedBuffer);
      if (verbose) {
        console.log(`[stream-enc] Encrypted data written to: ${outputPath}`);
      }
    }
    else if (mode === 'block-enc') {
      // (Or you can add more CLI flags for blockSize, nonceSize, searchMode, etc.)
      const password = argv.password || '';
      const verbose = argv.verbose;
      
      // For demonstration, let's just pick some sample defaults
      const blockSize = 6;
      const nonceSize = 8;
      const outputExtension = 128;
      const searchMode = 'scatter';    // or 'prefix', 'sequence', etc.
      const deterministicNonce = false;

      // Validate password and salt
      if (!password) {
        throw new Error("Password is required for stream encryption.");
      }

      // Call the new blockEncryptBuffer
      const encryptedBuffer = await blockEncryptBuffer(
        buffer,                      // Plain data
        password,                    // Password
        algorithm,                   // "rainbow" or "rainstorm"
        size,                        // Hash bits
        seed,                        // 64-bit seed (as BigInt)
        Buffer.from('mysalt'),       // Salt
        outputExtension,
        blockSize,
        nonceSize,
        searchMode,
        deterministicNonce,
        verbose
      );

      fs.writeFileSync(outputPath, encryptedBuffer);
      if (verbose) {
        console.log(`[block-enc] Block-based encryption written to: ${outputPath}`);
      }
    }
    else if (mode === 'dec') {
      const password = argv.password || '';
      const verbose = argv.verbose;
      if (!password) {
        throw new Error("Password is required for decryption.");
      }

      // 1) Check the file header
      const headerInfoStr = await getFileHeaderInfo(buffer);
      let isBlockMode = false;

      try {
        const headerJson = JSON.parse(headerInfoStr);
        // If cipherMode is "0x11", it’s block-mode puzzle
        if (headerJson.cipherMode === '0x11') {
          isBlockMode = true;
        }
      } catch(e) {
        // If it fails, we’ll assume it’s stream-based
      }

      let decryptedBuffer;
      if (isBlockMode) {
        // 2) Decrypt block-based
        decryptedBuffer = await blockDecryptBuffer(
          buffer,
          password
        );
        if (verbose) {
          console.log(`[dec] Detected block cipher mode, using block decryption.`);
        }
      } else {
        // 2) Decrypt stream-based
        decryptedBuffer = await streamDecryptBuffer(
          buffer,
          password,
          verbose
        );
        if (verbose) {
          console.log(`[dec] No block cipher header found, using stream decryption.`);
        }
      }

      // 3) Write decrypted result
      fs.writeFileSync(outputPath, decryptedBuffer);
      if (verbose) {
        console.log(`[dec] Decrypted data written to: ${outputPath}`);
      }
    }
    else {
      // Handle hashing modes (digest, stream)
      const outputStream = fs.createWriteStream(outputPath, { flags: 'a' });
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
    const seedInput = argv['seed'] || '0';
    let seed;

    // Determine if seed is a number or string
    if (/^\d+$/.test(seedInput)) {
      seed = BigInt(seedInput);
    } else {
      // If seed is a string, hash it using rainstormHash to derive a 64-bit seed
      const seedBuffer = Buffer.from(seedInput, 'utf-8');
      const seedHash = await rainstormHash(64, BigInt(0), seedBuffer);
      seed = BigInt(`0x${seedHash.slice(0, 16)}`); // Take first 8 bytes (16 hex characters)
    }

    const inputPath = argv._[0] || '/dev/stdin';
    const outputPath = argv.mode.includes('enc') ? inputPath + '.rc' : 
      argv.mode.includes('dec') ? inputPath + '.dec' : argv.outputFile;

    await handleMode(mode, algorithm, seed, inputPath, outputPath, size);
  } catch (e) {
    console.error('Fatal Error:', e);
    console.error(e.stack);
    process.exit(1);
  }
}

main();

