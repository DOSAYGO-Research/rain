#define VERSION "1.0.1"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <ctime>
#include <filesystem>
#include "rainbow.tpp"
#include "rainstorm.tpp"
#include "cxxopts.hpp"

// Adjust this path based on the endian.h file location
#include "endian.h"

enum class Mode {
  Digest,
  Stream
};

std::istream& operator>>(std::istream& in, Mode& mode) {
  std::string token;
  in >> token;
  if (token == "digest")
    mode = Mode::Digest;
  else if (token == "stream")
    mode = Mode::Stream;
  else
    in.setstate(std::ios_base::failbit);
  return in;
}

// test vectors
std::vector<std::string> test_vectors = {"", 
                     "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 
                     "The quick brown fox jumps over the lazy dog", 
                     "The quick brown fox jumps over the lazy cog", 
                     "The quick brown fox jumps over the lazy dog.", 
                     "After the rainstorm comes the rainbow.", 
                     std::string(64, '@')};

// Prototype of functions
void usage();
void performHash(Mode mode, const std::string& algorithm, const std::string& inpath, const std::string& outpath, uint32_t size, bool use_test_vectors, uint64_t seed, int output_length);
std::string generate_filename(const std::string& filename);
void generate_hash(Mode mode, const std::string& algorithm, std::vector<char>& buffer, uint64_t seed, int output_length, std::ostream& outstream, uint32_t hash_size);
uint64_t hash_string_to_64_bit(const std::string& seed_str);

int main(int argc, char** argv) {
  cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash.");
  auto seed_option = cxxopts::value<std::string>()->default_value("0");

  options.add_options()
    ("m,mode", "Mode: digest or stream", cxxopts::value<Mode>()->default_value("digest"))
    ("v,version", "Print version")
    ("a,algorithm", "Specify the hash algorithm to use", cxxopts::value<std::string>()->default_value("storm"))
    ("s,size", "Specify the size of the hash", cxxopts::value<uint32_t>()->default_value("256"))
    ("o,outfile", "Output file for the hash", cxxopts::value<std::string>()->default_value("/dev/stdout"))
    ("t,test-vectors", "Calculate the hash of the standard test vectors", cxxopts::value<bool>()->default_value("false"))
    ("l,output-length", "Output length in hashes", cxxopts::value<int>()->default_value("1000000"))
    ("seed", "Seed value", seed_option)
    ("h,help", "Print usage");

  auto result = options.parse(argc, argv);

  if (result.count("version")) {
    std::cout << "rainsum version: " << VERSION << '\n';
  }

  if (result.count("help")) {
    usage();
    return 0;
  }

  std::string seed_str = result["seed"].as<std::string>();
  uint64_t seed;
  // If the seed_str can be converted to uint64_t, use it as a number; otherwise, hash it (with rainstorm, seed 0, 64-bit)
  if (!(std::istringstream(seed_str) >> seed)) {
      seed = hash_string_to_64_bit(seed_str);
  }
  //std::cout << "Seed : " << seed << std::endl;

  Mode mode = result["mode"].as<Mode>();

  // Check for output-length in Digest mode after parsing all options
  if (mode == Mode::Digest && result.count("output-length")) {
    std::cerr << "Error: --output-length is not allowed in digest mode.\n";
    return 1;
  }

  std::string inpath;

  // Check if unmatched arguments exist (indicating there was a filename provided)
  if(!result.unmatched().empty()) {
    inpath = result.unmatched().front();
  } else {
    // No filename provided, read from stdin
    inpath = "/dev/stdin";
  }

  std::string algorithm = result["algorithm"].as<std::string>();
  uint32_t size = result["size"].as<uint32_t>();
  std::string outpath = result["outfile"].as<std::string>();
  bool use_test_vectors = result["test-vectors"].as<bool>();
  int output_length = result["output-length"].as<int>();

  if ( mode == Mode::Digest ) {
    output_length = size / 8;
  } else {
    output_length *= size;
  }

  performHash(mode, algorithm, inpath, outpath, size, use_test_vectors, seed, output_length);

  return 0;
}

