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


#include "tool.h"

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

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <iterator>
#include <cstdint>
#include <stdexcept>
#include <chrono>
#include "tool.h" // for invokeHash, etc.

static void puzzleEncryptFile(
  const std::string &inFilename,
  const std::string &outFilename,
  const std::string &key,
  HashAlgorithm algot,
  uint32_t hash_size,
  uint64_t seed,
  size_t blockSize
) {
  // 1) Read input file
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("Cannot open input file: " + inFilename);
  }
  std::vector<uint8_t> plainData((std::istreambuf_iterator<char>(fin)),
                                 (std::istreambuf_iterator<char>()));
  fin.close();

  // 2) Prepare output
  std::ofstream fout(outFilename, std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("Cannot open output file: " + outFilename);
  }

  // Write 8-byte original size
  uint64_t originalSize = plainData.size();
  fout.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));

  // 3) Partition into blocks
  size_t totalBlocks = (plainData.size() + blockSize - 1) / blockSize;
  std::vector<uint8_t> hashOut(hash_size / 8);
  std::vector<uint8_t> keyBuf(key.begin(), key.end());

  // Use a random 64-bit generator
  std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<uint64_t> dist; // full 64-bit range

  // Increase MAX_TRIES significantly for 3+ byte blocks
  const uint64_t MAX_TRIES = 10'000'000'000; 
  size_t offset = 0;

  auto startTime = std::chrono::steady_clock::now();

  for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
    size_t thisBlockSize = std::min(blockSize, plainData.size() - offset);
    std::vector<uint8_t> block(
      plainData.begin() + offset,
      plainData.begin() + offset + thisBlockSize
    );
    offset += thisBlockSize;

    bool found = false;
    uint64_t chosenNonce = 0;

    for (uint64_t tries = 1; tries <= MAX_TRIES; tries++) {
      uint64_t candidateNonce = dist(rng);

      // Build trial = key || candidateNonce
      std::vector<uint8_t> trial(keyBuf);
      trial.reserve(keyBuf.size() + 8);  // small optimization
      for (int i = 0; i < 8; i++) {
        trial.push_back(static_cast<uint8_t>((candidateNonce >> (8 * i)) & 0xFF));
      }

      // Hash it
      invokeHash<bswap>(algot, seed, trial, hashOut, hash_size);

      // Check prefix
      if (thisBlockSize <= hashOut.size() &&
          std::equal(block.begin(), block.end(), hashOut.begin())) {
        chosenNonce = candidateNonce;
        found = true;
        break;
      }

      // Optional: show progress every so often
      if (tries % 1'000'000 == 0) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - startTime).count();
        double hps = tries / elapsed;
        std::cerr << "\r[EncRand] Block " << blockIndex << ", "
                  << tries << " tries, ~" << static_cast<int>(hps)
                  << " H/s " << std::flush;
      }
    }

    if (!found) {
      std::cerr << "\n[EncRand] WARNING: puzzle not solved for block " << blockIndex
                << " within " << MAX_TRIES << " tries.\n";
    }

    // Write the chosen nonce (8 bytes)
    for (int i = 0; i < 8; i++) {
      uint8_t c = (chosenNonce >> (8 * i)) & 0xFF;
      fout.write(reinterpret_cast<char*>(&c), 1);
    }
    std::cerr << "\n[EncRand] Block " << blockIndex << " done.\n";
  }

  fout.close();
  std::cout << "[EncRand] Encryption finished: " << inFilename
            << " => " << outFilename << "\n";
}


