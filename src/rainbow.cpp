#define __RAINBNOWVERSION__ "3.7.0"
// includes the complete flow via mixB in response to a lack of backwards flow identified by Reiner Pope
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define KEEPALIVE
#endif

#include "common.h"

namespace rainbow {
  static const uint64_t P = UINT64_C(0xFFFFFFFFFFFFFFFF) - 58;
  static const uint64_t Q = UINT64_C(13166748625691186689);
  static const uint64_t R = UINT64_C(1573836600196043749);
  static const uint64_t S = UINT64_C(1478582680485693857);
  static const uint64_t T = UINT64_C(1584163446043636637);
  static const uint64_t U = UINT64_C(1358537349836140151);
  static const uint64_t V = UINT64_C(2849285319520710901);
  static const uint64_t W = UINT64_C(2366157163652459183);

  static inline void mixA(uint64_t* s) {
    uint64_t a = s[0], b = s[1], c = s[2], d = s[3];

    a *= P;
    a = ROTR64(a, 23);
    a *= Q;

    b ^= a;

    b *= R;
    b = ROTR64(b, 29);
    b *= S;

    c *= T;
    c = ROTR64(c, 31);
    c *= U;

    d ^= c;

    d *= V;
    d = ROTR64(d, 37);
    d *= W;

    s[0] = a; s[1] = b; s[2] = c; s[3] = d;
  }

  static inline void mixB(uint64_t* s, uint64_t iv) {
    uint64_t a = s[1], b = s[2];

    a *= V;
    a = ROTR64(a, 23);
    a *= W;

    b ^= a + iv;

    b *= R;
    b = ROTR64(b, 23);
    b *= S;

    s[1] = b; s[2] = a;
  }

  // Streaming state structure if needed (mirroring your original style)
  struct HashState : IHashState {
    uint64_t  h[4];
    seed_t    seed;
    size_t    len;
    uint32_t  hashsize;
    bool      inner = false;
    bool      final_block = false;
    bool      finalized = false;

    static HashState initialize(const seed_t seed, size_t olen, uint32_t hashsize) {
      HashState state;
      state.h[0] = seed + olen + 1;
      state.h[1] = seed + olen + 3;
      state.h[2] = seed + olen + 5;
      state.h[3] = seed + olen + 7;
      state.len = 0;
      state.seed = seed;
      state.hashsize = hashsize;
      return state;
    }

    void update(const uint8_t* chunk, size_t chunk_len) {
      while (chunk_len >= 16) {
        uint64_t g = GET_U64<false>(chunk, 0);

        h[0] -= g;
        h[1] += g;
        chunk += 8;

        g = GET_U64<false>(chunk, 0);
        h[2] += g;
        h[3] -= g;

        if (inner) {
          mixB(h, seed);
        } else {
          mixA(h);
        }
        inner = !inner;

        chunk += 8;
        chunk_len -= 16;
        len += 16;
      }

      // Process any remaining data
      // According to Frank's logic, if there's any remainder it's the final block
      if (chunk_len >= 0) {
        final_block = true;
        mixB(h, seed);

        switch (chunk_len) {
          case 15: h[0] += (uint64_t)chunk[14] << 56; // [[fallthrough]];
          case 14: h[1] += (uint64_t)chunk[13] << 48; // [[fallthrough]]
          case 13: h[2] += (uint64_t)chunk[12] << 40; // [[fallthrough]]
          case 12: h[3] += (uint64_t)chunk[11] << 32; // [[fallthrough]]
          case 11: h[0] += (uint64_t)chunk[10] << 24; // [[fallthrough]]
          case 10: h[1] += (uint64_t)chunk[9]  << 16; // [[fallthrough]]
          case  9: h[2] += (uint64_t)chunk[8]  << 8;  // [[fallthrough]]
          case  8: h[3] += chunk[7];                  //  [[fallthrough]]
          case  7: h[0] += (uint64_t)chunk[6]  << 48; // [[fallthrough]]
          case  6: h[1] += (uint64_t)chunk[5]  << 40; // [[fallthrough]]
          case  5: h[2] += (uint64_t)chunk[4]  << 32; // [[fallthrough]]
          case  4: h[3] += (uint64_t)chunk[3]  << 24; // [[fallthrough]]
          case  3: h[0] += (uint64_t)chunk[2]  << 16; // [[fallthrough]]
          case  2: h[1] += (uint64_t)chunk[1]  <<  8; // [[fallthrough]]
          case  1: h[2] += (uint64_t)chunk[0];
        }

        mixA(h);
        mixB(h, seed);
        mixA(h);

        len += chunk_len;
      }
    }

