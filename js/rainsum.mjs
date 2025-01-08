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
  rain
} from './lib/api.mjs';

const testVectors = [
  "",
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
  "The quick brown fox jumps over the lazy dog",
  "The quick brown fox jumps over the lazy cog",
  "The quick brown fox jumps over the lazy dog.",
  "After the rainstorm comes the rainbow.",
  "@".repeat(64),
];

const argv = yargs(hideBin(process.argv))
  .usage('Usage: $0 [options] [file]')
  .option('mode', {
    alias: 'm',
    description: 'Mode: digest, stream, info, stream-enc, or dec',
    type: 'string',
    choices: ['digest', 'stream', 'info', 'stream-enc', 'dec'],
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


  if ( argv.algorithm.includes('storm') || argv.mode.match(/enc|dec/g) ) {
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
    else if (mode === 'dec') {
      // Handle decryption
      const password = argv.password || '';
      const verbose = argv.verbose;

      // Validate password
      if (!password) {
        throw new Error("Password is required for decryption.");
      }

      // Call the buffer-based decryption function
      const decryptedBuffer = await streamDecryptBuffer(
        buffer,     // Encrypted data buffer
        password,   // Decryption key
        verbose     // Verbose flag
      );


      // Write decrypted data to output file
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
    const errAddress = rain.HEAPU8[e]
    console.log(rain.UTF8ToString(errAddress));
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
    const algorithm = (argv.algorithm || 'storm').toLowerCase();

    if (argv['test-vectors']) {
      // Handle test vectors
      const hashFun = algorithm.endsWith('bow') ? rainbowHash : rainstormHash;
      for (const [expectedHash, message] of testVectors) {
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

