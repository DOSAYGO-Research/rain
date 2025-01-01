#ifdef _OPENMP
#include <omp.h>
#endif
#include "tool.h" // for invokeHash, etc.

/* note the new Enum is 
  enum class Mode {
    Digest,
    Stream,
    BlockEnc,
    StreamEnc,
    Dec,
    Info
  };
*/

// NEW: Just read the file header and display metadata
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

// The main function
int main(int argc, char** argv) {
    try {
        // Initialize cxxopts with program name and description
        cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash, or perform puzzle-based block encryption/decryption, or stream encryption/decryption.");

        // Define command-line options
        options.add_options()
            ("m,mode", "Mode: digest, stream, block-enc, stream-enc, dec, info",
                cxxopts::value<std::string>()->default_value("digest"))
            ("a,algorithm", "Specify the hash algorithm to use (rainbow, rainstorm, rainbow-rp, rainstorm-nis1)",
                cxxopts::value<std::string>()->default_value("storm"))
            ("s,size", "Specify the size of the hash (e.g., 64, 128, 256, 512)",
                cxxopts::value<uint32_t>()->default_value("256"))
            ("block-size", "Block size in bytes for puzzle-based encryption (1-255)",
                cxxopts::value<uint8_t>()->default_value("3"))
            ("n,nonce-size", "Size of the nonce in bytes [1-255] (block-enc mode)",
                cxxopts::value<size_t>()->default_value("14"))
            ("deterministic-nonce", "Use a deterministic counter for nonce generation",
                cxxopts::value<bool>()->default_value("false"))
            ("search-mode", "Search mode for plaintext mining block cipher: prefix, sequence, series, scatter, mapscatter, parascatter",
                cxxopts::value<std::string>()->default_value("scatter"))
            ("o,output-file", "Output file (stream mode)",
                cxxopts::value<std::string>()->default_value("/dev/stdout"))
            ("t,test-vectors", "Calculate the hash of the standard test vectors",
                cxxopts::value<bool>()->default_value("false"))
            ("l,output-length", "Output length in hash iterations (stream mode)",
                cxxopts::value<uint64_t>()->default_value("1000000"))
            ("x,output-extension", "Output extension in bytes (block-enc mode). Extend digest by this many bytes to make mining larger P blocks faster",
                cxxopts::value<uint32_t>()->default_value("128"))
            ("seed", "Seed value",
                cxxopts::value<std::string>()->default_value("0"))
            // Mining options
            ("mine-mode", "Mining demo mode: chain, nonceInc, nonceRand (mining demo only)",
                cxxopts::value<std::string>()->default_value("None"))
            ("match-prefix", "Hex prefix to match in mining tasks (mining demo only)",
                cxxopts::value<std::string>()->default_value(""))
            // Encrypt / Decrypt options
            ("P,password", "Encryption/Decryption password (raw, insecure)",
                cxxopts::value<std::string>()->default_value(""))
            // NEW: verbose
            ("vv,verbose", "Enable verbose output for series/scatter indices",
                cxxopts::value<bool>()->default_value("false"))
            ("v,version", "Print version")
            ("h,help", "Print usage");

        // Parse command-line options
        auto result = options.parse(argc, argv);

        // Handle help and version immediately
        if (result.count("help")) {
            std::cout << options.help() << "\n";
            return 0;
        }

        if (result.count("version")) {
            std::cout << "rainsum version: " << VERSION << "\n"; // Replace with actual VERSION
            return 0;
        }

        // Access and validate options

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

        // Nonce nature
        bool deterministicNonce = result["deterministic-nonce"].as<bool>();

        // Search Mode
        std::string searchMode = result["search-mode"].as<std::string>();
        if (searchMode != "prefix" && searchMode != "sequence" &&
            searchMode != "series" && searchMode != "scatter" && searchMode != "mapscatter" && searchMode != "parascatter") {
            throw std::runtime_error("Invalid search mode: " + searchMode);
        }

        // Output extension
        uint32_t output_extension = result["output-extension"]. as<uint32_t>();

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
        else if (modeStr == "block-enc") mode = Mode::BlockEnc;
        else if (modeStr == "stream-enc") mode = Mode::StreamEnc;
        else if (modeStr == "dec") mode = Mode::Dec;
        else if (modeStr == "info") mode = Mode::Info; // NEW
        else throw std::runtime_error("Invalid mode: " + modeStr);

        // Determine Hash Algorithm
        std::string algorithm = result["algorithm"].as<std::string>();
        HashAlgorithm algot = getHashAlgorithm(algorithm);
        if (algot == HashAlgorithm::Unknown) {
            throw std::runtime_error("Unsupported algorithm string: " + algorithm);
        }

        // Validate Hash Size based on Algorithm
        if (algot == HashAlgorithm::Rainbow) {
            if (hash_size == 512) {
              hash_size = 256;
            }
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

        std::string password = result["password"].as<std::string>();

        // NEW: verbose
        bool verbose = result["verbose"].as<bool>();

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

        // If mode == info, just show header info
        if (mode == Mode::Info) {
            if (inpath.empty()) {
                throw std::runtime_error("No input file specified for info mode.");
            }
            puzzleShowFileInfo(inpath);
            return 0;
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
        else if (mode == Mode::BlockEnc) {
            if (inpath.empty()) {
                throw std::runtime_error("No input file specified for encryption.");
            }
            std::string key;
            if (!password.empty()) {
                key = password;
            }
            else {
                key = promptForKey("Enter encryption key: ");
            }

            // We'll write ciphertext to inpath + ".rc"
            std::string encFile = inpath + ".rc";
            puzzleEncryptFileWithHeader(inpath, encFile, key, algot, hash_size, seed, blockSize, nonceSize, searchMode, verbose, deterministicNonce);
            std::cout << "[Enc] Wrote encrypted file to: " << encFile << "\n";
        }
        else if (mode == Mode::StreamEnc) {
            if (inpath.empty()) {
                throw std::runtime_error("No input file specified for encryption.");
            }
            std::string key;
            if (!password.empty()) {
                key = password;
            }
            else {
                key = promptForKey("Enter encryption key: ");
            }

            // We'll write ciphertext to inpath + ".rc"
            std::string encFile = inpath + ".rc";
            puzzleEncryptFileWithHeader(inpath, encFile, key, algot, hash_size, seed, blockSize, nonceSize, searchMode, verbose, deterministicNonce);
            std::cout << "[Enc] Wrote encrypted file to: " << encFile << "\n";
        }
        else if (mode == Mode::Dec) {
            // Puzzle-based decryption (block-based)
            if (inpath.empty()) {
                throw std::runtime_error("No ciphertext file specified for decryption.");
            }
            std::string key;
            if (!password.empty()) {
                key = password;
            }
            else {
                key = promptForKey("Enter encryption key: ");
            }

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

