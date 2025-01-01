#ifdef _OPENMP
#include <omp.h>
#endif
#include "tool.h" // for invokeHash, etc.

// =================================================================
// ADDED: Unified FileHeader Struct and Associated Helpers
// =================================================================

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
    uint64_t originalSize;      // Compressed plaintext size
    // Additional fields can be added here if needed (e.g., searchMode for block cipher)
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
    
    out.write(reinterpret_cast<const char*>(&hdr.originalSize), sizeof(hdr.originalSize));
    // If additional fields are present, write them here
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
    
    in.read(reinterpret_cast<char*>(&hdr.originalSize), sizeof(hdr.originalSize));
    // If additional fields are present, read them here
    
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

// ADDED: KDF with Iterations and Info
static const std::string KDF_INFO_STRING = "powered by Rain hashes created by Cris and DOSYAGO (aka DOSAYGO) over the years 2023 through 2025";
static const int KDF_ITERATIONS = 8;

// ADDED: Derive PRK with iterations
static std::vector<uint8_t> derivePRK(const std::vector<uint8_t> &seed,
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
    
    // Iterate the hash function KDF_ITERATIONS times
    for (int i = 0; i < KDF_ITERATIONS; ++i) {
        invokeHash<false>(algot, seed, temp, prk, hash_bits);
        temp = prk; // Next iteration takes the output of the previous
    }
    
    return prk;
}

