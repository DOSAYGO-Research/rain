#pragma once
#include <atomic> // for std::atomic
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <iterator>
#include <zlib.h>
#include <stdexcept>
// only define USE_FILESYSTEM if it is supported and needed
#include <sys/stat.h>
#ifdef USE_FILESYSTEM
#include <filesystem>
#endif

#include "rainbow.cpp"
#include "rainstorm.cpp"
#include "cxxopts.hpp"
#include "common.h"

#define VERSION "1.3.1"

uint32_t MagicNumber = 0x59524352; // RCRY

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

// enums
  enum class Mode {
    Digest,
    Stream,
    BlockEnc,   
    StreamEnc,
    Dec,    
    Info
  };

  // Mining mode enum
  enum class MineMode {
    None,
    Chain,
    NonceInc,
    NonceRand
  };

  enum SearchMode {
    Prefix,
    Sequence, 
    Series,
    Scatter,
    MapScatter,
    ParaScatter
  };

  enum class HashAlgorithm {
    Rainbow,
    Rainstorm,
    Unknown
  };

// Prototypes
  // Add a forward declaration for our new function
  static void puzzleShowFileInfo(const std::string &inFilename);

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

// stream helpers
  template <typename T>
  std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
    os << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
      if (i > 0) os << ", ";
      os << vec[i];
    }
    os << "]";
    return os;
  }

  inline bool hasPrefix(const std::vector<uint8_t>& hash_output, const std::vector<uint8_t>& prefix_bytes) {
    if (prefix_bytes.size() > hash_output.size()) return false;
    return std::equal(prefix_bytes.begin(), prefix_bytes.end(), hash_output.begin());
  }

// Helper function to securely overwrite a file with zeros and then truncate it
  static void overwriteFileWithZeros(const std::string &filename) {
      if (!std::filesystem::exists(filename)) {
          return; // File does not exist; nothing to do
      }

      // Open the file in read-write binary mode
      std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
      if (!file.is_open()) {
          throw std::runtime_error("Cannot open existing file for overwriting: " + filename);
      }

      // Determine the size of the file
      file.seekg(0, std::ios::end);
      std::streampos fileSize = file.tellg();
      file.seekp(0, std::ios::beg);

      // Create a buffer of zeros
      const size_t bufferSize = 4096; // 4KB buffer
      std::vector<char> zeros(bufferSize, 0x00);

      std::streampos remaining = fileSize;
      while (remaining > 0) {
          size_t toWrite = static_cast<size_t>(std::min(static_cast<std::streampos>(bufferSize), remaining));
          file.write(zeros.data(), toWrite);
          if (!file) {
              throw std::runtime_error("Failed to write zeros to file: " + filename);
          }
          remaining -= toWrite;
      }

      file.close();

      // Truncate the file to zero size
      std::filesystem::resize_file(filename, 0);
  }

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

// mode helpers
  std::string modeToString(const Mode& mode) {
    switch(mode) {
      case Mode::Digest: return "Digest";
      case Mode::Stream: return "Stream";
      case Mode::BlockEnc:    return "BlockEnc";    // added
      case Mode::StreamEnc:    return "StreamEnc";    // added
      case Mode::Dec:    return "Dec";    // added
      case Mode::Info:   return "Info";
      default: throw std::runtime_error("Unknown hash mode");
    }
  }

  std::istream& operator>>(std::istream& in, Mode& mode) {
    std::string token;
    in >> token;
    if (token == "digest")      mode = Mode::Digest;
    else if (token == "stream") mode = Mode::Stream;
    else if (token == "block-enc")    mode = Mode::BlockEnc;   // added
    else if (token == "stream-enc")    mode = Mode::StreamEnc;   // added
    else if (token == "dec")    mode = Mode::Dec;   // added
    else if (token == "info")   mode = Mode::Info;
    else                       in.setstate(std::ios_base::failbit);
    return in;
  }

  std::string mineModeToString(const MineMode& mode) {
    switch(mode) {
      case MineMode::None:          return "None";
      case MineMode::Chain:         return "Chain";
      case MineMode::NonceInc:      return "NonceInc";    // added
      case MineMode::NonceRand:     return "NonceRand";    // added
      default: throw std::runtime_error("Unknown mine mode");
    }
  }

// ADDED: Unified FileHeader struct to include IV and Salt
struct FileHeader {
    uint32_t magic;             // MagicNumber
    uint8_t version;            // Version
    uint8_t cipherMode;         // Mode: 0x10 = Stream, 0x11 = Block
    uint8_t blockSize;          // Block size in bytes (for block cipher)
    uint8_t nonceSize;          // Nonce size in bytes
    uint16_t hashSizeBits;      // Hash size in bits
    std::string hashName;       // Hash algorithm name
    uint64_t iv;                // 64-bit IV (seed)
    uint8_t saltLen;            // Length of salt
    std::vector<uint8_t> salt;  // Salt data
    uint8_t searchModeEnum;     // Search mode enum (0x00 - 0x05 for block ciphers, 0xFF for stream)
    uint64_t originalSize;      // Compressed plaintext size
};

// ADDED: Function to write the unified FileHeader
static void writeFileHeader(std::ofstream &out, const FileHeader &hdr) {
    out.write(reinterpret_cast<const char*>(&hdr.magic), sizeof(hdr.magic));
    out.write(reinterpret_cast<const char*>(&hdr.version), sizeof(hdr.version));
    out.write(reinterpret_cast<const char*>(&hdr.cipherMode), sizeof(hdr.cipherMode));
    out.write(reinterpret_cast<const char*>(&hdr.blockSize), sizeof(hdr.blockSize));
    out.write(reinterpret_cast<const char*>(&hdr.nonceSize), sizeof(hdr.nonceSize));
    out.write(reinterpret_cast<const char*>(&hdr.hashSizeBits), sizeof(hdr.hashSizeBits));
    
    uint8_t hnLen = static_cast<uint8_t>(hdr.hashName.size());
    out.write(reinterpret_cast<const char*>(&hnLen), sizeof(hnLen));
    out.write(hdr.hashName.data(), hnLen);
    
    out.write(reinterpret_cast<const char*>(&hdr.iv), sizeof(hdr.iv));
    
    out.write(reinterpret_cast<const char*>(&hdr.saltLen), sizeof(hdr.saltLen));
    if (hdr.saltLen > 0) {
        out.write(reinterpret_cast<const char*>(hdr.salt.data()), hdr.salt.size());
    }
    
    out.write(reinterpret_cast<const char*>(&hdr.searchModeEnum), sizeof(hdr.searchModeEnum));
    out.write(reinterpret_cast<const char*>(&hdr.originalSize), sizeof(hdr.originalSize));
}

