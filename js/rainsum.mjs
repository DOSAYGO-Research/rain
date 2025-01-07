#!/usr/bin/env node
import fs from 'fs';
import process from 'process';
import yargs from 'yargs';
import { hideBin } from 'yargs/helpers';
import { rainbowHash, rainstormHash, getFileHeaderInfo } from './lib/api.mjs';

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
    description: 'Mode: digest, stream, or info',
    type: 'string',
    choices: ['digest', 'stream', 'info'],
    default: 'digest',
  })
  .option('algorithm', {
    alias: 'a',
    description: 'Specify the hash algorithm to use (bow or storm)',
    type: 'string',
    choices: ['bow', 'storm'],
    default: 'storm',
  })
  .option('size', {
    alias: 's',
    description: 'Specify the size of the hash in bits (e.g., 64, 128, 256)',
    type: 'number',
    default: 256,
  })
  .option('output-file', {
    alias: 'o',
    description: 'Output file for the hash',
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
  .help()
  .alias('help', 'h')
  .demandCommand(0, 1) // Accepts 0 or 1 positional arguments
  .argv;

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
 * Handles hashing or info retrieval based on the mode.
 * @param {string} mode - The mode ('digest', 'stream', or 'info').
 * @param {string} algorithm - The hashing algorithm ('bow' or 'storm').
 * @param {BigInt} seed - The seed value.
 * @param {string} inputPath - The input file path.
 * @param {string} outputPath - The output file path.
 * @param {number} size - The size of the hash in bits.
 */
async function hashAnything(mode, algorithm, seed, inputPath, outputPath, size) {
  try {
    let buffer = [];
    let inputName;
    if (inputPath === '/dev/stdin') {
      for await (const chunk of process.stdin) {
        buffer.push(chunk);
      }
      buffer = Buffer.concat(buffer);
      inputName = 'stdin';
    } else {
      buffer = fs.readFileSync(inputPath);
      inputName = inputPath;
    }

    if (mode === 'info') {
      // Handle info mode
      const info = await getFileHeaderInfo(buffer);
      try {
        const parsedInfo = JSON.parse(info);
        console.log("=== File Header Info ===");
        console.log(JSON.stringify(parsedInfo, null, 2));
      } catch (e) {
        console.log("Error parsing header info:", info);
      }
    } else {
      // Handle hashing modes (digest, stream)
      const outputStream = fs.createWriteStream(outputPath, { flags: 'a' });
      await hashBuffer(mode, algorithm, seed, buffer, outputStream, size, inputName);
    }
  } catch (e) {
    console.log('error', e, e.stack);
  }
}

/**
 * Entry point of the CLI tool.
 */
async function main() {
  try {
    const algorithm = (argv.algorithm || 'storm').toLowerCase();

    if (argv['test-vectors']) {
      const hashFun = algorithm.endsWith('bow') ? rainbowHash : rainstormHash;
      for (const testVector of testVectors) {
        const hash = await hashFun(256, BigInt(0), Buffer.from(testVector));
        console.log(`${hash} "${testVector}"`);
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
    const outputPath = argv['output-file'] || '/dev/stdout';

    await hashAnything(mode, algorithm, seed, inputPath, outputPath, size);
  } catch (e) {
    console.log('error', e, e.stack);
  }
}

main();

