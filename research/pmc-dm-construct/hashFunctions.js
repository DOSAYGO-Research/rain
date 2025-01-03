// src/hashFunctions.js
export function weakHash(input) {
  // A simple non-cryptographic hash function
  let hash = 0;
  for (let i = 0; i < input.length; i++) {
    hash = (hash * 31 + input.charCodeAt(i)) & 0xffffffff;
  }
  const digest = hash.toString(16).padStart(8, '0');
  return digest.repeat(4).slice(0, 32); // Extend to 32 bytes for testing
}

