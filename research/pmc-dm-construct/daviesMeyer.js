// src/daviesMeyer.js
import { pmcEncrypt } from './pmc.js';

/**
 * Davies-Meyer Hash Construction
 * @param {string} block - The input block to hash.
 * @param {string} chainingValue - The current chaining value (initially IV).
 * @param {string} key - The private key for the block cipher.
 * @returns {Promise<string>} The updated chaining value (hash).
 */
export async function daviesMeyer(block, chainingValue, key) {
  const { minedText } = await pmcEncrypt(block, key);
  const dmHash = BigInt('0x' + chainingValue) ^ BigInt('0x' + minedText);
  return dmHash.toString(16).padStart(64, '0'); // Pad to 64 hex chars (32 bytes)
}

