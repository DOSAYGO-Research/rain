#include <fstream>
#include <sys/stat.h>

#include "rainbow.tpp"
#include "rainstorm.tpp"
#include "cxxopts.hpp"
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

// Prototype of functions
void usage();
void performHash(Mode mode, const std::string& algorithm, const std::string& inpath, const std::string& outpath, uint32_t size, bool use_test_vectors, uint64_t seed, uint64_t output_length);
std::string generate_filename(const std::string& filename);
void generate_hash(Mode mode, const std::string& algorithm, std::vector<uint8_t>& buffer, uint64_t seed, uint64_t output_length, std::ostream& outstream, uint32_t hash_size);
uint64_t hash_string_to_64_bit(const std::string& seed_str);

uint64_t getFileSize(const std::string& filename) {
    struct stat st;
    if(stat(filename.c_str(), &st) != 0) {
        return 0; // You may want to handle this error differently
    }
    return st.st_size;
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

uint64_t hash_string_to_64_bit(const std::string& seed_str) {
    std::vector<char> buffer(seed_str.begin(), seed_str.end());
    std::vector<uint8_t> hash_output(8);  // 64 bits = 8 bytes
    rainstorm::rainstorm<64, bswap>(buffer.data(), buffer.size(), 0, hash_output.data());  // assuming bswap is defined elsewhere
    uint64_t seed = 0;
    std::memcpy(&seed, hash_output.data(), 8);
    return seed;
}

void usage() {
  std::cout << "Usage: rainsum [OPTIONS] [INFILE]\n"
            << "Calculate a Rainbow or Rainstorm hash.\n\n"
            << "Options:\n"
            << "  -m, --mode [digest|stream]        Specifies the mode, where:\n"
            << "                                    digest mode (the default) gives a fixed length hash in hex, or\n"
            << "                                    stream mode gives a variable length binary feedback output\n"
            << "  -a, --algorithm [bow|storm]       Specify the hash algorithm to use. Default: storm\n"
            << "  -s, --size [64-256|64-512]        Specify the bit size of the hash. Default: 256\n"
            << "  -o, --output-file FILE            Output file for the hash or stream\n"
            << "  -t, --test-vectors                Calculate the hash of the standard test vectors\n"
            << "  -l, --output-length HASHES        Set the output length in hash iterations (stream only)\n"
            << "  -v, --version                     Print out the version\n"
            << "  --seed                            Seed value (64-bit number or string). If string is used,\n"
            << "                                    it is hashed with Rainstorm to a 64-bit number\n";
}

// test vectors
std::vector<std::string> test_vectors = {"", 
                     "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 
                     "The quick brown fox jumps over the lazy dog", 
                     "The quick brown fox jumps over the lazy cog", 
                     "The quick brown fox jumps over the lazy dog.", 
                     "After the rainstorm comes the rainbow.", 
                     std::string(64, '@')};

