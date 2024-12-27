#pragma once

#include <fstream>
#include <sys/stat.h>

// only define USE_FILESYSTEM if it is supported and needed
#ifdef USE_FILESYSTEM
#include <filesystem>
#endif

#include "rainbow.cpp"
#include "rainstorm.cpp"
#include "cxxopts.hpp"
#include "common.h"

#define VERSION "1.3.0"

enum class Mode {
  Digest,
  Stream,
  Enc,   // added
  Dec    // added
};

std::string modeToString(const Mode& mode) {
  switch(mode) {
    case Mode::Digest: return "Digest";
    case Mode::Stream: return "Stream";
    case Mode::Enc:    return "Enc";    // added
    case Mode::Dec:    return "Dec";    // added
    default: throw std::runtime_error("Unknown hash mode");
  }
}

std::istream& operator>>(std::istream& in, Mode& mode) {
  std::string token;
  in >> token;
  if (token == "digest")      mode = Mode::Digest;
  else if (token == "stream") mode = Mode::Stream;
  else if (token == "enc")    mode = Mode::Enc;   // added
  else if (token == "dec")    mode = Mode::Dec;   // added
  else                       in.setstate(std::ios_base::failbit);
  return in;
}


// Mining mode enum
enum class MineMode {
  None,
  Chain,
  NonceInc,
  NonceRand
};

std::string mineModeToString(const MineMode& mode) {
  switch(mode) {
    case MineMode::None:          return "None";
    case MineMode::Chain:         return "Chain";
    case MineMode::NonceInc:      return "NonceInc";    // added
    case MineMode::NonceRand:     return "NonceRand";    // added
    default: throw std::runtime_error("Unknown mine mode");
  }
}

// ------------------------------------------------------------------
// Mining Mode Operator>>(std::istream&, MineMode&) Implementation
//    (If you haven't placed this in tool.h, it can go here)
// ------------------------------------------------------------------
std::istream& operator>>(std::istream& in, MineMode& mode) {
  std::string token;
  in >> token;
  if (token == "chain") {
    mode = MineMode::Chain;
  } else if (token == "nonceInc") {
    mode = MineMode::NonceInc;
  } else if (token == "nonceRand") {
    mode = MineMode::NonceRand;
  } else {
    mode = MineMode::None;
  }
  return in;
}

enum SearchMode {
  Prefix,
  Sequence, 
  Series,
  Scatter
};

std::string searchModeToString(const SearchMode& mode) {
  switch(mode) {
    case SearchMode::Prefix:      return "Prefix";
    case SearchMode::Sequence:    return "Sequence";
    case SearchMode::Series:      return "Series";      // added
    case SearchMode::Scatter:     return "Scatter";     // added
    default: throw std::runtime_error("Unknown search mode");
  }
}

std::istream& operator>>(std::istream& in, SearchMode& mode) {
  std::string token;
  in >> token;
  if (token == "prefix") {
    mode = SearchMode::Prefix;
  } else if (token == "Sequence") {
    mode = SearchMode::Sequence;
  } else if (token == "Series") {
    mode = SearchMode::Series;
  } else if (token == "Scatter" ) {
    mode = SearchMode::Scatter;
  } else {
    in.setstate(std::ios_base::failbit);
  }
  return in;
}

// Stream retrieval (stdin vs file)
std::istream& getInputStream() {
#ifdef _WIN32
  // On Windows, use std::cin
  return std::cin;
#else
  // On Unix-based systems, use std::ifstream with /dev/stdin
  static std::ifstream in("/dev/stdin");
  return in;
#endif
}

enum class HashAlgorithm {
  Rainbow,
  Rainstorm,
  Unknown
};

std::string hashAlgoToString(const HashAlgorithm& algo) {
  switch(algo) {
    case HashAlgorithm::Rainbow:   return "Rainbow";
    case HashAlgorithm::Rainstorm: return "Rainstorm";
    default:
      throw std::runtime_error("Unknown hash algorithm value (expected rainbow or rainstorm)");
  }
}