// puzzleDecryptFile: reads [8-byte originalSize][(N) x 8-byte nonces],
// recovers each block via hash(key||nonce).
static void puzzleDecryptFile(
  const std::string &inFilename,
  const std::string &outFilename,
  const std::string &key,
  HashAlgorithm algot,
  uint32_t hash_size,
  uint64_t seed,
  size_t blockSize
) {
  // 1) Read entire ciphertext
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("Cannot open ciphertext for decryption: " + inFilename);
  }
  // First 8 bytes = original size
  uint64_t originalSize = 0;
  fin.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));

  // The rest are 8-byte nonces
  std::vector<uint8_t> cipherData(
    (std::istreambuf_iterator<char>(fin)),
    (std::istreambuf_iterator<char>())
  );
  fin.close();

  if (cipherData.size() % 8 != 0) {
    throw std::runtime_error("[Dec] Cipher data not multiple of 8 bytes after header!");
  }
  size_t totalBlocks = cipherData.size() / 8;

  // 2) Prepare output file
  std::ofstream fout(outFilename, std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("Cannot open output file for plaintext: " + outFilename);
  }

  // 3) Reconstruct each block
  std::vector<uint8_t> keyBuf(key.begin(), key.end());
  std::vector<uint8_t> hashOut(hash_size / 8);

  size_t recoveredSoFar = 0;

  for (size_t i = 0; i < totalBlocks; i++) {
    // Read the 8-byte nonce
    uint64_t storedNonce = 0;
    for (int j = 0; j < 8; j++) {
      storedNonce |= (static_cast<uint64_t>(cipherData[i*8 + j]) << (8 * j));
    }

    // Recompute the hash
    std::vector<uint8_t> trial(keyBuf);
    for (int j = 0; j < 8; j++) {
      trial.push_back((storedNonce >> (8 * j)) & 0xFF);
    }
    invokeHash<bswap>(algot, seed, trial, hashOut, hash_size);

    // Figure out how many bytes remain vs. blockSize
    size_t remaining = originalSize - recoveredSoFar;
    size_t thisBlockSize = std::min(blockSize, remaining);
    if (thisBlockSize > 0) {
      if (thisBlockSize <= hashOut.size()) {
        // Write that many bytes from hashOut
        fout.write(reinterpret_cast<const char*>(hashOut.data()), thisBlockSize);
        recoveredSoFar += thisBlockSize;
      } else {
        std::cerr << "[Dec] Unexpected error: hashOut smaller than blockSize.\n";
        break;
      }
    }
    // If recoveredSoFar == originalSize, we can break early if we want.
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
    cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash, or do puzzle-based encryption.");
    auto seed_option = cxxopts::value<std::string>()->default_value("0");

    // Expand your mode description to include enc & dec
    options.add_options()
      ("m,mode", "Mode: digest, stream, enc, dec",
        cxxopts::value<Mode>()->default_value("digest"))
      ("v,version", "Print version")
      ("a,algorithm", "Specify the hash algorithm to use",
        cxxopts::value<std::string>()->default_value("bow"))
      ("s,size", "Specify the size of the hash",
        cxxopts::value<uint32_t>()->default_value("256"))
      ("o,output-file", "Output file", cxxopts::value<std::string>()->default_value("/dev/stdout"))
      ("t,test-vectors", "Calculate the hash of the standard test vectors",
        cxxopts::value<bool>()->default_value("false"))
      ("l,output-length", "Output length in hash iterations (stream mode)",
        cxxopts::value<uint64_t>()->default_value("1000000"))
      ("seed", "Seed value", seed_option)
      // Mining
      ("mine-mode", "Mining mode: chain, nonceInc, nonceRand",
        cxxopts::value<MineMode>()->default_value("None"))
      ("match-prefix", "Hex prefix to match in mining tasks",
        cxxopts::value<std::string>()->default_value(""))

      // Encrypt / Decrypt
       ("p,puzzle-len",
   "Number of bytes in the puzzle prefix [default: 3]",
   cxxopts::value<size_t>()->default_value("3"))

    // New block-size for puzzle-based enc/dec
      ("block-size", "Block size in bytes for puzzle-based encryption",
        cxxopts::value<size_t>()->default_value("3"))
      ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    size_t puzzleLen = result["puzzle-len"].as<size_t>();

    if (result.count("version")) {
      std::cout << "rainsum version: " << VERSION << '\n';
    }

    if (result.count("help")) {
      usage();
      return 0;
    }

    // Convert seed (either numeric or hash string)
    std::string seed_str = result["seed"].as<std::string>();
    uint64_t seed;
    if (!(std::istringstream(seed_str) >> seed)) {
      seed = hash_string_to_64_bit(seed_str);
    }

    Mode mode = result["mode"].as<Mode>();

    std::string algorithm = result["algorithm"].as<std::string>();
    HashAlgorithm algot = getHashAlgorithm(algorithm);
    if (algot == HashAlgorithm::Unknown) {
      throw std::runtime_error("Unsupported algorithm string: " + algorithm);
    }

    uint32_t size = result["size"].as<uint32_t>();
    // Validate sizes (similar checks from your code)
    if (algot == HashAlgorithm::Rainbow) {
      if (size != 64 && size != 128 && size != 256) {
        throw std::runtime_error("Invalid size for Rainbow (must be 64,128,256).");
      }
    } else if (algot == HashAlgorithm::Rainstorm) {
      if (size != 64 && size != 128 && size != 256 && size != 512) {
        throw std::runtime_error("Invalid size for Rainstorm (must be 64,128,256,512).");
      }
    }

    bool use_test_vectors = result["test-vectors"].as<bool>();
    uint64_t output_length = result["output-length"].as<uint64_t>();

    // In digest mode, final output is size/8
    // In stream mode, you read 'output_length' times the hash
    if (mode == Mode::Digest) {
      output_length = size / 8;
    } else if (mode == Mode::Stream) {
      output_length *= size;
    }

    // Mining arguments
    MineMode mine_mode = result["mine-mode"].as<MineMode>();
    std::string prefixHex = result["match-prefix"].as<std::string>();

    // Unmatched arguments might be input file
    std::string inpath;
    if (!result.unmatched().empty()) {
      inpath = result.unmatched().front();
    }

    // If user selected a mining mode
    if (mine_mode != MineMode::None) {
      if (prefixHex.empty()) {
        throw std::runtime_error("You must specify --match-prefix for mining modes.");
      }
      // Convert prefix from hex
      auto hexToBytes = [&](const std::string &hexstr) {
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
          mineChain(algot, seed, size, prefixBytes);
          return 0;
        case MineMode::NonceInc:
          mineNonceInc(algot, seed, size, prefixBytes, inpath);
          return 0;
        case MineMode::NonceRand:
          mineNonceRand(algot, seed, size, prefixBytes, inpath);
          return 0;
        default:
          throw std::runtime_error("Invalid mine-mode encountered.");
      }
    }

    size_t blockSize = result["block-size"].as<size_t>();

    // Normal hashing or encryption/decryption
    std::string outpath = result["output-file"].as<std::string>();

    if (mode == Mode::Digest) {
      // Just a normal digest
      if (outpath == "/dev/stdout") {
        hashAnything(Mode::Digest, algot, inpath, std::cout, size, use_test_vectors, seed, output_length);
      } else {
        std::ofstream outfile(outpath, std::ios::binary);
        if (!outfile.is_open()) {
          std::cerr << "Failed to open output file: " << outpath << std::endl;
          return 1;
        }
        hashAnything(Mode::Digest, algot, inpath, outfile, size, use_test_vectors, seed, output_length);
      }
    }
    else if (mode == Mode::Stream) {
      if (outpath == "/dev/stdout") {
        hashAnything(Mode::Stream, algot, inpath, std::cout, size, use_test_vectors, seed, output_length);
      } else {
        std::ofstream outfile(outpath, std::ios::binary);
        if (!outfile.is_open()) {
          std::cerr << "Failed to open output file: " << outpath << std::endl;
          return 1;
        }
        hashAnything(Mode::Stream, algot, inpath, outfile, size, use_test_vectors, seed, output_length);
      }
    }
    else if (mode == Mode::Enc) {
      if (inpath.empty()) {
        throw std::runtime_error("No input file specified for encryption.");
      }
      std::string key = promptForKey("Enter encryption key: ");

      // We'll write ciphertext to inpath + ".rc"
      std::string encFile = inpath + ".rc";
      puzzleEncryptFile(inpath, encFile, key, algot, size, seed, blockSize);
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
      puzzleDecryptFile(inpath, decFile, key, algot, size, seed, blockSize);
      std::cout << "[Dec] Wrote decrypted file to: " << decFile << "\n";
    }

    return 0;
  }
  catch (const std::runtime_error &e) {
    std::cerr << "An error occurred: " << e.what() << std::endl;
    return 1;
  }
}


