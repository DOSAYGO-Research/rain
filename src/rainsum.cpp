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
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <iterator>
#include "tool.h" // for invokeHash, etc.


// ------------------------------------------------------------------
// Helper functions for mining
// ------------------------------------------------------------------
/*
static std::vector<uint8_t> hexToBytes(const std::string& hexstr) {
  if (hexstr.size() % 2 != 0) {
    throw std::runtime_error("Hex prefix must have even length.");
  }
  std::vector<uint8_t> bytes(hexstr.size() / 2);
  for (size_t i = 0; i < bytes.size(); ++i) {
    unsigned int val = 0;
    std::stringstream ss;
    ss << std::hex << hexstr.substr(i * 2, 2);
    ss >> val;
    bytes[i] = static_cast<uint8_t>(val);
  }
  return bytes;
}
*/

inline bool hasPrefix(const std::vector<uint8_t>& hash_output, const std::vector<uint8_t>& prefix_bytes) {
  if (prefix_bytes.size() > hash_output.size()) return false;
  return std::equal(prefix_bytes.begin(), prefix_bytes.end(), hash_output.begin());
}

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

static void puzzleEncryptFileWithHeader(
  const std::string &inFilename,
  const std::string &outFilename,
  const std::string &key,
  HashAlgorithm algot,
  uint32_t hash_size,
  uint64_t seed,
  size_t blockSize,
  size_t nonceSize,
  const std::string &searchMode
) {
  // Open input file
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("Cannot open input file: " + inFilename);
  }
  std::vector<uint8_t> plainData((std::istreambuf_iterator<char>(fin)),
                                 (std::istreambuf_iterator<char>()));
  fin.close();

  // Open output file
  std::ofstream fout(outFilename, std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("Cannot open output file: " + outFilename);
  }

  // Write header
  uint32_t magicNumber = 0x52435259; // "RCRY"
  uint8_t version = 0x01;
  uint8_t blockSize8 = static_cast<uint8_t>(blockSize);
  uint8_t nonceSize8 = static_cast<uint8_t>(nonceSize);
  uint16_t digestSize16 = static_cast<uint16_t>(hash_size);
  uint8_t searchModeEnum = 0x00; // Default to 'prefix'
  if (searchMode == "prefix") searchModeEnum = 0x00;
  else if (searchMode == "sequence") searchModeEnum = 0x01;
  else if (searchMode == "series") searchModeEnum = 0x02;
  else if (searchMode == "scatter") searchModeEnum = 0x03;

  std::string hashName = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
  uint8_t hashNameLength = static_cast<uint8_t>(hashName.size());

  fout.write(reinterpret_cast<const char*>(&magicNumber), sizeof(magicNumber));
  fout.write(reinterpret_cast<const char*>(&version), sizeof(version));
  fout.write(reinterpret_cast<const char*>(&blockSize8), sizeof(blockSize8));
  fout.write(reinterpret_cast<const char*>(&nonceSize8), sizeof(nonceSize8));
  fout.write(reinterpret_cast<const char*>(&digestSize16), sizeof(digestSize16));
  fout.write(reinterpret_cast<const char*>(&hashNameLength), sizeof(hashNameLength));
  fout.write(hashName.data(), hashName.size());
  fout.write(reinterpret_cast<const char*>(&searchModeEnum), sizeof(searchModeEnum));

  uint64_t originalSize = plainData.size();
  fout.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));

  // Partition plaintext into blocks
  size_t totalBlocks = (plainData.size() + blockSize - 1) / blockSize;
  std::vector<uint8_t> hashOut(hash_size / 8);
  std::vector<uint8_t> keyBuf(key.begin(), key.end());

  // Prepare random generator for nonce
  std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<uint64_t> dist;

  for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
      size_t thisBlockSize = std::min(blockSize, remaining);
      std::vector<uint8_t> block(
          plainData.begin() + blockIndex * blockSize,
          plainData.begin() + blockIndex * blockSize + thisBlockSize
      );

      bool found = false;
      std::vector<uint8_t> chosenNonce(nonceSize, 0);
      // For 'scatter'/'series' mode, we need to store multiple indices
      std::vector<size_t> scatterIndices(thisBlockSize, 0); // Initialize with zeros

      for (uint64_t tries = 0; ; tries++) { // No fixed MAX_TRIES
          // Generate random nonce
          for (size_t i = 0; i < nonceSize; i++) {
              chosenNonce[i] = static_cast<uint8_t>(dist(rng) & 0xFF);
          }

          // Build trial buffer
          std::vector<uint8_t> trial(keyBuf);
          trial.insert(trial.end(), chosenNonce.begin(), chosenNonce.end());

          // Hash it
          invokeHash<bswap>(algot, seed, trial, hashOut, hash_size);

          if (searchModeEnum == 0x00) { // Prefix
              if (hashOut.size() >= thisBlockSize &&
                  std::equal(block.begin(), block.end(), hashOut.begin())) {
                  // Found at the front
                  scatterIndices.assign(thisBlockSize, 0); // All indices are 0
                  found = true;
              }
          }
          else if (searchModeEnum == 0x01) { // Sequence
              // Search for block as a contiguous substring
              for (size_t i = 0; i <= hashOut.size() - thisBlockSize; i++) {
                  if (std::equal(block.begin(), block.end(), hashOut.begin() + i)) {
                      scatterIndices.assign(thisBlockSize, i); // All bytes start at 'i'
                      found = true;
                      break;
                  }
              }
          }
          else if (searchModeEnum == 0x02) { // Series (same as Scatter)
              bool allFound = true;
              for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                  auto it = std::find(hashOut.begin(), hashOut.end(), block[byteIdx]);
                  if (it != hashOut.end()) {
                      scatterIndices[byteIdx] = std::distance(hashOut.begin(), it);
                  }
                  else {
                      allFound = false;
                      break;
                  }
              }
              if (allFound) {
                  found = true;
              }
          }

          if (found) break;

          // Optional: Implement a maximum number of tries to prevent infinite loops
          // Example:
          /*
          if (tries > MAX_TRIES) {
              throw std::runtime_error("Failed to find a suitable nonce for block " + std::to_string(blockIndex));
          }
          */

          if (tries % 1'000'000 == 0) {
              std::cerr << "\r[Enc] Block " << blockIndex << ", " << tries << " tries... " << std::flush;
          }
      }

      // Write nonce and indices
      fout.write(reinterpret_cast<const char*>(chosenNonce.data()), nonceSize);
      if (searchModeEnum == 0x02) { // Series/Scatter
          // Write each index as size_t
          fout.write(reinterpret_cast<const char*>(scatterIndices.data()), scatterIndices.size() * sizeof(size_t));
      }
      else { // Prefix and Sequence
          // Write single start index (uint8_t)
          uint8_t startIndex = static_cast<uint8_t>(scatterIndices[0]);
          fout.write(reinterpret_cast<const char*>(&startIndex), sizeof(startIndex));
      }
  }

  fout.close();
  std::cout << "[Enc] Encryption complete: " << outFilename << "\n";
}

