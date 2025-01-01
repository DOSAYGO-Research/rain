
// ADDED: Unified FileHeader struct to include IV and Salt
struct FileHeader {
    uint32_t magic;             // MagicNumber
    uint8_t version;            // Version
    uint8_t cipherMode;         // Mode: 0x10 = Stream, 0x11 = Block
    uint8_t blockSize;          // Block size in bytes (for block cipher)
    uint8_t nonceSize;          // Nonce size in bytes
    uint16_t hashSizeBits;      // Hash size in bits
    uint32_t outputExtension;   // How much to extend output for PMC or to drop for stream
    std::string hashName;       // Hash algorithm name
    uint64_t iv;                // 64-bit IV (seed)
    uint8_t saltLen;            // Length of salt
    std::vector<uint8_t> salt;  // Salt data
    uint8_t searchModeEnum;     // Search mode enum (0x00 - 0x05 for block ciphers, 0xFF for stream)
    uint64_t originalSize;      // Compressed plaintext size
};

// Updated function to write the unified FileHeader
static void writeFileHeader(std::ofstream &out, const FileHeader &hdr) {
    out.write(reinterpret_cast<const char*>(&hdr.magic), sizeof(hdr.magic));
    out.write(reinterpret_cast<const char*>(&hdr.version), sizeof(hdr.version));
    out.write(reinterpret_cast<const char*>(&hdr.cipherMode), sizeof(hdr.cipherMode));
    out.write(reinterpret_cast<const char*>(&hdr.blockSize), sizeof(hdr.blockSize));
    out.write(reinterpret_cast<const char*>(&hdr.nonceSize), sizeof(hdr.nonceSize));
    out.write(reinterpret_cast<const char*>(&hdr.hashSizeBits), sizeof(hdr.hashSizeBits));
    out.write(reinterpret_cast<const char*>(&hdr.outputExtension), sizeof(hdr.outputExtension)); // New field

    uint8_t hnLen = static_cast<uint8_t>(hdr.hashName.size());
    out.write(reinterpret_cast<const char*>(&hnLen), sizeof(hnLen));
    out.write(hdr.hashName.data(), hnLen);

    out.write(reinterpret_cast<const char*>(&hdr.iv), sizeof(hdr.iv));

    out.write(reinterpret_cast<const char*>(&hdr.saltLen), sizeof(hdr.saltLen));
    if (hdr.saltLen > 0) {
        out.write(reinterpret_cast<const char*>(hdr.salt.data()), hdr.salt.size());
    }

    out.write(reinterpret_cast<const char*>(&hdr.searchModeEnum), sizeof(hdr.searchModeEnum));
    out.write(reinterpret_cast<const char*>(&hdr.originalSize), sizeof(hdr.originalSize));
}

// Updated function to read the unified FileHeader
static FileHeader readFileHeader(std::ifstream &in) {
    FileHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr.magic), sizeof(hdr.magic));
    in.read(reinterpret_cast<char*>(&hdr.version), sizeof(hdr.version));
    in.read(reinterpret_cast<char*>(&hdr.cipherMode), sizeof(hdr.cipherMode));
    in.read(reinterpret_cast<char*>(&hdr.blockSize), sizeof(hdr.blockSize));
    in.read(reinterpret_cast<char*>(&hdr.nonceSize), sizeof(hdr.nonceSize));
    in.read(reinterpret_cast<char*>(&hdr.hashSizeBits), sizeof(hdr.hashSizeBits));
    in.read(reinterpret_cast<char*>(&hdr.outputExtension), sizeof(hdr.outputExtension)); // New field

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

    in.read(reinterpret_cast<char*>(&hdr.searchModeEnum), sizeof(hdr.searchModeEnum));
    in.read(reinterpret_cast<char*>(&hdr.originalSize), sizeof(hdr.originalSize));

    return hdr;
}

// =================================================================
// [BEAUTIFUL] Enhanced showFileFullInfo to handle unified FileHeader
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
    std::cout << "Output Extension: " << hdr.outputExtension << " bytes\n"; // New field
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
    std::cout << "Search Mode Enum: 0x" << std::hex << static_cast<int>(hdr.searchModeEnum) << std::dec << "\n";
    std::cout << "===============================\n";
}