void usage() {
  std::cout << "Usage: rainsum [OPTIONS] [INFILE]\n"
            << "Calculate a Rainbow or Rainstorm hash.\n\n"
            << "Options:\n"
            << "  -m, --mode [digest|stream]    Specifies the mode, where:\n"
            << "                                  digest mode (the default) gives a fixed length hash in hex, or\n"
            << "                                  stream mode gives a variable length binary feedback output\n"
            << "  -a, --algorithm [bow|storm]   Specify the hash algorithm to use. Default: storm\n"
            << "  -s, --size [64-256|64-512]    Specify the bit size of the hash. Default: 256\n"
            << "  -o, --outfile FILE            Output file for the hash or stream\n"
            << "  -t, --test-vectors            Calculate the hash of the standard test vectors\n"
            << "  -l, --output-length BYTES     Set the output length in hash iterations (stream only)\n"
            << "  -v, --version                 Print out the version\n"
            << "  --seed                        Seed value (64-bit number or string)\n";
}

template<bool bswap>
void invokeHash(const std::string& algorithm, uint64_t seed, std::vector<char>& buffer, std::vector<uint8_t>& temp_out, int hash_size) {
  if(algorithm == "rainbow" || algorithm == "bow") {
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
  } else if(algorithm == "rainstorm" || algorithm == "storm") {
    switch(hash_size) {
      case 64:
        rainstorm::rainstorm<64, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
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
    throw std::runtime_error("Invalid algorithm");
  }
}

uint64_t hash_string_to_64_bit(const std::string& seed_str) {
    std::vector<char> buffer(seed_str.begin(), seed_str.end());
    std::vector<uint8_t> hash_output(8);  // 64 bits = 8 bytes
    rainstorm::rainstorm<64, bswap>(buffer.data(), buffer.size(), 0, hash_output.data());  // assuming bswap is defined elsewhere
    uint64_t seed = 0;
    std::memcpy(&seed, hash_output.data(), 8);
    return seed;
}

void performHash(Mode mode, const std::string& algorithm, const std::string& inpath, const std::string& outpath, uint32_t size, bool use_test_vectors, uint64_t seed, int output_length) {
  std::vector<char> buffer;
  std::ofstream outfile(outpath, std::ios::binary);

  if (use_test_vectors) {
    for (const auto& test_vector : test_vectors) {
      buffer.assign(test_vector.begin(), test_vector.end());
      generate_hash(mode, algorithm, buffer, seed, output_length, outfile, size);
      outfile << ' ' << '"' << test_vector << '"' << '\n';
    }
  } else {
    std::ifstream infile(inpath, std::ios::binary);
    buffer = std::vector<char>((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();
    
    generate_hash(mode, algorithm, buffer, seed, output_length, outfile, size);
    if ( mode == Mode::Digest ) {
      outfile << ' ' << inpath << '\n';
    }
  }
  
  outfile.close();
}

std::string generate_filename(const std::string& filename) {
  std::filesystem::path p{filename};
  std::string new_filename;
  std::string timestamp = "-" + std::to_string(std::time(nullptr));

  if (std::filesystem::exists(p)) {
    new_filename = p.stem().string() + timestamp + p.extension().string();
    
    // Handle filename collision
    int counter = 1;
    while (std::filesystem::exists(new_filename)) {
      new_filename = p.stem().string() + timestamp + "-" + std::to_string(counter++) + p.extension().string();
    }
  } else {
    new_filename = filename;
  }

  return new_filename;
}

void generate_hash(Mode mode, const std::string& algorithm, std::vector<char>& buffer, uint64_t seed, int output_length, std::ostream& outstream, uint32_t hash_size) {
  int byte_size = hash_size / 8;
  std::vector<uint8_t> temp_out(byte_size);
  
  if(mode == Mode::Digest) {
    invokeHash<bswap>(algorithm, seed, buffer, temp_out, hash_size);
    
    for (const auto& byte : temp_out) {
      outstream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
  }
  else if(mode == Mode::Stream) {
    while(output_length > 0) {
      invokeHash<bswap>(algorithm, seed, buffer, temp_out, hash_size);

      int chunk_size = std::min(output_length, byte_size);
      outstream.write(reinterpret_cast<const char*>(temp_out.data()), chunk_size);

      output_length -= chunk_size;
      if(output_length == 0) {
        break;
      }

      // Update buffer with the output of the last hash operation
      buffer.assign(temp_out.begin(), temp_out.begin() + chunk_size);
    }
  }
}
