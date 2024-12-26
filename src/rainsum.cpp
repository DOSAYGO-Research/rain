#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <ctime>
#include <chrono>
#include <random>
#include <sstream>

#include "tool.h"

// ------------------------------------------------------------------
// Helper functions for mining
// ------------------------------------------------------------------
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
// Main
// ------------------------------------------------------------------
int main(int argc, char** argv) {
  try {
    cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash.");
    auto seed_option = cxxopts::value<std::string>()->default_value("0");

    options.add_options()
      ("m,mode", "Mode: digest or stream", cxxopts::value<Mode>()->default_value("digest"))
      ("v,version", "Print version")
      ("a,algorithm", "Specify the hash algorithm to use", cxxopts::value<std::string>()->default_value("bow"))
      ("s,size", "Specify the size of the hash", cxxopts::value<uint32_t>()->default_value("256"))
      ("o,output-file", "Output file for the hash", cxxopts::value<std::string>()->default_value("/dev/stdout"))
      ("t,test-vectors", "Calculate the hash of the standard test vectors", cxxopts::value<bool>()->default_value("false"))
      ("l,output-length", "Output length in hashes (stream mode only)", cxxopts::value<uint64_t>()->default_value("1000000"))
      ("seed", "Seed value", seed_option)
      // The new mining options:
      ("mine-mode", "Mining mode: chain, nonceInc, nonceRand", cxxopts::value<MineMode>()->default_value("None"))
      ("match-prefix", "Hex prefix to match in mining tasks", cxxopts::value<std::string>()->default_value(""))
      ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

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

    // If user attempts --output-length in digest mode:
    if (mode == Mode::Digest && result.count("output-length")) {
      std::cerr << "Error: --output-length is not allowed in digest mode.\n";
      return 1;
    }

    std::string algorithm = result["algorithm"].as<std::string>();
    HashAlgorithm algot = getHashAlgorithm(algorithm);

    uint32_t size = result["size"].as<uint32_t>();

    // Validate sizes
    if (algot == HashAlgorithm::Rainbow) {
      switch (size) {
        case 64:
        case 128:
        case 256:
          break;
        default:
          throw std::runtime_error("Invalid hash_size for rainbow. Allowed: 64, 128, 256");
      }
    } else if (algot == HashAlgorithm::Rainstorm) {
      switch (size) {
        case 64:
        case 128:
        case 256:
        case 512:
          break;
        default:
          throw std::runtime_error("Invalid hash_size for rainstorm. Allowed: 64,128,256,512");
      }
    }

    bool use_test_vectors = result["test-vectors"].as<bool>();

    uint64_t output_length = result["output-length"].as<uint64_t>();
    if (mode == Mode::Digest) {
      // In digest mode, the final output is always exactly size/8
      output_length = size / 8;
    } else {
      // In stream mode, output_length is multiplied by the hash size
      output_length *= size;
    }

    // Extract mining-related args
    MineMode mine_mode = result["mine-mode"].as<MineMode>();
    std::string prefixHex = result["match-prefix"].as<std::string>();

    // Check unmatched for potential filename or base input
    std::string baseInput;
    std::string inpath;
    if (!result.unmatched().empty()) {
      baseInput = result.unmatched().front();
      inpath = baseInput; // up to you whether you treat it as file vs base input
    }

    // If user selected a mining mode:
    if (mine_mode != MineMode::None) {
      if (prefixHex.empty()) {
        throw std::runtime_error("You must specify --match-prefix for mining modes.");
      }
      std::vector<uint8_t> prefixBytes = hexToBytes(prefixHex);

      switch (mine_mode) {
        case MineMode::Chain:
          mineChain(algot, seed, size, prefixBytes);
          return 0;
        case MineMode::NonceInc:
          mineNonceInc(algot, seed, size, prefixBytes, baseInput);
          return 0;
        case MineMode::NonceRand:
          mineNonceRand(algot, seed, size, prefixBytes, baseInput);
          return 0;
        default:
          throw std::runtime_error("Unknown mining mode encountered.");
      }
    }

    // No mining: proceed with normal hashing
    std::string outpath = result["output-file"].as<std::string>();

    if (outpath == "/dev/stdout") {
      if (mode == Mode::Digest) {
        hashAnything(Mode::Digest, algot, inpath, std::cout, size, use_test_vectors, seed, size / 8);
      } else {
        hashAnything(Mode::Stream, algot, inpath, std::cout, size, use_test_vectors, seed, output_length);
      }
    } else {
      std::ofstream outfile(outpath, std::ios::binary);
      if (!outfile.is_open()) {
        std::cerr << "Failed to open output file: " << outpath << std::endl;
        return 1;
      }

      if (mode == Mode::Digest) {
        hashAnything(Mode::Digest, algot, inpath, outfile, size, use_test_vectors, seed, size / 8);
      } else {
        hashAnything(Mode::Stream, algot, inpath, outfile, size, use_test_vectors, seed, output_length);
      }
      outfile.close();
    }

    return 0;
  }
  catch (const std::runtime_error& e) {
    std::cerr << "An error occurred: " << e.what() << std::endl;
    return 1;
  }
}