// ADDED: Function to read the unified FileHeader
static FileHeader readFileHeader(std::ifstream &in) {
    FileHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr.magic), sizeof(hdr.magic));
    in.read(reinterpret_cast<char*>(&hdr.version), sizeof(hdr.version));
    in.read(reinterpret_cast<char*>(&hdr.cipherMode), sizeof(hdr.cipherMode));
    in.read(reinterpret_cast<char*>(&hdr.blockSize), sizeof(hdr.blockSize));
    in.read(reinterpret_cast<char*>(&hdr.nonceSize), sizeof(hdr.nonceSize));
    in.read(reinterpret_cast<char*>(&hdr.hashSizeBits), sizeof(hdr.hashSizeBits));
    
    uint8_t hnLen;
    in.read(reinterpret_cast<char*>(&hnLen), sizeof(hnLen));
    hdr.hashName.resize(hnLen);
    in.read(&hdr.hashName[0], hnLen);
    
    in.read(reinterpret_cast<char*>(&hdr.iv), sizeof(hdr.iv));
    
    in.read(reinterpret_cast<char*>(&hdr.saltLen), sizeof(hdr.saltLen));
    hdr.salt.resize(hdr.saltLen);
    if (hdr.saltLen > 0) {
        in.read(reinterpret_cast<char*>(hdr.salt.data()), hdr.saltLen);
    }
    
    in.read(reinterpret_cast<char*>(&hdr.searchModeEnum), sizeof(hdr.searchModeEnum));
    in.read(reinterpret_cast<char*>(&hdr.originalSize), sizeof(hdr.originalSize));
    
    return hdr;
}

