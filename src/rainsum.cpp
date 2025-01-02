#ifdef _OPENMP
#include <omp.h>
#endif
#include "tool.h" // for invokeHash, etc.
#include "file-header.h"
#include "block-cipher.h"
#include "stream-cipher.h"


// =================================================================
// ADDED: Main Function with Additions Only
// =================================================================
int main(int argc, char** argv) {
    try {
        // Initialize cxxopts with program name and description
        cxxopts::Options options("rainsum", "Calculate a Rainbow or Rainstorm hash, or perform puzzle-based block encryption/decryption, or stream encryption/decryption.");

        // Define command-line options (unchanged)
        options.add_options()
            ("m,mode", "Mode: digest, stream, block-enc, stream-enc, dec, info",
                cxxopts::value<std::string>()->default_value("digest"))
            ("a,algorithm", "Specify the hash algorithm to use (rainbow, rainstorm, rainbow-rp, rainstorm-nis1)",
                cxxopts::value<std::string>()->default_value("bow"))
            ("s,size", "Specify the size of the hash (e.g., 64, 128, 256, 512)",
                cxxopts::value<uint32_t>()->default_value("256"))
            ("block-size", "Block size in bytes for puzzle-based encryption (1-255)",
                cxxopts::value<uint8_t>()->default_value("3"))
            ("n,nonce-size", "Size of the nonce in bytes [1-255] (block-enc mode)",
                cxxopts::value<size_t>()->default_value("14"))
            ("deterministic-nonce", "Use a deterministic counter for nonce generation",
                cxxopts::value<bool>()->default_value("false"))
            ("search-mode", "Search mode for plaintext mining block cipher: prefix, sequence, series, scatter, mapscatter, parascatter",
#ifdef _OPENMP
                cxxopts::value<std::string>()->default_value("parascatter"))
#else
                cxxopts::value<std::string>()->default_value("scatter"))
#endif
            ("o,output-file", "Output file (stream mode)",
                cxxopts::value<std::string>()->default_value("/dev/stdout"))
            ("t,test-vectors", "Calculate the hash of the standard test vectors",
                cxxopts::value<bool>()->default_value("false"))
            ("l,output-length", "Output length in hash iterations (stream mode)",
                cxxopts::value<uint64_t>()->default_value("1000000"))
            ("x,output-extension", "Output extension in bytes (block-enc mode). Extend digest by this many bytes to make mining larger P blocks faster",
                cxxopts::value<uint32_t>()->default_value("128"))
            ("seed", "Seed value (0x prefixed hex string or numeric)",
                cxxopts::value<std::string>()->default_value("0x0"))
            ("salt", "Salt value (0x prefixed hex string or string)",
                cxxopts::value<std::string>()->default_value(""))
            // Mining options
            ("mine-mode", "Mining demo mode: chain, nonceInc, nonceRand (mining demo only)",
                cxxopts::value<std::string>()->default_value("None"))
            ("match-prefix", "Hex prefix to match in mining tasks (mining demo only)",
                cxxopts::value<std::string>()->default_value(""))
            // Encrypt / Decrypt options
            ("P,password", "Encryption/Decryption password (raw, insecure)",
                cxxopts::value<std::string>()->default_value(""))
            // ADDED: verbose
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

        // ADDED: verbose
        bool verbose = result["verbose"].as<bool>();


        // Access and validate options

        // Hash Size
        uint32_t hash_size = result["size"].as<uint32_t>();

        // Determine Mode
        std::string modeStr = result["mode"].as<std::string>();
        Mode mode;
        if (modeStr == "digest") mode = Mode::Digest;
        else if (modeStr == "stream") mode = Mode::Stream;
        else if (modeStr == "block-enc") mode = Mode::BlockEnc;
        else if (modeStr == "stream-enc") mode = Mode::StreamEnc;
        else if (modeStr == "dec") mode = Mode::Dec;
        else if (modeStr == "info") mode = Mode::Info;
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
            if ( mode == Mode::BlockEnc || mode == Mode::StreamEnc || mode == Mode::Dec ) {
              algot = HashAlgorithm::Rainstorm;
              hash_size = 512;
            }
        }
        else if (algot == HashAlgorithm::Rainstorm) {
            if (hash_size != 64 && hash_size != 128 && hash_size != 256 && hash_size != 512) {
                throw std::runtime_error("Invalid size for Rainstorm (must be 64, 128, 256, or 512).");
            }
            if ( mode == Mode::BlockEnc || mode == Mode::StreamEnc || mode == Mode::Dec ) {
              hash_size = 512;
            }
        }

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
        uint32_t output_extension = result["output-extension"].as<uint32_t>();

        // Convert Seed (either numeric or hex string)
        std::string seed_str = result["seed"].as<std::string>();
        uint64_t seed = 0;
        bool userProvidedSeed = false;
        if (!seed_str.empty() && seed_str != "0x0") {
          userProvidedSeed = true;
          try {
            if (seed_str.find("0x") == 0 || seed_str.find("0X") == 0) {
              seed = std::stoull(seed_str.substr(2), nullptr, 16);
            } else {
              seed = std::stoull(seed_str, nullptr, 10);
            }
          }
          catch (...) {
            // If not a valid number, hash the string to 64 bits
            seed = hash_string_to_64_bit(seed_str);
          }
        }

        // If user did NOT provide seed, generate a random one
        if (!userProvidedSeed && mode != Mode::Digest) {
          // A simple example using std::random_device + Mersenne Twister
          std::random_device rd;
          std::mt19937_64 rng(rd());
          seed = rng();  // 64-bit random
          if (verbose) {
            std::cerr << "[Info] No seed provided; generated random seed: 0x"
                      << std::hex << seed << std::dec << "\n";
          }
        }

        // SALT handling:
        std::string salt_str = result["salt"].as<std::string>();
        std::vector<uint8_t> salt;
        bool userProvidedSalt = false;

        if (!salt_str.empty()) {
          userProvidedSalt = true;
          if (salt_str.find("0x") == 0 || salt_str.find("0X") == 0) {
            // Hex string
            salt_str = salt_str.substr(2);
            if (salt_str.size() % 2 != 0) {
              throw std::runtime_error("Salt hex string must have even length.");
            }
            for (size_t i = 0; i < salt_str.size(); i += 2) {
              uint8_t byte = static_cast<uint8_t>(std::stoul(salt_str.substr(i, 2), nullptr, 16));
              salt.push_back(byte);
            }
          } else {
            // Regular string
            salt.assign(salt_str.begin(), salt_str.end());
          }
        }

        // If user did NOT provide salt, generate random salt with size 32 (or hash_size/8, your choice)
        if (!userProvidedSalt && mode != Mode::Digest) {
          // For example, 32 random bytes:
          const size_t saltLen = 32;
          std::random_device rd;
          std::mt19937_64 rng(rd());
          salt.resize(saltLen);
          for (size_t i = 0; i < saltLen; ++i) {
            salt[i] = static_cast<uint8_t>(rng());
          }
          if (verbose) {
            std::cerr << "[Info] No salt provided; generated random 32-byte salt:\n  ";
            for (auto &b : salt) {
              std::cerr << std::hex << (int)b << " ";
            }
            std::cerr << std::dec << "\n";
          }
        }

        // Test Vectors
        bool use_test_vectors = result["test-vectors"].as<bool>();

        // Output Length
        uint64_t output_length = result["output-length"].as<uint64_t>();

        // Adjust output_length based on mode
        if (mode == Mode::Digest) {
            // digest mode output_length is just the digest size
            output_length = hash_size / 8;
        }
        else if (mode == Mode::Stream) {
            // stream mode is hash output in bytes by output_length because output length is then iterations of hash
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

            // Show unified FileHeader info
            showFileFullInfo(inpath);
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
            std::string key_input;
            if (!password.empty()) {
                key_input = password;
            }
            else {
                key_input = promptForKey("Enter encryption key: ");
            }

            // We'll write ciphertext to inpath + ".rc"
            std::string encFile = inpath + ".rc";

            // Check if encFile exists and overwrite it with zeros if it does
            try {
                overwriteFileWithZeros(encFile);
                if (std::filesystem::exists(encFile)) {
                    std::cout << "[Info] Existing encrypted file '" << encFile << "' has been securely overwritten with zeros.\n";
                }
            }
            catch (const std::exception &e) {
                throw std::runtime_error("Error while overwriting existing encrypted file: " + std::string(e.what()));
            }

            // ADDED: Call the existing puzzleEncryptFileWithHeader with updated header
            puzzleEncryptFileWithHeader(inpath, encFile, key_input, algot, hash_size, seed, salt, blockSize, nonceSize, searchMode, verbose, deterministicNonce, output_extension);
            std::cout << "[Enc] Wrote encrypted file to: " << encFile << "\n";
        }
        else if (mode == Mode::StreamEnc) {
            if (inpath.empty()) {
                throw std::runtime_error("No input file specified for encryption.");
            }
            std::string key_input;
            if (!password.empty()) {
                key_input = password;
            }
            else {
                key_input = promptForKey("Enter encryption key: ");
            }

            // We'll write ciphertext to inpath + ".rc"
            std::string encFile = inpath + ".rc";

            // Check if encFile exists and overwrite it with zeros if it does
            try {
                overwriteFileWithZeros(encFile);
                if (std::filesystem::exists(encFile)) {
                    std::cout << "[Info] Existing encrypted file '" << encFile << "' has been securely overwritten with zeros.\n";
                }
            }
            catch (const std::exception &e) {
                throw std::runtime_error("Error while overwriting existing encrypted file: " + std::string(e.what()));
            }

            // ADDED: Call streamEncryptFileWithHeader
            streamEncryptFileWithHeader(
                inpath,
                encFile,
                key_input,
                algot,
                hash_size,
                seed,          // Using seed as IV
                salt,          // Using provided salt
                output_extension,
                verbose
            );
            std::cout << "[StreamEnc] Wrote encrypted file to: " << encFile << "\n";
        }
        else if (mode == Mode::Dec) {
            if (inpath.empty()) {
                throw std::runtime_error("No ciphertext file specified for decryption.");
            }
            std::string key_input;
            if (!password.empty()) {
                key_input = password;
            }
            else {
                key_input = promptForKey("Enter decryption key: ");
            }

            // We'll write plaintext to inpath + ".dec"
            std::string decFile = inpath + ".dec";

            // ADDED: Check cipherMode from header
            std::ifstream fin(inpath, std::ios::binary);
            if (!fin.is_open()) {
                throw std::runtime_error("[Dec] Cannot open ciphertext file: " + inpath);
            }
            FileHeader hdr = readFileHeader(fin);
            fin.close();

            if (hdr.magic != MagicNumber) {
                throw std::runtime_error("[Dec] Invalid magic number in header.");
            }

            if (hdr.cipherMode == 0x10) { // Stream Cipher Mode
                // ADDED: Stream Decryption
                streamDecryptFileWithHeader(
                    inpath,
                    decFile,
                    key_input,
                    verbose
                );
                std::cout << "[Dec] Wrote decrypted plaintext to: " << decFile << "\n";
            }
            else if (hdr.cipherMode == 0x11) { // Block Cipher Mode
                // ADDED: Block Decryption (integration with tool.h assumed)
                puzzleDecryptFileWithHeader(inpath, decFile, key_input);
                std::cout << "[Dec] Wrote decrypted plaintext to: " << decFile << "\n";
            }
            else {
                throw std::runtime_error("[Dec] Unknown cipher mode in header.");
            }
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

