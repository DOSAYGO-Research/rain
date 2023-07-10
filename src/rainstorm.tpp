/**
  Rainstorm hash function - 1024-bit internal state, 512-bit blocks, up to 512-bit output
  Block-based
  Can also be utilized as an eXtensible Output Function (XOF, keystream generator). 

  https://github.com/dosyago/rain

  Copyright 2023 Cris Stringfellow (and DOSYAGO)
  Rainstorm hash is licensed under Apache-2.0
  
    Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
**/
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "endian.h"

namespace rainstorm {
  // more efficient implementations are welcome! 
  constexpr int ROUNDS = 4;

  // P to W are primes chosen to have excellent avalanche qualities
  // Z are prime bit rotation amounts 
  // CTR_LEFT and CTR_RIGHT are arbitrary round function initializers

  constexpr uint64_t P         = UINT64_C(    0xFFFFFFFFFFFFFFFF) - 58;
  constexpr uint64_t Q         = UINT64_C(  13166748625691186689);
  constexpr uint64_t R         = UINT64_C(   1573836600196043749);
  constexpr uint64_t S         = UINT64_C(   1478582680485693857);
  constexpr uint64_t T         = UINT64_C(   1584163446043636637);
  constexpr uint64_t U         = UINT64_C(   1358537349836140151);
  constexpr uint64_t V         = UINT64_C(   2849285319520710901);
  constexpr uint64_t W         = UINT64_C(   2366157163652459183);
  static const uint64_t K[8]   = {        P, Q, R, S, T, U, V, W};
  static const uint64_t Z[8]   = {17, 19, 23, 29, 31, 37, 41, 53}; 

  constexpr uint64_t CTR_LEFT  = UINT64_C(    0xefcdab8967452301);
  constexpr uint64_t CTR_RIGHT = UINT64_C(    0x1032547698badcfe);

  // Weak function that works with 512-bit (8x64-bit) blocks
  static inline void weakfunc(uint64_t* h, const uint64_t* data, bool left) {
    uint64_t ctr;
    if ( left ) {
      ctr = CTR_LEFT;
      for (int i = 0, j = 1, k = 8; i < 8; i++, j++, k++) {
        h[i] ^= data[i];            // ingest

        h[i] -= K[i];               // add 
        h[i] = ROTR64(h[i], Z[i]);  // rotate

        h[k] ^= h[i];               // xor blit high 512

        ctr += h[i];                // chain
        h[j] -= ctr;                // chain
      }
    } else {
      ctr = CTR_RIGHT;
      for (int i = 8, j = 0, k = 1; i < 16; i++, j++, k++) {
        h[i] ^= data[j];            // ingest

        h[i] -= K[j];               // add
        h[i] = ROTR64(h[i], Z[j]);  // rotate

        h[j] ^= h[i];               // blit low 512

        ctr += h[i];                // chain
        h[(k&7)+8] -= ctr;          // chain
      }
    }
  }

  template <uint32_t hashsize, bool bswap>
  //void newnewhash(const void* in, const size_t len, const seed_t seed, void* out) {
  static void rainstorm(const void* in, const size_t len, const seed_t seed, void* out) {
    const uint8_t * data = (const uint8_t *)in;
    uint64_t h[16] = {
        seed + len + 1,
        seed + len + 2,
        seed + len + 2,
        seed + len + 3,
        seed + len + 5,
        seed + len + 7,
        seed + len + 11,
        seed + len + 13,
        seed + len + 17,
        seed + len + 19,
        seed + len + 23,
        seed + len + 29,
        seed + len + 31,
        seed + len + 37,
        seed + len + 41,
        seed + len + 43
    };

    uint64_t temp[8];
    size_t lenRemaining = len;

    // Process 512-bit blocks
    while (lenRemaining >= 64) {
      for (int i = 0, j = 0; i < 8; ++i, j+= 8) {
        temp[i] = GET_U64<bswap>(data, j);
      }

      for( int i = 0; i < ROUNDS; i++) {
        weakfunc(h, temp, i&1);
      }

      data += 64;
      lenRemaining -= 64;
    }

    // Pad and process any remaining data less than 64 bytes (512 bits)
    memset(temp, (0x80+lenRemaining) & 255, sizeof(temp));
    memcpy(temp, data, lenRemaining);
    temp[lenRemaining >> 3] |= lenRemaining >> (lenRemaining - 56)*8;

    for( int i = 0; i < ROUNDS; i++) {
      weakfunc(h, temp, i&1);
    }

    // Final processing
    for (int i = 0, j = 8; i < 8; i++, j++) {
      h[i] -= h[j];
    }

    if ( hashsize > 64 ) {
      weakfunc(h, temp, true);
    }

    // Output the hash
    for (uint32_t i = 0, j = 0; i < std::min(8, (int)hashsize / 64); i++, j+= 8) {
      PUT_U64<bswap>(h[i], (uint8_t *)out, j);
    }
  }
}
