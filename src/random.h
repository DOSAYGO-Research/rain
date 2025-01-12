#pragma once

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <random>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// Platform-specific includes
#ifdef _WIN32
  #include <windows.h>
  #include <bcrypt.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/random.h>
#elif defined(__EMSCRIPTEN__)
  #include <emscripten/emscripten.h>

// JavaScript function to fill buffer with secure random bytes
EM_JS(int, fill_random_bytes, (void* buf, size_t size), {
    try {
        var heap = new Uint8Array(Module.HEAPU8.buffer, buf, size);
        if (typeof crypto !== 'undefined' && typeof crypto.getRandomValues === 'function') {
            // Browser environment
            crypto.getRandomValues(heap);
        } else if (typeof require === 'function') {
            // Node.js environment
            var nodeCrypto = require('crypto');
            var randomBytes = nodeCrypto.randomBytes(size);
            heap.set(randomBytes);
        } else {
            throw new Error('No secure random number generator available.');
        }
        return 0; // Success
    } catch (e) {
        return -1; // Failure
    }
});
#else
  #include <fstream>
#endif

class CustomRandom {
public:
  // Random number from 64-bit unsigned integer range
  static uint64_t randombytes_random() {
    uint64_t value;
    randombytes_buf(&value, sizeof(value));
    return value;
  }

  // Uniform random number in range [0, upper_bound)
  static uint64_t randombytes_uniform(const uint64_t upper_bound) {
    if (upper_bound < 2) {
      return 0; // Only one possible value
    }

    uint64_t r, min;
    min = -upper_bound % upper_bound; // Ensure uniformity

    do {
      r = randombytes_random();
    } while (r < min);

    return r % upper_bound;
  }

  // Fill a buffer with random bytes
  static void randombytes_buf(void *buf, size_t size) {
#ifdef __EMSCRIPTEN__
    if (fill_random_bytes(buf, size) != 0) {
        throw std::runtime_error("fill_random_bytes failed");
    }
#elif defined(_WIN32)
    if (BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf), static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    if (getrandom(buf, size, 0) == -1) {
        throw std::runtime_error("getrandom() failed");
    }
#elif defined(__APPLE__)
    if (getentropy(buf, size) == -1) {
        throw std::runtime_error("getentropy() failed");
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Failed to open /dev/urandom");
    }
    ssize_t read_bytes = read(fd, buf, size);
    close(fd);
    if (read_bytes < 0 || static_cast<size_t>(read_bytes) != size) {
        throw std::runtime_error("Failed to read from /dev/urandom");
    }
#endif
  }
};

// Random function alias
using RandomFunc = std::function<void(void*, size_t)>;

// Default mode: std::mt19937_64 seeded with std::random_device
void defaultRandom(void* buf, size_t size) {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    uint8_t* output = static_cast<uint8_t*>(buf);
    for (size_t i = 0; i < size; ++i) {
        output[i] = dist(rng);
    }
}

// Seeded-MT mode: std::mt19937_64 seeded with CustomRandom
void seededMtRandom(void* buf, size_t size) {
    static thread_local std::mt19937_64 rng(CustomRandom::randombytes_random());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    uint8_t* output = static_cast<uint8_t*>(buf);
    for (size_t i = 0; i < size; ++i) {
        output[i] = dist(rng);
    }
}

// Direct mode: Fully use CustomRandom
void directRandom(void* buf, size_t size) {
    CustomRandom::randombytes_buf(buf, size);
}

// Function to select random function based on entropy mode
RandomFunc selectRandomFunc(const std::string& entropyMode) {
    if (entropyMode == "default") {
        return defaultRandom;
    } else if (entropyMode == "seeded-mt") {
        return seededMtRandom;
    } else if (entropyMode == "direct") {
        return directRandom;
    } else {
        throw std::invalid_argument("Invalid entropy mode: " + entropyMode);
    }
}