// ADDED: Function to check if a file is in Stream Cipher mode
static bool fileIsStreamMode(const std::string &filename) {
    std::ifstream fin(filename, std::ios::binary);
    if (!fin.is_open()) return false;
    FileHeader hdr = readFileHeader(fin);
    fin.close();
    return (hdr.magic == MagicNumber) && (hdr.cipherMode == 0x10); // 0x10 = Stream Cipher
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
  std::string searchModeToString(const SearchMode& mode) {
    switch(mode) {
      case SearchMode::Prefix:      return "Prefix";
      case SearchMode::Sequence:    return "Sequence";
      case SearchMode::Series:      return "Series";      // added
      case SearchMode::Scatter:     return "Scatter";     // added
      case SearchMode::MapScatter:     return "MapScatter";     // added
      case SearchMode::ParaScatter:     return "ParaScatter";     // added
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
    } else if (token == "MapScatter" ) {
      mode = SearchMode::MapScatter;
    } else if (token == "ParaScatter" ) {
      mode = SearchMode::ParaScatter;
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


// Convert string -> HashAlgorithm
  std::string hashAlgoToString(const HashAlgorithm& algo) {
    switch(algo) {
      case HashAlgorithm::Rainbow:   return "Rainbow";
      case HashAlgorithm::Rainstorm: return "Rainstorm";
      default:
        throw std::runtime_error("Unknown hash algorithm value (expected rainbow or rainstorm)");
    }
  }

  HashAlgorithm getHashAlgorithm(const std::string& algorithm) {
    if (algorithm == "rainbow" || algorithm == "bow") {
      return HashAlgorithm::Rainbow;
    } else if (algorithm == "rainstorm" || algorithm == "storm") {
      return HashAlgorithm::Rainstorm;
    } else {
      return HashAlgorithm::Unknown;
    }
  }

// hash helper
  template<bool bswap>
  void invokeHash(HashAlgorithm algot, uint64_t seed, std::vector<uint8_t>& buffer,
                  std::vector<uint8_t>& temp_out, int hash_size) {
    if (algot == HashAlgorithm::Rainbow) {
      switch(hash_size) {
        case 64:
          rainbow::rainbow<64, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
          break;
        case 128:
          rainbow::rainbow<128, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
          break;
        case 256:
          rainbow::rainbow<256, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
          break;
        default:
          throw std::runtime_error("Invalid hash_size for rainbow");
      }
    } else if (algot == HashAlgorithm::Rainstorm) {
      switch(hash_size) {
        case 64:
          rainstorm::rainstorm<64, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
          break;
        case 128:
          rainstorm::rainstorm<128, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
          break;
        case 256:
          rainstorm::rainstorm<256, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
          break;
        case 512:
          rainstorm::rainstorm<512, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
          break;
        default:
          throw std::runtime_error("Invalid hash_size for rainstorm");
      }
    } else {
      throw std::runtime_error("Invalid algorithm: " + hashAlgoToString(algot));
    }
  }

// ------------------------------------------------------------------
// Mining Implementations
// ------------------------------------------------------------------
  static void chainAppend(std::vector<uint8_t>& inputBuffer, const std::vector<uint8_t>& hash_output) {
    // Just append the new hash to the existing buffer
    inputBuffer.insert(inputBuffer.end(), hash_output.begin(), hash_output.end());
  }

  void mineChain(HashAlgorithm algot, uint64_t seed, uint32_t hash_size, const std::vector<uint8_t>& prefix_bytes) {
    std::vector<uint8_t> inputBuffer;
    std::vector<uint8_t> hash_output(hash_size / 8);

    uint64_t iterationCount = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
      iterationCount++;
      invokeHash<bswap>(algot, seed, inputBuffer, hash_output, hash_size);

      if (hasPrefix(hash_output, prefix_bytes)) {
        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        double hps = iterationCount / elapsed;

        std::cerr << "\n[mineChain] Found after " << iterationCount << " iterations, ~"
                  << hps << " H/s\n";

        // Print final hash to stdout
        std::cout << "Final Hash: ";
        for (auto b : hash_output) {
          std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        std::cout << std::dec << std::endl;
        return;
      }
      chainAppend(inputBuffer, hash_output);

      if (iterationCount % 1000 == 0) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        double hps = iterationCount / elapsed;
        std::cerr << "\r[mineChain] " << iterationCount << " iterations, ~"
                  << hps << " H/s    " << std::flush;
      }
    }
  }

  void mineNonceInc(HashAlgorithm algot, uint64_t seed, uint32_t hash_size,
                    const std::vector<uint8_t>& prefix_bytes,
                    const std::string& baseInput) {
    std::vector<uint8_t> hash_output(hash_size / 8);
    uint64_t iterationCount = 0;
    uint64_t nonce = 0;

    auto start_time = std::chrono::steady_clock::now();

    while (true) {
      iterationCount++;

      // Build input = baseInput + numeric nonce as text
      std::stringstream ss;
      ss << baseInput << nonce;
      std::string inputStr = ss.str();

      // Convert to bytes
      std::vector<uint8_t> buffer(inputStr.begin(), inputStr.end());
      invokeHash<bswap>(algot, seed, buffer, hash_output, hash_size);

      if (hasPrefix(hash_output, prefix_bytes)) {
        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        double hps = iterationCount / elapsed;

        std::cerr << "\n[mineNonceInc] Found after " << iterationCount
                  << " iterations, ~" << hps << " H/s\n"
                  << "Winning nonce: " << nonce << "\n";

        std::cout << "Final Hash: ";
        for (auto b : hash_output) {
          std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        std::cout << std::dec << "\n";
        return;
      }

      if (iterationCount % 1000 == 0) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        double hps = iterationCount / elapsed;
        std::cerr << "\r[mineNonceInc] " << iterationCount << " iterations, ~"
                  << hps << " H/s    " << std::flush;
      }

      nonce++;
    }
  }

  void mineNonceRand(HashAlgorithm algot, uint64_t seed, uint32_t hash_size,
                     const std::vector<uint8_t>& prefix_bytes,
                     const std::string& baseInput) {
    std::vector<uint8_t> hash_output(hash_size / 8);
    uint64_t iterationCount = 0;

    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    auto start_time = std::chrono::steady_clock::now();

    while (true) {
      iterationCount++;

      // Build input = baseInput + random bytes
      std::vector<uint8_t> buffer(baseInput.begin(), baseInput.end());
      // e.g. 16 random bytes appended
      for (int i = 0; i < 16; i++) {
        buffer.push_back(dist(rng));
      }

      invokeHash<bswap>(algot, seed, buffer, hash_output, hash_size);

      if (hasPrefix(hash_output, prefix_bytes)) {
        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        double hps = iterationCount / elapsed;

        std::cerr << "\n[mineNonceRand] Found after " << iterationCount
                  << " iterations, ~" << hps << " H/s\n";

        std::cout << "Final Hash: ";
        for (auto b : hash_output) {
          std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        std::cout << std::dec << "\n";
        return;
      }

      if (iterationCount % 1000 == 0) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        double hps = iterationCount / elapsed;
        std::cerr << "\r[mineNonceRand] " << iterationCount << " iterations, ~"
                  << hps << " H/s    " << std::flush;
      }
    }
  }

// ------------------------------------------------------------------
// Original hashBuffer
// ------------------------------------------------------------------
  void hashBuffer(Mode mode, HashAlgorithm algot, std::vector<uint8_t>& buffer,
                  uint64_t seed, uint64_t output_length, std::ostream& outstream,
                  uint32_t hash_size) {
    int byte_size = hash_size / 8;
    std::vector<uint8_t> temp_out(byte_size);

    if (mode == Mode::Digest) {
      invokeHash<bswap>(algot, seed, buffer, temp_out, hash_size);

      for (const auto& byte : temp_out) {
        outstream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
      }
    }
    else if (mode == Mode::Stream) {
      while (output_length > 0) {
        invokeHash<bswap>(algot, seed, buffer, temp_out, hash_size);

        uint64_t chunk_size = std::min(output_length, (uint64_t)byte_size);
        outstream.write(reinterpret_cast<const char*>(temp_out.data()), chunk_size);

        output_length -= chunk_size;
        if (output_length == 0) {
          break;
        }

        // Update buffer with the output of the last hash operation
        buffer.assign(temp_out.begin(), temp_out.begin() + chunk_size);
      }
    }
  }

// ------------------------------------------------------------------
// Original hashAnything
// ------------------------------------------------------------------
  void hashAnything(Mode mode, HashAlgorithm algot, const std::string& inpath,
                    std::ostream& outstream, uint32_t size, bool use_test_vectors,
                    uint64_t seed, uint64_t output_length) {

    std::vector<uint8_t> buffer;
    std::vector<uint8_t> chunk(CHUNK_SIZE);

    if (use_test_vectors) {
      for (const auto& test_vector : test_vectors) {
        buffer.assign(test_vector.begin(), test_vector.end());
        hashBuffer(mode, algot, buffer, seed, output_length, outstream, size);
        outstream << ' ' << '"' << test_vector << '"' << '\n';
      }
    } else {
      std::istream* in_stream = nullptr;
      std::ifstream infile;

      uint64_t input_length = 0;

      if (!inpath.empty()) {
        infile.open(inpath, std::ios::binary);
        if (infile.fail()) {
          throw std::runtime_error("Cannot open file for reading: " + inpath);
        }
        in_stream = &infile;
        input_length = getFileSize(inpath);

        std::unique_ptr<IHashState> state;
        if (algot == HashAlgorithm::Rainbow) {
          state = std::make_unique<rainbow::HashState>(
                    rainbow::HashState::initialize(seed, input_length, size));
        } else if (algot == HashAlgorithm::Rainstorm) {
          state = std::make_unique<rainstorm::HashState>(
                    rainstorm::HashState::initialize(seed, input_length, size));
        } else {
          throw std::runtime_error("Invalid algorithm: " + hashAlgoToString(algot));
        }

        // Stream the file
        while (*in_stream) {
          in_stream->read(reinterpret_cast<char*>(chunk.data()), CHUNK_SIZE);
          if (in_stream->fail() && !in_stream->eof()) {
            throw std::runtime_error(
              "Input file could not be read after " +
              std::to_string(state->len) + " bytes processed.");
          }
          std::streamsize bytes_read = in_stream->gcount();
          if (bytes_read > 0 || input_length == 0) {
            state->update(chunk.data(), bytes_read);
          }
        }

        if (infile.is_open()) {
          infile.close();
        }

        // Finalize
        std::vector<uint8_t> output(output_length);
        state->finalize(output.data());

        if (mode == Mode::Digest) {
          for (const auto& byte : output) {
            outstream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
          }
          outstream << ' ' << (inpath.empty() ? "stdin" : inpath) << '\n';
        } else {
          outstream.write(reinterpret_cast<char*>(output.data()), output_length);
        }
      }
      else {
        // Read from stdin fully
        in_stream = &getInputStream();
        buffer = std::vector<uint8_t>(std::istreambuf_iterator<char>(*in_stream), {});
        input_length = buffer.size();
        hashBuffer(mode, algot, buffer, seed, output_length, outstream, size);
        outstream << ' ' << (inpath.empty() ? "stdin" : inpath) << '\n';
      }
    }
  }

// ------------------------------------------------------------------
// Helper for password (key) prompt - disabling echo on POSIX systems
// ------------------------------------------------------------------
#ifdef _WIN32
  static std::string promptForKey(const std::string &prompt) {
    std::cerr << prompt;
    std::string key;
    std::getline(std::cin, key);
    return key;
  }
#else
#include <termios.h>
#include <unistd.h>
  static std::string promptForKey(const std::string &prompt) {
    std::cerr << prompt;
    // Disable echo
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios noEcho = oldt;
    noEcho.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &noEcho);

    std::string key;
    std::getline(std::cin, key);

    // Restore echo
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cerr << std::endl;
    return key;
  }
#endif

// Compress using zlib
  std::vector<uint8_t> compressData(const std::vector<uint8_t>& data) {
    z_stream zs = {};
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
      throw std::runtime_error("Failed to initialize zlib deflate.");
    }

    zs.next_in = const_cast<Bytef*>(data.data());
    zs.avail_in = data.size();

    std::vector<uint8_t> compressed;
    uint8_t buffer[1024];

    do {
      zs.next_out = buffer;
      zs.avail_out = sizeof(buffer);

      if (deflate(&zs, Z_FINISH) == Z_STREAM_ERROR) {
        deflateEnd(&zs);
        throw std::runtime_error("zlib compression failed.");
      }

      compressed.insert(compressed.end(), buffer, buffer + (sizeof(buffer) - zs.avail_out));
    } while (zs.avail_out == 0);

    deflateEnd(&zs);
    return compressed;
  }