// ADDED: Extend Output with iterations and counter
static std::vector<uint8_t> extendOutputKDF(const std::vector<uint8_t> &prk,
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

// =================================================================
// ADDED: Stream Encryption and Decryption Functions
// =================================================================

// ADDED: Stream Encryption Function
static void streamEncryptFileWithHeader(
    const std::string &inFilename,
    const std::string &outFilename,
    const std::string &key,
    HashAlgorithm algot,
    uint32_t hash_bits,
    uint64_t seed, // Used as IV
    const std::vector<uint8_t> &salt,
    uint32_t output_extension,
    bool verbose
) {
    // 1) Read and compress the plaintext
    std::ifstream fin(inFilename, std::ios::binary);
    if (!fin.is_open()) {
        throw std::runtime_error("[StreamEnc] Cannot open input file: " + inFilename);
    }
    std::vector<uint8_t> plainData((std::istreambuf_iterator<char>(fin)),
                                   (std::istreambuf_iterator<char>()));
    fin.close();
    auto compressed = compressData(plainData); // Assuming compressData is defined in tool.h

    // 2) Open output file
    std::ofstream fout(outFilename, std::ios::binary);
    if (!fout.is_open()) {
        throw std::runtime_error("[StreamEnc] Cannot open output file: " + outFilename);
    }

    // 3) Prepare FileHeader
    FileHeader hdr{};
    hdr.magic = MagicNumber;
    hdr.version = 0x02; // Example version
    hdr.cipherMode = 0x10; // Stream Cipher Mode
    hdr.blockSize = 0; // Not used for stream cipher
    hdr.nonceSize = 0; // Not used for stream cipher
    hdr.hashSizeBits = hash_bits;
    hdr.hashName = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
    hdr.iv = seed; // Using seed as IV
    hdr.saltLen = static_cast<uint8_t>(salt.size());
    hdr.salt = salt;
    hdr.originalSize = compressed.size();

    // 4) Write FileHeader
    writeFileHeader(fout, hdr);

    // 5) Derive PRK
    std::vector<uint8_t> ikm(key.begin(), key.end());
    std::vector<uint8_t> prk = derivePRK(ikm, salt, ikm, algot, hash_bits); // Using ikm as both seed and ikm

    // 6) Generate Keystream with KDF
    size_t needed = compressed.size();
    auto keystream = extendOutputKDF(prk, needed, algot, hash_bits);

    // 7) XOR compressed data with keystream
    for (size_t i = 0; i < compressed.size(); ++i) {
        compressed[i] ^= keystream[i];
    }

    // 8) Write ciphertext
    fout.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    fout.close();

    if (verbose) {
        std::cerr << "[StreamEnc] Encrypted " << compressed.size() 
                  << " bytes from " << inFilename 
                  << " to " << outFilename 
                  << " using IV=0x" << std::hex << seed << std::dec 
                  << " and salt of length " << static_cast<int>(hdr.saltLen) << " bytes.\n";
    }
}

// ADDED: Stream Decryption Function
static void streamDecryptFileWithHeader(
    const std::string &inFilename,
    const std::string &outFilename,
    const std::string &key,
    HashAlgorithm algot,
    uint32_t hash_bits,
    uint32_t output_extension,
    bool verbose
) {
    // 1) Open input file and read FileHeader
    std::ifstream fin(inFilename, std::ios::binary);
    if (!fin.is_open()) {
        throw std::runtime_error("[StreamDec] Cannot open input file: " + inFilename);
    }
    FileHeader hdr = readFileHeader(fin);
    if (hdr.magic != MagicNumber) {
        throw std::runtime_error("[StreamDec] Invalid magic number in header.");
    }
    if (hdr.cipherMode != 0x10) { // Stream Cipher Mode
        throw std::runtime_error("[StreamDec] File is not in Stream Cipher mode.");
    }

    // 2) Read ciphertext
    std::vector<uint8_t> cipherData((std::istreambuf_iterator<char>(fin)),
                                    (std::istreambuf_iterator<char>()));
    fin.close();

    // 3) Derive PRK
    std::vector<uint8_t> ikm(key.begin(), key.end());
    std::vector<uint8_t> prk = derivePRK(ikm, hdr.salt, ikm, algot, hash_bits); // Using ikm as both seed and ikm

    // 4) Generate Keystream with KDF
    size_t needed = cipherData.size();
    auto keystream = extendOutputKDF(prk, needed, algot, hash_bits);

    // 5) XOR ciphertext with keystream
    for (size_t i = 0; i < cipherData.size(); ++i) {
        cipherData[i] ^= keystream[i];
    }

    // 6) Decompress to get plaintext
    auto decompressed = decompressData(cipherData); // Assuming decompressData is defined in tool.h

    // 7) Write plaintext to output file
    std::ofstream fout(outFilename, std::ios::binary);
    if (!fout.is_open()) {
        throw std::runtime_error("[StreamDec] Cannot open output file: " + outFilename);
    }
    fout.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
    fout.close();

    if (verbose) {
        std::cerr << "[StreamDec] Decrypted " << decompressed.size() 
                  << " bytes from " << inFilename 
                  << " to " << outFilename 
                  << " using IV=0x" << std::hex << hdr.iv << std::dec 
                  << " and salt of length " << static_cast<int>(hdr.saltLen) << " bytes.\n";
    }
}

// =================================================================
// ADDED: Enhanced showFileFullInfo to handle unified FileHeader
// =================================================================
static void showFileFullInfo(const std::string &inFilename) {
    std::ifstream fin(inFilename, std::ios::binary);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open file for info: " + inFilename);
    }

    FileHeader hdr = readFileHeader(fin);
    fin.close();

    if (hdr.magic != MagicNumber) {
        throw std::runtime_error("Invalid magic number in file.");
    }

    std::string cipherModeStr;
    if (hdr.cipherMode == 0x10) {
        cipherModeStr = "StreamCipher";
    }
    else if (hdr.cipherMode == 0x11) {
        cipherModeStr = "BlockCipher";
    }
    else {
        cipherModeStr = "Unknown/LegacyPuzzle";
    }

    std::cout << "=== Unified File Header Info ===\n";
    std::cout << "Magic: RCRY (0x" << std::hex << hdr.magic << std::dec << ")\n";
    std::cout << "Version: " << static_cast<int>(hdr.version) << "\n";
    std::cout << "Cipher Mode: " << cipherModeStr << " (0x" 
              << std::hex << static_cast<int>(hdr.cipherMode) << std::dec << ")\n";
    std::cout << "Block Size: " << static_cast<int>(hdr.blockSize) << "\n";
    std::cout << "Nonce Size: " << static_cast<int>(hdr.nonceSize) << "\n";
    std::cout << "Hash Size: " << hdr.hashSizeBits << " bits\n";
    std::cout << "Hash Algorithm: " << hdr.hashName << "\n";
    std::cout << "IV (Seed): 0x" << std::hex << hdr.iv << std::dec << "\n";
    std::cout << "Salt Length: " << static_cast<int>(hdr.saltLen) << "\n";
    if (hdr.saltLen > 0) {
        std::cout << "Salt Data: ";
        for (auto b : hdr.salt) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(b) << " ";
        }
        std::cout << std::dec << "\n";
    }
    std::cout << "Compressed Plaintext Size: " << hdr.originalSize << " bytes\n";
    std::cout << "===============================\n";
}

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
            ("seed", "Seed value (hex string or numeric)",
                cxxopts::value<std::string>()->default_value("0"))
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
        uint32_t output_extension = result["output-extension"].as<uint32_t>();

        // Convert Seed (either numeric or hex string)
        std::string seed_str = result["seed"].as<std::string>();
        uint64_t seed = 0;
        bool userProvidedSeed = false;
        if (!seed_str.empty()) {
            try {
                if (seed_str.find("0x") == 0 || seed_str.find("0X") == 0) {
                    seed = std::stoull(seed_str.substr(2), nullptr, 16);
                }
                else {
                    seed = std::stoull(seed_str, nullptr, 10);
                }
                userProvidedSeed = true;
            }
            catch (...) {
                // If not a valid number, hash the string to 64 bits
                seed = hash_string_to_64_bit(seed_str); // Assuming this function exists in tool.h
            }
        }

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

        // ADDED: verbose
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
            std::string key;
            if (!password.empty()) {
                key = password;
            }
            else {
                key = promptForKey("Enter encryption key: ");
            }

            // We'll write ciphertext to inpath + ".rc"
            std::string encFile = inpath + ".rc";
            // ADDED: Construct FileHeader for BlockEnc
            FileHeader hdr{};
            hdr.magic = MagicNumber;
            hdr.version = 0x01; // Example version
            hdr.cipherMode = 0x11; // Block Cipher Mode
            hdr.blockSize = blockSize;
            hdr.nonceSize = static_cast<uint8_t>(nonceSize);
            hdr.hashSizeBits = hash_size;
            hdr.hashName = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
            hdr.iv = seed; // Seed used as IV
            hdr.saltLen = 0; // To be updated by tool later
            hdr.originalSize = 0; // To be updated by tool later

            // ADDED: Write FileHeader
            std::ofstream fout(encFile, std::ios::binary);
            if (!fout.is_open()) {
                throw std::runtime_error("[BlockEnc] Cannot open output file: " + encFile);
            }
            writeFileHeader(fout, hdr);
            fout.close();

            // ADDED: Call the existing puzzleEncryptFileWithHeader with updated header
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

            // ADDED: Default salt to hash_size/8 bytes of 0x00 if not provided
            std::vector<uint8_t> salt(hash_size / 8, 0x00); // Default salt

            // ADDED: You can parse salt from command line if desired (not shown here)

            // ADDED: Call streamEncryptFileWithHeader
            streamEncryptFileWithHeader(
                inpath,
                encFile,
                key,
                algot,
                hash_size,
                seed,          // Using seed as IV
                salt,          // Using default salt
                output_extension,
                verbose
            );
            std::cout << "[StreamEnc] Wrote encrypted file to: " << encFile << "\n";
        }
        else if (mode == Mode::Dec) {
            if (inpath.empty()) {
                throw std::runtime_error("No ciphertext file specified for decryption.");
            }
            std::string key;
            if (!password.empty()) {
                key = password;
            }
            else {
                key = promptForKey("Enter decryption key: ");
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
                    key,
                    algot,
                    hash_size,
                    output_extension,
                    verbose
                );
                std::cout << "[Dec] Wrote decrypted plaintext to: " << decFile << "\n";
            }
            else if (hdr.cipherMode == 0x11) { // Block Cipher Mode
                // ADDED: Block Decryption (integration with tool.h assumed)
                puzzleDecryptFileWithHeader(inpath, decFile, key );
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