static void puzzleDecryptFileWithHeader(
    const std::string &inFilename,
    const std::string &outFilename,
    const std::string &key,
    uint64_t seed
) {
    // Open input file
    std::ifstream fin(inFilename, std::ios::binary);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open ciphertext file: " + inFilename);
    }

    // Read header
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

    hashName.resize(hashNameLength);
    fin.read(&hashName[0], hashNameLength);
    fin.read(reinterpret_cast<char*>(&searchModeEnum), sizeof(searchModeEnum));
    fin.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));

    if (magicNumber != 0x52435259) { // Replace with your actual magic number
        throw std::runtime_error("Invalid magic number in header.");
    }

    HashAlgorithm algot = (hashName == "rainbow") ? HashAlgorithm::Rainbow :
                           (hashName == "rainstorm") ? HashAlgorithm::Rainstorm :
                           HashAlgorithm::Unknown;

    if (algot == HashAlgorithm::Unknown) {
        throw std::runtime_error("Unsupported hash algorithm in header.");
    }

    // Read the rest of the file as cipher data
    std::vector<uint8_t> cipherData(
        (std::istreambuf_iterator<char>(fin)),
        (std::istreambuf_iterator<char>())
    );
    fin.close();

    // Each block consists of:
    // - nonceSize bytes + 1 byte (prefix/sequence) OR
    // - nonceSize bytes + (blockSize * sizeof(size_t)) bytes (series/scatter)
    size_t blockCipherSize;
    if (searchModeEnum == 0x02) { // Series/Scatter
        blockCipherSize = nonceSize + (blockSize * sizeof(size_t));
    }
    else { // Prefix/Sequence
        blockCipherSize = nonceSize + 1;
    }

    if (cipherData.size() % blockCipherSize != 0) {
        throw std::runtime_error("[Dec] Cipher data not multiple of blockCipherSize bytes after header!");
    }
    size_t totalBlocks = cipherData.size() / blockCipherSize;

    // Prepare output file
    std::ofstream fout(outFilename, std::ios::binary);
    if (!fout.is_open()) {
        throw std::runtime_error("Cannot open output file for plaintext: " + outFilename);
    }

    // Reconstruct plaintext blocks
    std::vector<uint8_t> keyBuf(key.begin(), key.end());
    std::vector<uint8_t> hashOut(hash_size / 8);

    size_t recoveredSoFar = 0;

    for (size_t i = 0; i < totalBlocks; i++) {
        // Extract nonce
        std::vector<uint8_t> storedNonce(cipherData.begin() + i * blockCipherSize,
                                        cipherData.begin() + i * blockCipherSize + nonceSize);

        // Read indices based on search mode
        std::vector<size_t> scatterIndices;
        uint8_t startIndex = 0;
        if (searchModeEnum == 0x02) { // Series/Scatter
            scatterIndices.resize(blockSize);
            for (size_t j = 0; j < blockSize; j++) {
                size_t idx;
                fin.read(reinterpret_cast<char*>(&idx), sizeof(idx));
                scatterIndices[j] = idx;
            }
        }
        else { // Prefix/Sequence
            fin.read(reinterpret_cast<char*>(&startIndex), sizeof(startIndex));
        }

        // Recompute the hash
        std::vector<uint8_t> trial(keyBuf);
        trial.insert(trial.end(), storedNonce.begin(), storedNonce.end());
        invokeHash<bswap>(algot, seed, trial, hashOut, hash_size);

        // Determine the size of the current block
        size_t remaining = originalSize - recoveredSoFar;
        size_t thisBlockSize = std::min(static_cast<size_t>(blockSize), remaining);

        std::vector<uint8_t> plaintextBlock;

        if (searchModeEnum == 0x00) { // Prefix
            plaintextBlock.assign(hashOut.begin(), hashOut.begin() + thisBlockSize);
        }
        else if (searchModeEnum == 0x01) { // Sequence
            plaintextBlock.assign(hashOut.begin() + startIndex,
                                  hashOut.begin() + startIndex + thisBlockSize);
        }
        else if (searchModeEnum == 0x02) { // Series/Scatter
            plaintextBlock.reserve(thisBlockSize);
            for (size_t j = 0; j < thisBlockSize; j++) {
                if (scatterIndices[j] < hashOut.size()) {
                    plaintextBlock.push_back(hashOut[scatterIndices[j]]);
                }
                else {
                    throw std::runtime_error("[Dec] Scatter index out of range.");
                }
            }
        }

        // Write the plaintext block
        if (!plaintextBlock.empty()) {
            fout.write(reinterpret_cast<const char*>(plaintextBlock.data()), thisBlockSize);
            recoveredSoFar += thisBlockSize;
        }

        // Early exit if all data is recovered
        if (recoveredSoFar >= originalSize) {
            break;
        }
    }

    fout.close();
    std::cout << "[Dec] Ciphertext " << inFilename << " decrypted successfully.\n";
}