// Decompress using zlib
  std::vector<uint8_t> decompressData(const std::vector<uint8_t>& data) {
    z_stream zs = {};
    if (inflateInit(&zs) != Z_OK) {
      throw std::runtime_error("Failed to initialize zlib inflate.");
    }

    zs.next_in = const_cast<Bytef*>(data.data());
    zs.avail_in = data.size();

    std::vector<uint8_t> decompressed;
    uint8_t buffer[1024];

    do {
      zs.next_out = buffer;
      zs.avail_out = sizeof(buffer);

      int ret = inflate(&zs, Z_NO_FLUSH);
      if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
        inflateEnd(&zs);
        throw std::runtime_error("zlib decompression failed.");
      }

      decompressed.insert(decompressed.end(), buffer, buffer + (sizeof(buffer) - zs.avail_out));

      if (ret == Z_STREAM_END) break;
    } while (zs.avail_out == 0);

    inflateEnd(&zs);
    return decompressed;
  }

// usage
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

// prk and xof
// ADDED: KDF with Iterations and Info
  static const std::string KDF_INFO_STRING = "powered by Rain hashes created by Cris and DOSYAGO (aka DOSAYGO) over the years 2023 through 2025";
  static const int KDF_ITERATIONS = 8;
  static const int XOF_ITERATIONS = 4;
  static const int DE_ITERATIONS = 1;

  // ADDED: Derive PRK with iterations
  static std::vector<uint8_t> derivePRK(
    const std::vector<uint8_t> &seed,
    const std::vector<uint8_t> &salt,
    const std::vector<uint8_t> &ikm,
    HashAlgorithm algot,
    uint32_t hash_bits) {
      // Combine salt || ikm || info
      std::vector<uint8_t> combined;
      combined.reserve(salt.size() + ikm.size() + KDF_INFO_STRING.size());
      combined.insert(combined.end(), salt.begin(), salt.end());
      combined.insert(combined.end(), ikm.begin(), ikm.end());
      combined.insert(combined.end(), KDF_INFO_STRING.begin(), KDF_INFO_STRING.end());
      
      std::vector<uint8_t> prk(hash_bits / 8, 0);
      std::vector<uint8_t> temp(combined);

      // Convert first 8 bytes of seed vector to uint64_t (little-endian)
      uint64_t seed_num = 0;
      for (size_t j = 0; j < std::min(static_cast<size_t>(8), seed.size()); ++j) {
          seed_num |= static_cast<uint64_t>(seed[j]) << (j * 8);
      }
      
      // Iterate the hash function KDF_ITERATIONS times
      for (int i = 0; i < KDF_ITERATIONS; ++i) {
          invokeHash<false>(algot, seed_num, temp, prk, hash_bits);
          temp = prk; // Next iteration takes the output of the previous
      }
      
      return prk;
  }

  // ADDED: Extend Output with iterations and counter
  static std::vector<uint8_t> extendOutputKDF(
    const std::vector<uint8_t> &prk,
    size_t totalLen,
    HashAlgorithm algot,
    uint32_t hash_bits) {
      std::vector<uint8_t> out;
      out.reserve(totalLen);
      
      uint64_t counter = 1;
      std::vector<uint8_t> kn = prk; // Initial K_n = PRK
      
      while (out.size() < totalLen) {
          // Combine PRK || info || counter
          std::vector<uint8_t> combined;
          combined.reserve(kn.size() + KDF_INFO_STRING.size() + 8); // 8 bytes for counter
          combined.insert(combined.end(), prk.begin(), prk.end());
          combined.insert(combined.end(), KDF_INFO_STRING.begin(), KDF_INFO_STRING.end());
          
          // Append counter in big-endian
          for (int i = 7; i >= 0; --i) {
              combined.push_back(static_cast<uint8_t>((counter >> (i * 8)) & 0xFF));
          }
          
          // Iterate the hash function KDF_ITERATIONS times
          std::vector<uint8_t> temp(combined);
          std::vector<uint8_t> kn_next(hash_bits / 8, 0);
          for (int i = 0; i < KDF_ITERATIONS; ++i) {
              invokeHash<false>(algot, 0, temp, kn_next, hash_bits);
              temp = kn_next;
          }
          
          // Update K_n
          kn = kn_next;
          
          // Append to output
          size_t remaining = totalLen - out.size();
          size_t to_copy = std::min(static_cast<size_t>(kn.size()), remaining);
          out.insert(out.end(), kn.begin(), kn.begin() + to_copy);
          
          counter++;
      }
      
      out.resize(totalLen);
      return out;
  }

