// src/pmc.js
import crypto from 'crypto';
import { weakHash } from './hashFunctions.js';

/**
 * Generate a random nonce using crypto.randomBytes.
 * @param {number} length - Length of the nonce in bytes.
 * @returns {string} Hexadecimal representation of the nonce.
 */
function generateNonce(length = 8) {
  return crypto.randomBytes(length).toString('hex');
}

/**
 * PMC Encryption
 * @param {string} plaintext - The plaintext block to encrypt.
 * @param {string} key - The private key used for encryption.
 * @param {string} mode - Mining mode ('scatter', 'prefix', 'sequence').
 * @returns {Promise<object>} Encrypted block containing nonce and indices.
 */
export async function pmcEncrypt(plaintext, key, mode = 'scatter') {
  const maxAttempts = 1_000_000; // Prevent infinite loops during mining

  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    const nonce = generateNonce();
    const digest = weakHash(key + nonce);

    let indices = [];
    let matched = true;

    for (const char of plaintext) {
      const index = digest.indexOf(char);
      if (index === -1) {
        matched = false;
        break;
      }
      indices.push(index);
    }

    if (matched) {
      return { ciphertext: { nonce, indices }, digest };
    }
  }

  throw new Error('Failed to mine a valid nonce after maximum attempts.');
}

