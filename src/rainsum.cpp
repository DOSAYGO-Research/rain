#define VERSION "1.0.3"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <ctime>
#include <filesystem>
#include "tool.h"

int main(int argc, char** argv) {
  cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash.");
  auto seed_option = cxxopts::value<std::string>()->default_value("0");

  options.add_options()
    ("m,mode", "Mode: digest or stream", cxxopts::value<Mode>()->default_value("digest"))
    ("v,version", "Print version")
    ("a,algorithm", "Specify the hash algorithm to use", cxxopts::value<std::string>()->default_value("storm"))
    ("s,size", "Specify the size of the hash", cxxopts::value<uint32_t>()->default_value("256"))
    ("o,output-file", "Output file for the hash", cxxopts::value<std::string>()->default_value("/dev/stdout"))
    ("t,test-vectors", "Calculate the hash of the standard test vectors", cxxopts::value<bool>()->default_value("false"))
    ("l,output-length", "Output length in hashes", cxxopts::value<uint64_t>()->default_value("1000000"))
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
  }

  std::string algorithm = result["algorithm"].as<std::string>();
  uint32_t size = result["size"].as<uint32_t>();
  std::string outpath = result["output-file"].as<std::string>();
  bool use_test_vectors = result["test-vectors"].as<bool>();
  uint64_t output_length = result["output-length"].as<uint64_t>();

  if ( mode == Mode::Digest ) {
    output_length = size / 8;
  } else {
    output_length *= size;
  }

  if (mode == Mode::Digest) {
      performHash(Mode::Digest, algorithm, inpath, outpath, size, use_test_vectors, seed, size / 8);
  } else {
      performHash(Mode::Stream, algorithm, inpath, outpath, size, use_test_vectors, seed, output_length);
  }

  return 0;
}

template<bool bswap>
void invokeHash(const std::string& algorithm, uint64_t seed, std::vector<uint8_t>& buffer, std::vector<uint8_t>& temp_out, int hash_size) {
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

void performHash(Mode mode, const std::string& algorithm, const std::string& inpath, const std::string& outpath, uint32_t size, bool use_test_vectors, uint64_t seed, uint64_t output_length) {
    std::vector<uint8_t> buffer;
    std::ofstream outfile(outpath, std::ios::binary);
    std::vector<uint8_t> chunk(CHUNK_SIZE);

    if (use_test_vectors) {
        for (const auto& test_vector : test_vectors) {
            buffer.assign(test_vector.begin(), test_vector.end());
            generate_hash(mode, algorithm, buffer, seed, output_length, outfile, size);
            outfile << ' ' << '"' << test_vector << '"' << '\n';
        }
    } else {
        std::istream* in_stream = nullptr;
        std::ifstream infile;

        uint64_t input_length = 0;

        if (!inpath.empty()) {
            infile.open(inpath, std::ios::binary);
            if (infile.fail()) {
                throw std::runtime_error("Cannot open file");
            }
            in_stream = &infile;
            input_length = getFileSize(inpath);

            // Initialize the Rainbow hash state with the input length
            rainbow::HashState state = rainbow::initialize(seed, input_length);

            // Stream the file in 8192-byte chunks
            while (*in_stream) {
                in_stream->read(reinterpret_cast<char*>(chunk.data()), CHUNK_SIZE);
                std::streamsize bytes_read = in_stream->gcount();
                if (bytes_read > 0) {
                    // Update the state with the new chunk of data
                    rainbow::update(state, chunk.data(), bytes_read);
                }
            }

            // Close the file if it's open
            if (infile.is_open()) {
                infile.close();
            }

            // Finalize the hash
            std::vector<uint8_t> output(output_length);
            rainbow::finalize(state, size, output.data());

            std::cout << "Length : " << state.len << std::endl;
            std::cout << "File length : " << input_length << std::endl;


            // Write the output to the outfile

            if (mode == Mode::Digest) {
              for (const auto& byte : output) {
                outfile << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
              }
              outfile << ' ' << (inpath.empty() ? "stdin" : inpath) << '\n';
            } else {
              outfile.write(reinterpret_cast<char*>(output.data()), output_length);
            }
        } else {
          in_stream = &getInputStream();
          // Read all data into the buffer
          buffer = std::vector<uint8_t>(std::istreambuf_iterator<char>(*in_stream), {});
          input_length = buffer.size();
          std::cout << "File length : " << input_length << std::endl;

          // Instead of using the input as a string, convert it to a uint64_t seed
          std::string buffer_string(buffer.begin(), buffer.end());
          seed = hash_string_to_64_bit(buffer_string);

          generate_hash(mode, algorithm, buffer, seed, output_length, outfile, size);
          outfile << ' ' << (inpath.empty() ? "stdin" : inpath) << '\n';
        }
    }

    outfile.close();
}

void generate_hash(Mode mode, const std::string& algorithm, std::vector<uint8_t>& buffer, uint64_t seed, uint64_t output_length, std::ostream& outstream, uint32_t hash_size) {
  std::cout << "hash size: " << hash_size << std::endl;
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

      uint64_t chunk_size = std::min(output_length, (uint64_t)byte_size);
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