// encrypt/decrypt block mode
  /* Existing puzzleShowFileInfo function */
  static void puzzleShowFileInfo(const std::string &inFilename) {
      std::ifstream fin(inFilename, std::ios::binary);
      if (!fin.is_open()) {
          throw std::runtime_error("Cannot open file: " + inFilename);
      }

      uint32_t magicNumber;
      uint8_t version;
      uint8_t blockSize;
      uint8_t nonceSize;
      uint16_t hash_size;
      uint8_t hashNameLength;
      std::string hashName;
      uint8_t searchModeEnum;
      uint64_t originalSize;

      fin.read(reinterpret_cast<char*>(&magicNumber), sizeof(magicNumber));
      fin.read(reinterpret_cast<char*>(&version), sizeof(version));
      fin.read(reinterpret_cast<char*>(&blockSize), sizeof(blockSize));
      fin.read(reinterpret_cast<char*>(&nonceSize), sizeof(nonceSize));
      fin.read(reinterpret_cast<char*>(&hash_size), sizeof(hash_size));
      fin.read(reinterpret_cast<char*>(&hashNameLength), sizeof(hashNameLength));

      if (!fin.good()) {
          throw std::runtime_error("File too small or truncated header.");
      }

      hashName.resize(hashNameLength);
      fin.read(&hashName[0], hashNameLength);
      fin.read(reinterpret_cast<char*>(&searchModeEnum), sizeof(searchModeEnum));
      fin.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));

      fin.close();

      if (magicNumber != MagicNumber) {
          throw std::runtime_error("Invalid magic number (not an RCRY file).");
      }

      std::string searchMode;
      switch (searchModeEnum) {
          case 0x00: searchMode = "prefix"; break;
          case 0x01: searchMode = "sequence"; break;
          case 0x02: searchMode = "series"; break;
          case 0x03: searchMode = "scatter"; break;
          case 0x04: searchMode = "mapscatter"; break;
          case 0x05: searchMode = "parascatter"; break;
          default:   searchMode = "unknown"; break;
      }

      size_t totalBlocks = (originalSize + blockSize - 1) / blockSize;

      std::cout << "=== File Header Info ===\n";
      std::cout << "Magic: RCRY (0x" << std::hex << magicNumber << std::dec << ")\n";
      std::cout << "Version: " << static_cast<int>(version) << "\n";
      std::cout << "Block Size: " << static_cast<int>(blockSize) << "\n";
      std::cout << "Nonce Size: " << static_cast<int>(nonceSize) << "\n";
      std::cout << "Hash Size: " << hash_size << " bits\n";
      std::cout << "Hash Algorithm: " << hashName << "\n";
      std::cout << "Search Mode: " << searchMode << "\n";
      std::cout << "Compressed Plaintext Size: " << originalSize << " bytes\n";
      std::cout << "Total Blocks: " << totalBlocks << "\n";
      std::cout << "========================\n";
  }

  static void puzzleEncryptFileWithHeader(
      const std::string &inFilename,
      const std::string &outFilename,
      const std::string &key,
      HashAlgorithm algot,
      uint32_t hash_size,
      uint64_t seed,             // Used as IV
      std::vector<uint8_t> salt, // Provided salt
      size_t blockSize,
      size_t nonceSize,
      const std::string &searchMode,
      bool verbose,
      bool deterministicNonce
  ) {
      // 1) Read & compress plaintext
      std::ifstream fin(inFilename, std::ios::binary);
      if (!fin.is_open()) {
          throw std::runtime_error("Cannot open input file: " + inFilename);
      }
      std::vector<uint8_t> plainData(
          (std::istreambuf_iterator<char>(fin)),
          (std::istreambuf_iterator<char>())
      );
      fin.close();

      auto compressed = compressData(plainData);
      plainData = std::move(compressed);

      // 2) Prepare output & new FileHeader
      std::ofstream fout(outFilename, std::ios::binary);
      if (!fout.is_open()) {
          throw std::runtime_error("Cannot open output file: " + outFilename);
      }

      // 3) Determine searchModeEnum based on searchMode string
      uint8_t searchModeEnum = 0xFF; // Default for Stream Cipher Mode
      if (algot == HashAlgorithm::Rainbow || algot == HashAlgorithm::Rainstorm) { // Only for Block Cipher Modes
          if (searchMode == "prefix") {
              searchModeEnum = 0x00;
          }
          else if (searchMode == "sequence") {
              searchModeEnum = 0x01;
          }
          else if (searchMode == "series") {
              searchModeEnum = 0x02;
          }
          else if (searchMode == "scatter") {
              searchModeEnum = 0x03;
          }
          else if (searchMode == "mapscatter") {
              searchModeEnum = 0x04;
          }
          else if (searchMode == "parascatter") {
              searchModeEnum = 0x05;
          }
          else {
              throw std::runtime_error("Invalid search mode: " + searchMode);
          }
      }

      // 4) Build the new FileHeader
      FileHeader hdr{};
      hdr.magic        = MagicNumber;
      hdr.version      = 0x02;       // New version
      hdr.cipherMode   = 0x11;       // 0x11 for “Block Cipher + puzzle”
      hdr.blockSize    = static_cast<uint8_t>(blockSize);
      hdr.nonceSize    = static_cast<uint8_t>(nonceSize);
      hdr.hashSizeBits = hash_size;
      hdr.hashName     = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
      hdr.iv           = seed;       // The seed as our “IV”

      // 5) Assign provided salt
      hdr.saltLen = static_cast<uint8_t>(salt.size());
      hdr.salt    = salt;

      // 6) Assign original (compressed) size
      hdr.originalSize = plainData.size();

      // 7) Assign searchModeEnum
      hdr.searchModeEnum = searchModeEnum;

      // 8) Write the unified header
      writeFileHeader(fout, hdr);

      // 9) Prepare puzzle searching
      size_t totalBlocks = (hdr.originalSize + blockSize - 1) / blockSize;
      int progressInterval = 1'000'000;

      // 10) Derive a “master PRK” from (seed, salt, key)
      // Using your iterative KDF
      std::vector<uint8_t> keyBuf(key.begin(), key.end());
      // Convert seed (uint64_t) to vector<uint8_t>
      std::vector<uint8_t> seed_vec(8);
      for (size_t i = 0; i < 8; ++i) {
          seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
      }

      std::vector<uint8_t> prk = derivePRK(
          seed_vec,          // Converted seed as vector<uint8_t>
          salt,              // Provided salt
          keyBuf,            // IKM (Input Key Material)
          algot,             // Hash algorithm
          hash_size          // Hash size in bits
      );

      // 11) Extend that PRK into subkeys for each block
      // Each subkey is hash_size/8 bytes
      size_t subkeySize = hdr.hashSizeBits / 8;
      size_t totalNeeded = totalBlocks * subkeySize;
      std::vector<uint8_t> allSubkeys = extendOutputKDF(
          prk,
          totalNeeded,
          algot,
          hdr.hashSizeBits
      );

      // 12) Initialize RNG for non-deterministic nonce generation
      std::mt19937_64 rng(std::random_device{}());
      std::uniform_int_distribution<uint8_t> dist(0, 255);
      uint64_t nonceCounter = 0;

      size_t remaining = hdr.originalSize;
      // For mapscatter arrays
      uint8_t reverseMap[256 * 64] = {0};
      uint8_t reverseMapOffsets[256] = {0};

      // 13) Iterate over each block to perform puzzle encryption
      for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
          size_t thisBlockSize = std::min(blockSize, remaining);
          remaining -= thisBlockSize;

          // Extract this block
          std::vector<uint8_t> block(
              plainData.begin() + blockIndex * blockSize,
              plainData.begin() + blockIndex * blockSize + thisBlockSize
          );

          // Extract subkey for this block
          const size_t offset = blockIndex * subkeySize;
          if (offset + subkeySize > allSubkeys.size()) {
              throw std::runtime_error("Subkey index out of range.");
          }
          std::vector<uint8_t> blockSubkey(
              allSubkeys.begin() + offset,
              allSubkeys.begin() + offset + subkeySize
          );

          // -----------------------
          // parascatter branch
          // -----------------------
          if (searchModeEnum == 0x05) { // parascatter
            std::atomic<bool> done(false);
            std::vector<uint8_t> bestNonce(nonceSize);         // Shared best nonce
            std::vector<uint8_t> bestScatter(thisBlockSize);   // Shared best scatter indices
#ifdef _OPENMP
            #pragma omp parallel
#endif
            {
              uint64_t localTries = 0;
              std::vector<uint8_t> localNonce(nonceSize);
              std::vector<uint8_t> localScatterIndices(thisBlockSize, 0); // Thread-local
              std::vector<uint8_t> threadHashOut(hash_size / 8);
              std::bitset<64> localUsedIndices;
              std::mt19937_64 local_rng(std::random_device{}()); // Thread-local RNG
              std::uniform_int_distribution<uint8_t> local_dist(0, 255);

              while (!done.load(std::memory_order_acquire)) {
                // Generate nonce
                if (deterministicNonce) {
#ifdef _OPENMP
                  uint64_t current = omp_get_thread_num() + omp_get_num_threads() * (localTries + 1);
#else
                  uint64_t current = (localTries + 1);
#endif                 
                  for (size_t i = 0; i < nonceSize; i++) {
                    localNonce[i] = static_cast<uint8_t>((current >> (i * 8)) & 0xFF);
                  }
                } else {
                  for (size_t i = 0; i < nonceSize; i++) {
                    localNonce[i] = local_dist(local_rng);
                  }
                }

                // Build trial buffer
                std::vector<uint8_t> trial(keyBuf);
                trial.insert(trial.end(), localNonce.begin(), localNonce.end());

                // Hash it
                invokeHash<bswap>(algot, seed, trial, threadHashOut, hash_size);

                // Check scatter indices
                bool allFound = true;
                localUsedIndices.reset();
                for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                  auto it = threadHashOut.begin();
                  while (it != threadHashOut.end()) {
                    it = std::find(it, threadHashOut.end(), block[byteIdx]);
                    if (it != threadHashOut.end()) {
                      uint8_t idx = static_cast<uint8_t>(std::distance(threadHashOut.begin(), it));
                      if (!localUsedIndices.test(idx)) {
                        localScatterIndices[byteIdx] = idx;
                        localUsedIndices.set(idx);
                        break;
                      }
                      ++it;
                    } else {
                      allFound = false;
                      break;
                    }
                  }
                  if (!allFound) break;
                }

                if (allFound) {
                  // Critical section to update shared variables
#ifdef _OPENMP
                  #pragma omp critical
#endif
                  {
                    if (!done.load(std::memory_order_relaxed)) {
                      bestNonce = localNonce;
                      bestScatter = localScatterIndices;
                      done.store(true, std::memory_order_release); // Synchronize with other threads
                    }
                  }
                }

                // Periodic progress reporting
                if (localTries % progressInterval == 0 && !done.load(std::memory_order_relaxed)) {
#ifdef _OPENMP
                  # pragma omp critical
#endif

                  {
                    if (!done.load(std::memory_order_relaxed)) {
                      std::cerr << "\r[Parascatter] Block " << blockIndex + 1
                                << "/" << totalBlocks << ", thread "
#ifdef _OPENMP
                                << omp_get_thread_num() << ": "
#else
                                << "Non-parallel: "
#endif
                                << localTries << " tries..." << std::flush;
                    }
                  }
                }
                localTries++;
              } // End while
            } // End parallel


            // Write results after parallel region
            if (done.load(std::memory_order_relaxed)) {
                fout.write(reinterpret_cast<const char*>(bestNonce.data()), nonceSize);
                fout.write(reinterpret_cast<const char*>(bestScatter.data()), bestScatter.size());

                // Verbose output for written data
                if (verbose) {
                    std::cout << "\n[Parascatter] Final Nonce: ";
                    for (const auto& byte : bestNonce) {
                        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
                    }
                    std::cout << std::dec << "\n[Parascatter] Scatter Indices: ";
                    for (const auto& idx : bestScatter) {
                        std::cout << static_cast<int>(idx) << " ";
                    }
                    std::cout << std::endl;
                }

                // Display block progress
                if (verbose) {
                    std::cerr << "\r[Enc] Block " << blockIndex + 1 << "/" << totalBlocks << " processed.\n";
                }
            } else {
                throw std::runtime_error("[Parascatter] No solution found.");
            }

          // ------------------------------------------------------
          // other modes in else branch
          // ------------------------------------------------------
          } else {
              // We'll fill these once a solution is found
              bool found = false;
              std::vector<uint8_t> chosenNonce(nonceSize, 0);
              std::vector<uint8_t> scatterIndices(thisBlockSize, 0);

              for (uint64_t tries = 0; ; tries++) {
                  // Generate the nonce
                  if (deterministicNonce) {
                      for (size_t i = 0; i < nonceSize; i++) {
                          chosenNonce[i] = static_cast<uint8_t>((nonceCounter >> (i * 8)) & 0xFF);
                      }
                      nonceCounter++;
                  }
                  else {
                      for (size_t i = 0; i < nonceSize; i++) {
                          chosenNonce[i] = dist(rng);
                      }
                  }

                  // Build trial buffer
                  std::vector<uint8_t> trial(blockSubkey);
                  trial.insert(trial.end(), chosenNonce.begin(), chosenNonce.end());

                  // Hash it
                  std::vector<uint8_t> hashOut(hash_size / 8);
                  invokeHash<false>(algot, seed, trial, hashOut, hash_size);

                  if (searchModeEnum == 0x00) { // prefix
                      if (hashOut.size() >= thisBlockSize &&
                          std::equal(block.begin(), block.end(), hashOut.begin())) {
                          scatterIndices.assign(thisBlockSize, 0);
                          found = true;
                      }
                  }
                  else if (searchModeEnum == 0x01) { // sequence
                      for (size_t i = 0; i <= hashOut.size() - thisBlockSize; i++) {
                          if (std::equal(block.begin(), block.end(), hashOut.begin() + i)) {
                              uint8_t startIdx = static_cast<uint8_t>(i);
                              scatterIndices.assign(thisBlockSize, startIdx);
                              found = true;
                              break;
                          }
                      }
                  }
                  else if (searchModeEnum == 0x02) { // series
                      bool allFound = true;
                      std::bitset<64> usedIndices;
                      usedIndices.reset();
                      auto it = hashOut.begin();

                      for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                          while (it != hashOut.end()) {
                              it = std::find(it, hashOut.end(), block[byteIdx]);
                              if (it != hashOut.end()) {
                                  uint8_t idx = static_cast<uint8_t>(std::distance(hashOut.begin(), it));
                                  if (!usedIndices.test(idx)) {
                                      scatterIndices[byteIdx] = idx;
                                      usedIndices.set(idx);
                                      break;
                                  }
                                  ++it;
                              }
                              else {
                                  allFound = false;
                                  break;
                              }
                          }
                          if (it == hashOut.end()) {
                              allFound = false;
                              break;
                          }
                      }

                      if (allFound) {
                          found = true;
                          if (verbose) {
                              std::cout << "Series Indices: ";
                              for (auto idx : scatterIndices) {
                                  std::cout << static_cast<int>(idx) << " ";
                              }
                              std::cout << std::endl;
                          }
                      }
                  }
                  else if (searchModeEnum == 0x03) { // scatter
                      bool allFound = true;
                      std::bitset<64> usedIndices;
                      usedIndices.reset();

                      for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                          auto it = hashOut.begin();
                          while (it != hashOut.end()) {
                              it = std::find(it, hashOut.end(), block[byteIdx]);
                              if (it != hashOut.end()) {
                                  uint8_t idx = static_cast<uint8_t>(std::distance(hashOut.begin(), it));
                                  if (!usedIndices.test(idx)) {
                                      scatterIndices[byteIdx] = idx;
                                      usedIndices.set(idx);
                                      break;
                                  }
                                  ++it;
                              }
                              else {
                                  allFound = false;
                                  break;
                              }
                          }
                          if (it == hashOut.end()) {
                              allFound = false;
                              break;
                          }
                      }

                      if (allFound) {
                          found = true;
                          if (verbose) {
                              std::cout << "Scatter Indices: ";
                              for (auto idx : scatterIndices) {
                                  std::cout << static_cast<int>(idx) << " ";
                              }
                              std::cout << std::endl;
                          }
                      }
                  }
                  else if (searchModeEnum == 0x04) { // mapscatter
                      // Reset offsets
                      std::fill(std::begin(reverseMapOffsets), std::end(reverseMapOffsets), 0);

                      // Fill the map with all positions of each byte in hashOut
                      for (uint8_t i = 0; i < hashOut.size(); i++) {
                          uint8_t b = hashOut[i];
                          reverseMap[b * 64 + reverseMapOffsets[b]] = i;
                          reverseMapOffsets[b]++;
                      }

                      bool allFound = true;
                      for (size_t byteIdx = 0; byteIdx < thisBlockSize; ++byteIdx) {
                          uint8_t targetByte = block[byteIdx];
                          if (reverseMapOffsets[targetByte] == 0) {
                              allFound = false;
                              break;
                          }
                          reverseMapOffsets[targetByte]--;
                          scatterIndices[byteIdx] =
                              reverseMap[targetByte * 64 + reverseMapOffsets[targetByte]];
                      }

                      if (allFound) {
                          found = true;
                          if (verbose) {
                              std::cout << "Scatter Indices: ";
                              for (auto idx : scatterIndices) {
                                  std::cout << static_cast<int>(idx) << " ";
                              }
                              std::cout << std::endl;
                          }
                      }
                  }

                  if (found) {
                      // For non-parallel modes, write nonce and indices
                      fout.write(reinterpret_cast<const char*>(chosenNonce.data()), nonceSize);
                      if (searchModeEnum == 0x02 || searchModeEnum == 0x03 || searchModeEnum == 0x04) {
                          fout.write(reinterpret_cast<const char*>(scatterIndices.data()), scatterIndices.size());
                      }
                      else {
                          // prefix or sequence
                          uint8_t startIdx = scatterIndices[0];
                          fout.write(reinterpret_cast<const char*>(&startIdx), sizeof(startIdx));
                      }
                      break; // done with this block
                  }

                  // Periodic progress for non-parallel tries
                  if (tries % progressInterval == 0) {
                      std::cerr << "\r[Enc] Mode: " << searchMode
                                << ", Block " << (blockIndex + 1) << "/" << totalBlocks
                                << ", " << tries << " tries..." << std::flush;
                  }
              } // end tries loop

              // Display block progress
              if (verbose) {
                  std::cerr << "\r[Enc] Block " << blockIndex + 1 << "/" << totalBlocks << " processed.\n";
              }

          } // end else (other modes)

      } // end block loop

      fout.close();
      std::cout << "[Enc] Block-based puzzle encryption with subkeys complete: " << outFilename << "\n";
  }

  static void puzzleDecryptFileWithHeader(
      const std::string &inFilename,
      const std::string &outFilename,
      const std::string &key
  ) {
      // 1) Open input file & read FileHeader
      std::ifstream fin(inFilename, std::ios::binary);
      if (!fin.is_open()) {
          throw std::runtime_error("Cannot open ciphertext file: " + inFilename);
      }

      // Read unified FileHeader
      FileHeader hdr = readFileHeader(fin);
      if (hdr.magic != MagicNumber) { // "RCRY"
          throw std::runtime_error("Invalid magic number in file header.");
      }

      // Determine cipher mode
      if (hdr.cipherMode != 0x11) { // 0x11 = Block Cipher Mode
          throw std::runtime_error("File is not in Block Cipher mode (expected cipherMode=0x11).");
      }

      // Validate hash algorithm
      HashAlgorithm algot = HashAlgorithm::Unknown;
      if (hdr.hashName == "rainbow") {
          algot = HashAlgorithm::Rainbow;
      }
      else if (hdr.hashName == "rainstorm") {
          algot = HashAlgorithm::Rainstorm;
      }
      else {
          throw std::runtime_error("Unsupported hash algorithm in header: " + hdr.hashName);
      }

      // Read the rest of the file as cipher data
      std::vector<uint8_t> cipherData(
          (std::istreambuf_iterator<char>(fin)),
          (std::istreambuf_iterator<char>())
      );
      fin.close();

      // 2) Derive PRK using KDF
      // Assume 'derivePRK' and 'extendOutputKDF' are defined in tool.h and in scope
      std::vector<uint8_t> ikm(key.begin(), key.end());
      // Convert seed (uint64_t) to vector<uint8_t>
      std::vector<uint8_t> seed_vec(8);
      uint64_t seed = hdr.iv;
      for (size_t i = 0; i < 8; ++i) {
          seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
      }

      std::vector<uint8_t> prk = derivePRK(
          seed_vec,          // Converted seed as vector<uint8_t>
          hdr.salt,              // Provided salt
          ikm,            // IKM (Input Key Material)
          algot,             // Hash algorithm
          hdr.hashSizeBits          // Hash size in bits
      );

      // 3) Extend PRK into subkeys for each block
      size_t totalBlocks = (hdr.originalSize + hdr.blockSize - 1) / hdr.blockSize;
      size_t subkeySize = hdr.hashSizeBits / 8;
      size_t totalNeeded = totalBlocks * subkeySize;
      std::vector<uint8_t> allSubkeys = extendOutputKDF(
          prk,
          totalNeeded,
          algot,
          hdr.hashSizeBits
      );

      // 4) Prepare to reconstruct plaintext
      std::vector<uint8_t> plaintextAccumulated;
      plaintextAccumulated.reserve(hdr.originalSize);

      size_t cipherOffset = 0;
      size_t recoveredSoFar = 0;

      for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
          size_t thisBlockSize = std::min<size_t>(static_cast<size_t>(hdr.blockSize), hdr.originalSize - recoveredSoFar);
          std::vector<uint8_t> block;

          // 4.1) Extract storedNonce
          if (cipherOffset + hdr.nonceSize > cipherData.size()) {
              throw std::runtime_error("Unexpected end of cipher data when reading nonce.");
          }
          std::vector<uint8_t> storedNonce(
              cipherData.begin() + cipherOffset,
              cipherData.begin() + cipherOffset + hdr.nonceSize
          );
          cipherOffset += hdr.nonceSize;

          // 4.2) Read scatterIndices or startIndex based on searchModeEnum
          std::vector<uint8_t> scatterIndices;
          uint8_t startIndex = 0;
          if (hdr.searchModeEnum == 0x02 || hdr.searchModeEnum == 0x03 ||
              hdr.searchModeEnum == 0x04 || hdr.searchModeEnum == 0x05) {
              // Series, Scatter, MapScatter, Parascatter
              if (cipherOffset + thisBlockSize > cipherData.size()) {
                  throw std::runtime_error("Unexpected end of cipher data when reading scatter indices.");
              }
              scatterIndices.assign(
                  cipherData.begin() + cipherOffset,
                  cipherData.begin() + cipherOffset + thisBlockSize
              );
              cipherOffset += thisBlockSize;
          }
          else {
              // Prefix or Sequence
              if (cipherOffset + 1 > cipherData.size()) {
                  throw std::runtime_error("Unexpected end of cipher data when reading start index.");
              }
              startIndex = cipherData[cipherOffset];
              cipherOffset += 1;
          }

          // 4.3) Derive subkey for this block
          size_t subkeyOffset = blockIndex * subkeySize;
          if (subkeyOffset + subkeySize > allSubkeys.size()) {
              throw std::runtime_error("Subkey index out of range.");
          }
          std::vector<uint8_t> blockSubkey(
              allSubkeys.begin() + subkeyOffset,
              allSubkeys.begin() + subkeyOffset + subkeySize
          );

          // 4.4) Recompute the hash using blockSubkey and storedNonce
          std::vector<uint8_t> trial(blockSubkey);
          trial.insert(trial.end(), storedNonce.begin(), storedNonce.end());

          std::vector<uint8_t> hashOut(hdr.hashSizeBits / 8);
          invokeHash<false>(algot, hdr.iv, trial, hashOut, hdr.hashSizeBits); // Assuming 'hdr.iv' is passed correctly

          // 4.5) Reconstruct plaintext block based on searchModeEnum
          if (hdr.searchModeEnum == 0x00) { // Prefix
              if (hashOut.size() < thisBlockSize) {
                  throw std::runtime_error("[Dec] Hash output smaller than block size for Prefix mode.");
              }
              block.assign(hashOut.begin(), hashOut.begin() + thisBlockSize);
          }
          else if (hdr.searchModeEnum == 0x01) { // Sequence
              if (startIndex + thisBlockSize > hashOut.size()) {
                  throw std::runtime_error("[Dec] Start index out of bounds in Sequence mode.");
              }
              block.assign(hashOut.begin() + startIndex, hashOut.begin() + startIndex + thisBlockSize);
          }
          else if (hdr.searchModeEnum == 0x02 || hdr.searchModeEnum == 0x03 ||
                   hdr.searchModeEnum == 0x04 || hdr.searchModeEnum == 0x05) { // Series, Scatter, MapScatter, Parascatter
              block.reserve(thisBlockSize);
              for (size_t j = 0; j < thisBlockSize; j++) {
                  uint8_t idx = scatterIndices[j];
                  if (idx >= hashOut.size()) {
                      throw std::runtime_error("[Dec] Scatter index out of range.");
                  }
                  block.push_back(hashOut[idx]);
              }
          }
          else {
              throw std::runtime_error("Invalid search mode enum in decryption.");
          }

          // 4.6) Accumulate the reconstructed block
          plaintextAccumulated.insert(
              plaintextAccumulated.end(),
              block.begin(),
              block.end()
          );
          recoveredSoFar += thisBlockSize;

          // 4.7) Progress Reporting
          if (blockIndex % 100 == 0) {
              std::cerr << "\r[Dec] Processing block " << (blockIndex + 1) << " / " << totalBlocks << "..." << std::flush;
          }
      } // end for(blockIndex)

      std::cout << "\n[Dec] Ciphertext blocks decrypted successfully.\n";

      // 5) Decompress the accumulated plaintext
      std::vector<uint8_t> decompressedData = decompressData(plaintextAccumulated);
      if (plaintextAccumulated.size() != hdr.originalSize) {
          throw std::runtime_error("Compressed data size does not match original size.");
      }

      // 6) Write the decompressed plaintext to output file
      std::ofstream fout(outFilename, std::ios::binary);
      if (!fout.is_open()) {
          throw std::runtime_error("Cannot open output file for decompressed plaintext: " + outFilename);
      }

      fout.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
      fout.close();

      std::cout << "[Dec] Decompressed plaintext written to: " << outFilename << "\n";
  }