    void finalize(void* out) {
      if (finalized) return;

      uint64_t g = 0;
      g -= h[2];
      g -= h[3];
      PUT_U64<false>(g, (uint8_t *)out, 0);

      if (hashsize == 128) {
        mixA(h);
        g = 0;
        g -= h[3];
        g -= h[2];
        PUT_U64<false>(g, (uint8_t *)out, 8);
      } else if (hashsize == 256) {
        mixA(h);
        g = 0;
        g -= h[3];
        g -= h[2];
        PUT_U64<false>(g, (uint8_t *)out, 8);
        mixA(h);
        mixB(h, seed);
        mixA(h);
        g = 0;
        g -= h[3];
        g -= h[2];
        PUT_U64<false>(g, (uint8_t *)out, 16);
        mixA(h);
        g = 0;
        g -= h[3];
        g -= h[2];
        PUT_U64<false>(g, (uint8_t *)out, 24);
      }

      finalized = true;
    }
  };

  // Single-call version with no streaming needed
  template <uint32_t hashsize, bool bswap>
  static void rainbow(const void* in, const size_t olen, const seed_t seed, void* out) {
    const uint8_t * data = (const uint8_t *)in;
    uint64_t h[4] = {
      seed + olen + 1,
      seed + olen + 3,
      seed + olen + 5,
      seed + olen + 7
    };
    size_t len = olen;
    uint64_t g = 0;
    bool inner = false;

    while (len >= 16) {
      g = GET_U64<bswap>(data, 0);
      h[0] -= g;
      h[1] += g;
      data += 8;

      g = GET_U64<bswap>(data, 0);
      h[2] += g;
      h[3] -= g;

      if (inner) {
        mixB(h, seed);
      } else {
        mixA(h);
      }
      inner = !inner;

      data += 8;
      len  -= 16;
    }

    // After main loop
    mixB(h, seed);

    switch (len) {
      case 15: h[0] += (uint64_t)data[14] << 56; // [[fallthrough]];
      case 14: h[1] += (uint64_t)data[13] << 48; // [[fallthrough]]
      case 13: h[2] += (uint64_t)data[12] << 40; // [[fallthrough]]
      case 12: h[3] += (uint64_t)data[11] << 32; // [[fallthrough]]
      case 11: h[0] += (uint64_t)data[10] << 24; // [[fallthrough]]
      case 10: h[1] += (uint64_t)data[9]  << 16; // [[fallthrough]]
      case  9: h[2] += (uint64_t)data[8]  << 8;  // [[fallthrough]]
      case  8: h[3] += data[7];                  //  [[fallthrough]]
      case  7: h[0] += (uint64_t)data[6]  << 48; // [[fallthrough]]
      case  6: h[1] += (uint64_t)data[5]  << 40; // [[fallthrough]]
      case  5: h[2] += (uint64_t)data[4]  << 32; // [[fallthrough]]
      case  4: h[3] += (uint64_t)data[3]  << 24; // [[fallthrough]]
      case  3: h[0] += (uint64_t)data[2]  << 16; // [[fallthrough]]
      case  2: h[1] += (uint64_t)data[1]  <<  8; // [[fallthrough]]
      case  1: h[2] += (uint64_t)data[0];
    }

    mixA(h);
    mixB(h, seed);
    mixA(h);

    g = 0;
    g -= h[2];
    g -= h[3];
    PUT_U64<bswap>(g, (uint8_t *)out, 0);

    if (hashsize == 128) {
      mixA(h);
      g = 0;
      g -= h[3];
      g -= h[2];
      PUT_U64<bswap>(g, (uint8_t *)out, 8);
    } else if (hashsize == 256) {
      mixA(h);
      g = 0;
      g -= h[3];
      g -= h[2];
      PUT_U64<bswap>(g, (uint8_t *)out, 8);
      mixA(h);
      mixB(h, seed);
      mixA(h);
      g = 0;
      g -= h[3];
      g -= h[2];
      PUT_U64<bswap>(g, (uint8_t *)out, 16);
      mixA(h);
      g = 0;
      g -= h[3];
      g -= h[2];
      PUT_U64<bswap>(g, (uint8_t *)out, 24);
    }
  }
}
