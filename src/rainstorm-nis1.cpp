#define __STORMVERSION__ "1.5.0-nis1"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

// If you have platform-specific or hashing environment headers, include them here
// #include "Platform.h"
// #include "Hashlib.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define KEEPALIVE
#endif

#include "common.h"

namespace rainstorm_nis1 {
  // Frank's fixes: use static constexpr for these constants
  static constexpr int ROUNDS = 4;
  static constexpr int FINAL_ROUNDS = 2;

  // Primes and rotation amounts chosen for avalanche qualities
  static constexpr uint64_t P = UINT64_C(0xFFFFFFFFFFFFFFFF) - 58;
  static constexpr uint64_t Q = UINT64_C(13166748625691186689);
  static constexpr uint64_t R = UINT64_C(1573836600196043749);
  static constexpr uint64_t S = UINT64_C(1478582680485693857);
  static constexpr uint64_t T = UINT64_C(1584163446043636637);
  static constexpr uint64_t U = UINT64_C(1358537349836140151);
  static constexpr uint64_t V = UINT64_C(2849285319520710901);
  static constexpr uint64_t W = UINT64_C(2366157163652459183);

  static const uint64_t K[8] = { P, Q, R, S, T, U, V, W };
  static const uint64_t Z[8] = { 17, 19, 23, 29, 31, 37, 41, 53 };

  static constexpr uint64_t CTR_LEFT  = UINT64_C(0xefcdab8967452301);
  static constexpr uint64_t CTR_RIGHT = UINT64_C(0x1032547698badcfe);

  static inline void weakfunc(uint64_t* h, const uint64_t* data, bool left) {
    uint64_t ctr;
    if (left) {
      ctr = CTR_LEFT;
      for (int i = 0, j = 1, k = 8; i < 8; i++, j++, k++) {
        h[i] ^= data[i];            // ingest
        h[i] -= K[i];               
        h[i] = ROTR64(h[i], Z[i]);  // rotate

        h[k] ^= h[i];               // xor blit high 512

        ctr += h[i];                
        h[j] -= ctr;                
      }
    } else {
      ctr = CTR_RIGHT;
      for (int i = 8, j = 0, k = 1; i < 16; i++, j++, k++) {
        h[i] ^= data[j];            // ingest
        h[i] -= K[j];               
        h[i] = ROTR64(h[i], Z[j]);  // rotate

        h[j] ^= h[i];               // blit low 512

        ctr += h[i];                
        h[(k & 7) + 8] -= ctr;      
      }
    }
  }

  struct HashState : IHashState {
    uint64_t  h[16];
    seed_t    seed;
    size_t    len;             
    size_t    olen;
    uint32_t  hashsize;
    bool      inner = 0;
    bool      final_block = false;
    bool      finalized = false;

    static HashState initialize(const seed_t seed, size_t olen, uint32_t hashsize) {
      HashState state;

      state.h[0]  = seed + olen + 1;
      state.h[1]  = seed + olen + 2;
      state.h[2]  = seed + olen + 2;
      state.h[3]  = seed + olen + 3;
      state.h[4]  = seed + olen + 5;
      state.h[5]  = seed + olen + 7;
      state.h[6]  = seed + olen + 11;
      state.h[7]  = seed + olen + 13;
      state.h[8]  = seed + olen + 17;
      state.h[9]  = seed + olen + 19;
      state.h[10] = seed + olen + 23;
      state.h[11] = seed + olen + 29;
      state.h[12] = seed + olen + 31;
      state.h[13] = seed + olen + 37;
      state.h[14] = seed + olen + 41;
      state.h[15] = seed + olen + 43;

      state.len = 0;  
      state.seed = seed;
      state.olen = olen;
      state.hashsize = hashsize;
      return state;
    }

    void update(const uint8_t* chunk, size_t chunk_len) {
      uint64_t temp[8];
      if (this->final_block) {
        // can't update after final block
        return;
      }

      while (chunk_len >= 64) {
        for (int i = 0, j = 0; i < 8; ++i, j += 8) {
          temp[i] = GET_U64<false>(chunk, j);
        }

        for (int i = 0; i < ROUNDS; i++) {
          weakfunc(this->h, temp, i & 1);
        }

        chunk += 64;
        chunk_len -= 64;
        this->len += 64;
      }

      if (chunk_len > 0 || this->olen == 0) {
        // Pad the remaining data
        memset(temp, (0x80 + chunk_len) & 255, sizeof(temp));
        memcpy(temp, chunk, chunk_len);
        // Frank's fix: Don't perform the length encoding that can cause issues:
        // temp[lenRemaining >> 3] |= (uint64_t)(lenRemaining << ((lenRemaining&7)*8)); 
        // was removed in Frank's code.

        for (int i = 0; i < ROUNDS; i++) {
          weakfunc(this->h, temp, i & 1);
        }

        for (int i = 0, j = 8; i < 8; i++, j++) {
          h[i] -= h[j];
        }

        // If hashsize > 64, do final rounds as Frank does:
        if (hashsize > 64) {
          for (int i = 0; i < std::max((int)hashsize / 64, FINAL_ROUNDS); i++) {
            weakfunc(h, temp, true);
          }
        }

        this->len += chunk_len;
        this->final_block = true;
      }
    }

    void finalize(void* out) {
      if (finalized) {
        return;
      }

      for (int i = 0, j = 0; i < std::min((int)8, (int)this->hashsize / 64); i++, j += 8) {
        PUT_U64<false>(h[i], (uint8_t *)out, j);
      }

      finalized = true;
    }
  };

  template <uint32_t hashsize, bool bswap>
  static void rainstorm_nis1(const void* in, const size_t len, const seed_t seed, void* out) {
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

    while (lenRemaining >= 64) {
      for (int i = 0, j = 0; i < 8; ++i, j += 8) {
        temp[i] = GET_U64<bswap>(data, j);
      }

      for (int i = 0; i < ROUNDS; i++) {
        weakfunc(h, temp, i & 1);
      }

      data += 64;
      lenRemaining -= 64;
    }

    memset(temp, (0x80 + lenRemaining) & 255, sizeof(temp));
    memcpy(temp, data, lenRemaining);
    // Frank's fix: remove the length encoding line that can cause issues
    // temp[lenRemaining >> 3] |= (uint64_t)(lenRemaining << ((lenRemaining&7)*8));

    for (int i = 0; i < ROUNDS; i++) {
      weakfunc(h, temp, i & 1);
    }

    for (int i = 0, j = 8; i < 8; i++, j++) {
      h[i] -= h[j];
    }

    if (hashsize > 64) {
      for (int i = 0; i < std::max((int)hashsize / 64, FINAL_ROUNDS); i++) {
        weakfunc(h, temp, true);
      }
    }

    for (uint32_t i = 0, j = 0; i < std::min((uint32_t)8, hashsize / 64); i++, j += 8) {
      PUT_U64<bswap>(h[i], (uint8_t *)out, j);
    }
  }
}

#ifdef __EMSCRIPTEN__
extern "C" {
  KEEPALIVE void rainstorm_nis1Hash64(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm_nis1::rainstorm_nis1<64, false>(in, len, seed, out);
  }

  KEEPALIVE void rainstorm_nis1Hash128(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm_nis1::rainstorm_nis1<128, false>(in, len, seed, out);
  }

  KEEPALIVE void rainstorm_nis1Hash256(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm_nis1::rainstorm_nis1<256, false>(in, len, seed, out);
  }

  KEEPALIVE void rainstorm_nis1Hash512(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm_nis1::rainstorm_nis1<512, false>(in, len, seed, out);
  }
}
#endif

