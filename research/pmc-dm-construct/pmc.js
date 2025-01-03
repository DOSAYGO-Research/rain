// src/pmc.js
import crypto from 'crypto';
import { weakHash1 as weakHash } from './hashFunctions.js';

console.log({weakHash: weakHash.toString()});

/**
 * Generate a random nonce using crypto.randomBytes.
 * @param {number} length - Length of the nonce in bytes.
 * @returns {string} Hexadecimal representation of the nonce.
 */
function generateNonce(length = 8) {
  return crypto.randomBytes(length).toString('hex');
}

/**
 * Split plaintext into blocks of a fixed size.
 * @param {string} plaintext - The plaintext to split.
 * @param {number} blockSize - Size of each block.
 * @returns {string[]} Array of plaintext blocks.
 */
function splitIntoBlocks(plaintext, blockSize) {
  plaintext = Buffer.from(plaintext, 'utf-8');
  const blocks = [];
  for (let i = 0; i < plaintext.length; i += blockSize) {
    blocks.push(plaintext.slice(i, i + blockSize));
  }
  return blocks;
}

/**
 * PMC Encryption with Block Processing
 * @param {string} plaintext - The plaintext to encrypt.
 * @param {string} key - The private key used for encryption.
 * @param {number} blockSize - Size of each plaintext block.
 * @param {string} mode - Mining mode ('scatter', 'prefix', 'sequence').
 * @returns {Promise<object[]>} List of encrypted blocks containing <nonce, indices>.
 */
export async function pmcEncrypt(plaintext, key, blockSize = 3, mode = 'scatter') {
  const maxAttempts = 100000;
  const blocks = splitIntoBlocks(plaintext, blockSize);
  const result = [];

  for (const block of blocks) {
    let mined = false;

    for (let attempt = 0; attempt < maxAttempts; attempt++) {
      const nonce = generateNonce();
      const digest = weakHash(key + nonce);

      let indices = [];
      let matched = true;

      for (const char of block) {
        const index = digest.indexOf(char.toString(16));
        if (index === -1) {
          matched = false;
          break;
        }
        indices.push(index);
      }

      if (matched) {
        result.push({ nonce, indices });
        mined = true;
        break;
      }
    }

    if (!mined) {
      throw new Error(`Failed to mine a valid nonce for block "${block}" after maximum attempts.`);
    }
  }

  return result;
}

