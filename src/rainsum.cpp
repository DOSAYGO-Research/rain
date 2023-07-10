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
void performHash(const std::string& algorithm, const std::string& inpath, const std::string& outpath, uint32_t size, bool use_test_vectors, uint64_t seed, int output_length);
std::string generate_filename(const std::string& filename);
std::vector<uint8_t> generate_hash(const std::string& algorithm, const std::vector<char>& buffer, uint64_t seed, int output_length);

int main(int argc, char** argv) {
    cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash.");

    options.add_options()
        ("a,algorithm", "Specify the hash algorithm to use", cxxopts::value<std::string>()->default_value("storm"))
        ("s,size", "Specify the size of the hash", cxxopts::value<uint32_t>()->default_value("256"))
        ("o,outfile", "Output file for the hash", cxxopts::value<std::string>()->default_value("output.txt"))
        ("t,test-vectors", "Calculate the hash of the standard test vectors", cxxopts::value<bool>()->default_value("false"))
        ("l,output-length", "Output length in bytes", cxxopts::value<int>()->default_value("32"))
        ("seed", "Seed value", cxxopts::value<uint64_t>()->default_value("0"))
        ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        usage();
        return 0;
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
    uint64_t seed = result["seed"].as<uint64_t>();
    int output_length = result["output-length"].as<int>();

    performHash(algorithm, inpath, outpath, size, use_test_vectors, seed, output_length);

    return 0;
}

void usage() {
    std::cout << "Usage: rainsum [OPTIONS] [FILE]\n"
              << "Calculate a Rainbow or Rainstorm hash.\n\n"
              << "Options:\n"
              << "  -a, --algorithm [bow|storm]   Specify the hash algorithm to use\n"
              << "  -s, --size [64-256|64-512]    Specify the size of the hash\n"
              << "  -o, --outfile FILE            Output file for the hash\n"
              << "  -t, --test-vectors            Calculate the hash of the standard test vectors\n"
              << "  -l, --output-length BYTES     Output length in bytes\n"
              << "  --seed                        Seed value\n";
}

void performHash(const std::string& algorithm, const std::string& inpath, const std::string& outpath, uint32_t size, bool use_test_vectors, uint64_t seed, int output_length) {
    std::vector<char> buffer;

    if (use_test_vectors) {
        for (const auto& test_vector : test_vectors) {
            buffer.assign(test_vector.begin(), test_vector.end());
            auto out = generate_hash(algorithm, buffer, seed, output_length);

            for (const auto& byte : out) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            }
            std::cout << ' ' << '"' << test_vector << '"' << '\n';
        }
    } else {
        std::ifstream infile(inpath, std::ios::binary);
        buffer = std::vector<char>((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        infile.close();

        auto out = generate_hash(algorithm, buffer, seed, output_length);

        for (const auto& byte : out) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        std::cout << ' ' << inpath << '\n';
    }
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

std::vector<uint8_t> generate_hash(const std::string& algorithm, const std::vector<char>& buffer, uint64_t seed, int output_length) {
    std::vector<uint8_t> out;
    int hash_size = (algorithm == "bow") ? 256 / 8 : 512 / 8;
    std::vector<uint8_t> temp_out(hash_size);

    while (out.size() < output_length) {
        if(algorithm == "bow") {
            rainbow::rainbow<256, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
        } else if(algorithm == "storm") {
            rainstorm::rainstorm<512, bswap>(buffer.data(), buffer.size(), seed, temp_out.data());
        }
        out.insert(out.end(), temp_out.begin(), temp_out.end());
    }

    // Resize to the desired output length
    out.resize(output_length);

    return out;
}