// Convert string -> HashAlgorithm
HashAlgorithm getHashAlgorithm(const std::string& algorithm) {
  if (algorithm == "rainbow" || algorithm == "bow") {
    return HashAlgorithm::Rainbow;
  } else if (algorithm == "rainstorm" || algorithm == "storm") {
    return HashAlgorithm::Rainstorm;
  } else {
    return HashAlgorithm::Unknown;
  }
}

// Prototypes
void usage();

void hashBuffer(Mode mode, HashAlgorithm algot,
                std::vector<uint8_t>& buffer, uint64_t seed,
                uint64_t output_length, std::ostream& outstream,
                uint32_t hash_size);

void hashAnything(Mode mode, HashAlgorithm algot,
                  const std::string& inpath, std::ostream& outstream,
                  uint32_t size, bool use_test_vectors,
                  uint64_t seed, uint64_t output_length);

std::string generate_filename(const std::string& filename);

uint64_t hash_string_to_64_bit(const std::string& seed_str);

// For cxxopts: getFileSize usage
uint64_t getFileSize(const std::string& filename) {
  struct stat st;
  if(stat(filename.c_str(), &st) != 0) {
    return 0; // You may want to handle this error differently
  }
  return st.st_size;
}

#ifdef USE_FILESYSTEM
std::string generate_filename(const std::string& filename) {
  std::filesystem::path p{filename};
  std::string new_filename;
  std::string timestamp = "-" + std::to_string(std::time(nullptr));

  if (std::filesystem::exists(p)) {
    new_filename = p.stem().string() + timestamp + p.extension().string();

    // Handle filename collision
    int counter = 1;
    while (std::filesystem::exists(new_filename)) {
      new_filename = p.stem().string() + timestamp + "-" + std::to_string(counter++)
                     + p.extension().string();
    }
  } else {
    new_filename = filename;
  }

  return new_filename;
}
#else
// If filesystem isn't available, just return the filename as is
std::string generate_filename(const std::string& filename) {
  return filename;
}
#endif

uint64_t hash_string_to_64_bit(const std::string& seed_str) {
  std::vector<char> buffer(seed_str.begin(), seed_str.end());
  std::vector<uint8_t> hash_output(8); // 64 bits = 8 bytes
  // We'll use 64-bit rainstorm for string -> seed
  rainstorm::rainstorm<64, bswap>(buffer.data(), buffer.size(), 0, hash_output.data());
  uint64_t seed = 0;
  std::memcpy(&seed, hash_output.data(), 8);
  return seed;
}

void usage() {
  std::cout << "Usage: rainsum [OPTIONS] [INFILE]\n"
            << "Calculate a Rainbow or Rainstorm hash.\n\n"
            << "Options:\n"
            << "  -m, --mode [digest|stream]        Specifies the mode, where:\n"
            << "                                    digest mode (the default) gives a fixed length hash in hex,\n"
            << "                                    stream mode gives a variable length binary feedback output\n"
            << "  -a, --algorithm [bow|storm]       Specify the hash algorithm to use. Default: storm\n"
            << "  -s, --size [64-256|64-512]        Specify the bit size of the hash. Default: 256\n"
            << "  -o, --output-file FILE            Output file for the hash or stream\n"
            << "  -t, --test-vectors                Calculate the hash of the standard test vectors\n"
            << "  -l, --output-length HASHES        Set the output length in hash iterations (stream only)\n"
            << "  -v, --version                     Print out the version\n"
            << "  --seed                            Seed value (64-bit number or string). If string is used,\n"
            << "                                    it is hashed with Rainstorm to a 64-bit number\n"
            << "  --mine-mode [chain|nonceInc|nonceRand]   Perform 'mining' tasks until prefix is matched\n"
            << "  --match-prefix <hexstring>               Hex prefix to match for mining tasks\n";
}

// Standard test vectors
std::vector<std::string> test_vectors = {
  "",
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
  "The quick brown fox jumps over the lazy dog",
  "The quick brown fox jumps over the lazy cog",
  "The quick brown fox jumps over the lazy dog.",
  "After the rainstorm comes the rainbow.",
  std::string(64, '@')
};