// The main function
int main(int argc, char** argv) {
    try {
        // Initialize cxxopts with program name and description
        cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash, or perform puzzle-based encryption/decryption.");

        // Define command-line options
        options.add_options()
            ("m,mode", "Mode: digest, stream, enc, dec",
                cxxopts::value<std::string>()->default_value("digest"))
            ("v,version", "Print version")
            ("a,algorithm", "Specify the hash algorithm to use (rainbow, rainstorm)",
                cxxopts::value<std::string>()->default_value("bow"))
            ("s,size", "Specify the size of the hash (e.g., 64, 128, 256, 512)",
                cxxopts::value<uint32_t>()->default_value("256"))
            ("block-size", "Block size in bytes for puzzle-based encryption (1-255)",
                cxxopts::value<uint8_t>()->default_value("3"))
            ("n,nonce-size", "Size of the nonce in bytes (1-255)",
                cxxopts::value<size_t>()->default_value("8"))
            ("search-mode", "Search mode: prefix, sequence, series, scatter",
                cxxopts::value<std::string>()->default_value("prefix"))
            ("o,output-file", "Output file",
                cxxopts::value<std::string>()->default_value("/dev/stdout"))
            ("t,test-vectors", "Calculate the hash of the standard test vectors",
                cxxopts::value<bool>()->default_value("false"))
            ("l,output-length", "Output length in hash iterations (stream mode)",
                cxxopts::value<uint64_t>()->default_value("1000000"))
            ("seed", "Seed value",
                cxxopts::value<std::string>()->default_value("0"))
            // Mining options
            ("mine-mode", "Mining mode: chain, nonceInc, nonceRand",
                cxxopts::value<std::string>()->default_value("None"))
            ("match-prefix", "Hex prefix to match in mining tasks",
                cxxopts::value<std::string>()->default_value(""))
            // Encrypt / Decrypt options
            ("p,puzzle-len", "Number of bytes in the puzzle prefix [default: 3]",
                cxxopts::value<size_t>()->default_value("3"))
            ("h,help", "Print usage");

        // Parse command-line options
        auto result = options.parse(argc, argv);

        // Handle help and version immediately
        if (result.count("help")) {
            std::cout << options.help() << "\n";
            return 0;
        }

        if (result.count("version")) {
            std::cout << "rainsum version: 1.0.0\n"; // Replace with actual VERSION
            return 0;
        }

        // Access and validate options

        // Puzzle Length
        size_t puzzleLen = result["puzzle-len"].as<size_t>();

        // Hash Size
        uint32_t hash_size = result["size"].as<uint32_t>();

        // Block Size
        uint8_t blockSize = result["block-size"].as<uint8_t>();
        if (blockSize == 0 || blockSize > 255) {
            throw std::runtime_error("Block size must be between 1 and 255 bytes.");
        }

        // Nonce Size
        size_t nonceSize = result["nonce-size"].as<size_t>();
        if (nonceSize == 0 || nonceSize > 255) {
            throw std::runtime_error("Nonce size must be between 1 and 255 bytes.");
        }

        // Search Mode
        std::string searchMode = result["search-mode"].as<std::string>();
        if (searchMode != "prefix" && searchMode != "sequence" &&
            searchMode != "series" && searchMode != "scatter") {
            throw std::runtime_error("Invalid search mode: " + searchMode);
        }

        // Convert Seed (either numeric or hash string)
        std::string seed_str = result["seed"].as<std::string>();
        uint64_t seed;
        if (!(std::istringstream(seed_str) >> seed)) {
            seed = hash_string_to_64_bit(seed_str);
        }

        // Determine Mode
        std::string modeStr = result["mode"].as<std::string>();
        Mode mode;
        if (modeStr == "digest") mode = Mode::Digest;
        else if (modeStr == "stream") mode = Mode::Stream;
        else if (modeStr == "enc") mode = Mode::Enc;
        else if (modeStr == "dec") mode = Mode::Dec;
        else throw std::runtime_error("Invalid mode: " + modeStr);

        // Determine Hash Algorithm
        std::string algorithm = result["algorithm"].as<std::string>();
        HashAlgorithm algot = getHashAlgorithm(algorithm);
        if (algot == HashAlgorithm::Unknown) {
            throw std::runtime_error("Unsupported algorithm string: " + algorithm);
        }

        // Validate Hash Size based on Algorithm
        if (algot == HashAlgorithm::Rainbow) {
            if (hash_size != 64 && hash_size != 128 && hash_size != 256) {
                throw std::runtime_error("Invalid size for Rainbow (must be 64, 128, or 256).");
            }
        }
        else if (algot == HashAlgorithm::Rainstorm) {
            if (hash_size != 64 && hash_size != 128 && hash_size != 256 && hash_size != 512) {
                throw std::runtime_error("Invalid size for Rainstorm (must be 64, 128, 256, or 512).");
            }
        }

        // Test Vectors
        bool use_test_vectors = result["test-vectors"].as<bool>();

        // Output Length
        uint64_t output_length = result["output-length"].as<uint64_t>();

        // Adjust output_length based on mode
        if (mode == Mode::Digest) {
            output_length = hash_size / 8;
        }
        else if (mode == Mode::Stream) {
            output_length *= hash_size / 8;
        }

        // Mining arguments
        std::string mine_mode_str = result["mine-mode"].as<std::string>();
        MineMode mine_mode;
        if (mine_mode_str == "chain") mine_mode = MineMode::Chain;
        else if (mine_mode_str == "nonceInc") mine_mode = MineMode::NonceInc;
        else if (mine_mode_str == "nonceRand") mine_mode = MineMode::NonceRand;
        else mine_mode = MineMode::None;

        std::string prefixHex = result["match-prefix"].as<std::string>();

        // Unmatched arguments might be input file
        std::string inpath;
        if (!result.unmatched().empty()) {
            inpath = result.unmatched().front();
        }

        // Handle Mining Modes
        if (mine_mode != MineMode::None) {
            if (prefixHex.empty()) {
                throw std::runtime_error("You must specify --match-prefix for mining modes.");
            }

            // Convert prefix from hex
            auto hexToBytes = [&](const std::string &hexstr) -> std::vector<uint8_t> {
                if (hexstr.size() % 2 != 0) {
                    throw std::runtime_error("Hex prefix must have even length.");
                }
                std::vector<uint8_t> bytes(hexstr.size() / 2);
                for (size_t i = 0; i < bytes.size(); ++i) {
                    unsigned int val = 0;
                    std::stringstream ss;
                    ss << std::hex << hexstr.substr(i*2, 2);
                    ss >> val;
                    bytes[i] = static_cast<uint8_t>(val);
                }
                return bytes;
            };

            std::vector<uint8_t> prefixBytes = hexToBytes(prefixHex);

            switch (mine_mode) {
                case MineMode::Chain:
                    mineChain(algot, seed, hash_size, prefixBytes);
                    return 0;
                case MineMode::NonceInc:
                    mineNonceInc(algot, seed, hash_size, prefixBytes, inpath);
                    return 0;
                case MineMode::NonceRand:
                    mineNonceRand(algot, seed, hash_size, prefixBytes, inpath);
                    return 0;
                default:
                    throw std::runtime_error("Invalid mine-mode encountered.");
            }
        }

        // Normal Hashing or Encryption/Decryption
        std::string outpath = result["output-file"].as<std::string>();

        if (mode == Mode::Digest) {
            // Just a normal digest
            if (outpath == "/dev/stdout") {
                hashAnything(Mode::Digest, algot, inpath, std::cout, hash_size, use_test_vectors, seed, output_length);
            }
            else {
                std::ofstream outfile(outpath, std::ios::binary);
                if (!outfile.is_open()) {
                    std::cerr << "Failed to open output file: " << outpath << std::endl;
                    return 1;
                }
                hashAnything(Mode::Digest, algot, inpath, outfile, hash_size, use_test_vectors, seed, output_length);
            }
        }
        else if (mode == Mode::Stream) {
            if (outpath == "/dev/stdout") {
                hashAnything(Mode::Stream, algot, inpath, std::cout, hash_size, use_test_vectors, seed, output_length);
            }
            else {
                std::ofstream outfile(outpath, std::ios::binary);
                if (!outfile.is_open()) {
                    std::cerr << "Failed to open output file: " << outpath << std::endl;
                    return 1;
                }
                hashAnything(Mode::Stream, algot, inpath, outfile, hash_size, use_test_vectors, seed, output_length);
            }
        }
        else if (mode == Mode::Enc) {
            if (inpath.empty()) {
                throw std::runtime_error("No input file specified for encryption.");
            }
            std::string key = promptForKey("Enter encryption key: ");

            // We'll write ciphertext to inpath + ".rc"
            std::string encFile = inpath + ".rc";
            puzzleEncryptFileWithHeader(inpath, encFile, key, algot, hash_size, seed, blockSize, nonceSize, searchMode);
            std::cout << "[Enc] Wrote encrypted file to: " << encFile << "\n";
        }
        else if (mode == Mode::Dec) {
            // Puzzle-based decryption (block-based)
            if (inpath.empty()) {
                throw std::runtime_error("No ciphertext file specified for decryption.");
            }
            std::string key = promptForKey("Enter decryption key: ");

            // We'll write plaintext to inpath + ".dec"
            std::string decFile = inpath + ".dec";
            puzzleDecryptFileWithHeader(inpath, decFile, key, seed);
            std::cout << "[Dec] Wrote decrypted file to: " << decFile << "\n";
        }

        return 0;
    }
    catch (const cxxopts::exceptions::exception& e) {
      std::cerr << "Error parsing options: " << e.what() << "\n";
      return 1;
    }
    catch (const std::bad_cast &e) {
        std::cerr << "Bad cast during option parsing: " << e.what() << "\n";
        return 1;
    }
    catch (const std::runtime_error &e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
}



