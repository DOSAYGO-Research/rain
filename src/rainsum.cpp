#ifdef _OPENMP
#include <omp.h>
#endif
#include "tool.h" // for invokeHash, etc.

// =================================================================
// ADDED: Unified FileHeader Struct and Associated Helpers
// =================================================================


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
    // Convert seed to vector<uint8_t>
    std::vector<uint8_t> seed_vec(8);
    for (size_t i = 0; i < 8; ++i) {
        seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
    }

    std::vector<uint8_t> ikm(key.begin(), key.end());
    std::vector<uint8_t> prk = derivePRK(seed_vec, salt, ikm, algot, hash_bits); // Using seed_vec

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
    std::vector<uint8_t> cipherData(
        (std::istreambuf_iterator<char>(fin)),
        (std::istreambuf_iterator<char>())
    );
    fin.close();

    // 3) Derive PRK
    // Convert seed to vector<uint8_t>
    std::vector<uint8_t> seed_vec(8);
    for (size_t i = 0; i < 8; ++i) {
        seed_vec[i] = static_cast<uint8_t>((hdr.iv >> (i * 8)) & 0xFF);
    }

    std::vector<uint8_t> ikm(key.begin(), key.end());
    std::vector<uint8_t> prk = derivePRK(seed_vec, hdr.salt, ikm, algot, hash_bits); // Using seed_vec

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
    std::cout << "Search Mode Enum: 0x" << std::hex << static_cast<int>(hdr.searchModeEnum) << std::dec << "\n";
    std::cout << "===============================\n";
}
  // parallel scatter
    // A struct to hold all the parameters we need in parallel
    struct ParascatterArgs {
      size_t blockIndex;
      size_t thisBlockSize;
      std::vector<uint8_t> block;
      std::vector<uint8_t> blockSubkey;
      size_t nonceSize;
      size_t hash_size;
      uint64_t seed;
      HashAlgorithm algot;
      bool verbose;
      bool deterministicNonce;
      size_t progressInterval;
      // Note: We can't store references, but we can store pointers or a std::ofstream*.
      std::ofstream* fout;
    };

    void parallelParascatter(
        size_t blockIndex,
        size_t thisBlockSize,
        const std::vector<uint8_t>& block,
        const std::vector<uint8_t>& blockSubkey,
        size_t nonceSize,
        size_t hash_size,
        uint64_t seed,
        HashAlgorithm algot,
        std::ofstream& fout,
        bool verbose,
        bool deterministicNonce,
        size_t progressInterval
    ) {
      // Step 1: Heap-allocate a struct and copy all parameters into it
      std::unique_ptr<ParascatterArgs> args(new ParascatterArgs{
        blockIndex,
        thisBlockSize,
        block,
        blockSubkey,
        nonceSize,
        hash_size,
        seed,
        algot,
        verbose,
        deterministicNonce,
        progressInterval,
        &fout
      });

      // Step 2: Shared data for solution
      std::atomic<bool> found(false);
      std::vector<uint8_t> chosenNonce(args->nonceSize, 0);
      std::vector<uint8_t> scatterIndices(args->thisBlockSize, 0);

      // Step 3: Mutexes
      static std::mutex consoleMutex;
      static std::mutex stateMutex;
      uint64_t tries;

      // Step 4: Parallel region. 
      // We pass the pointer 'args.get()' as shared, so nobody touches the main thread’s stack.
      #pragma omp parallel default(none) \
        shared(args, found, chosenNonce, scatterIndices, consoleMutex, stateMutex, std::cout, std::cerr, block, thisBlockSize) private(blockIndex) firstprivate(nonceSize, hash_size, seed, verbose, deterministicNonce, progressInterval)
      {
        // Thread-local variables
        std::vector<uint8_t> localNonce(args->nonceSize);
        std::vector<uint8_t> localScatterIndices(args->thisBlockSize);

        std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);

        uint64_t nonceCounter = 0;
        uint64_t tries;

        for (tries = 0; !found.load(std::memory_order_acquire); ) {
          // Generate nonce
          if (args->deterministicNonce) {
            for (size_t i = 0; i < args->nonceSize; ++i) {
              localNonce[i] = static_cast<uint8_t>((nonceCounter >> (i * 8)) & 0xFF);
            }
            #pragma omp atomic
            ++nonceCounter;
          } else {
            for (size_t i = 0; i < args->nonceSize; ++i) {
              localNonce[i] = dist(rng);
            }
          }

          // Build trial buffer
          std::vector<uint8_t> trial(args->blockSubkey);
          trial.insert(trial.end(), localNonce.begin(), localNonce.end());

          // Hash it
          std::vector<uint8_t> hashOut(args->hash_size / 8);
          invokeHash<false>(args->algot, args->seed, trial, hashOut, args->hash_size);

          bool allFound = true;
          std::bitset<64> usedIndices;
          usedIndices.reset();

          for (size_t byteIdx = 0; byteIdx < thisBlockSize; ++byteIdx) {
            auto it = hashOut.begin();
            while (it != hashOut.end()) {
              it = std::find(it, hashOut.end(), block[byteIdx]);
              if (it != hashOut.end()) {
                uint8_t idx = static_cast<uint8_t>(std::distance(hashOut.begin(), it));
                if (!usedIndices.test(idx)) {
                  localScatterIndices[byteIdx] = idx;
                  usedIndices.set(idx);
                  break;
                }
                ++it;
              } else {
                allFound = false;
                break;
              }
            }
            if (it == hashOut.end()) {
                allFound = false;
                break;
            }
          }

          if (allFound) {
            if (verbose) {
              // Synchronize console output
              std::lock_guard<std::mutex> lock(consoleMutex);
              std::cout << "Scatter Indices: ";
              for (auto idx : localScatterIndices) {
                std::cout << static_cast<int>(idx) << " ";
              }
              std::cout << std::endl;
            }

            #pragma omp critical
            {
              std::lock_guard<std::mutex> lock(stateMutex);
              if (!found.exchange(true, std::memory_order_acq_rel)) {
                chosenNonce = localNonce;
                scatterIndices = localScatterIndices;
                found.store(true);
              }
            }
            break;
          }

          tries++;
        } // End of tries loop
        if (tries % progressInterval == 0) {
          // Synchronize console output
          std::lock_guard<std::mutex> lock(consoleMutex);
          std::cerr << "\r[Enc] Mode: parascatter, Block " << (blockIndex + 1)
                      << ", Tries: " << tries << "..." << std::flush;
        }

      } // End of parallel region
      // Periodic progress reporting

      if (found.load(std::memory_order_acquire)) {
        args->fout->write(reinterpret_cast<const char*>(chosenNonce.data()), nonceSize);
        args->fout->write(reinterpret_cast<const char*>(scatterIndices.data()), scatterIndices.size());
      } else {
        throw std::runtime_error("Failed to find a solution for parascatter block.");
      }
    }

  // encrypt block cipher
    static void puzzleEncryptFileWithHeader(
        const std::string &inFilename,
        const std::string &outFilename,
        const std::string &key,
        HashAlgorithm algot,
        uint32_t hash_size,
        uint64_t seed,             // Used as IV
        std::vector<uint8_t> salt, // Provided salt
        size_t blockSize,
        size_t nonceSize,
        const std::string &searchMode,
        bool verbose,
        bool deterministicNonce
    ) {
        // 1) Read & compress plaintext
        std::ifstream fin(inFilename, std::ios::binary);
        if (!fin.is_open()) {
            throw std::runtime_error("Cannot open input file: " + inFilename);
        }
        std::vector<uint8_t> plainData(
            (std::istreambuf_iterator<char>(fin)),
            (std::istreambuf_iterator<char>())
        );
        fin.close();

        //auto compressed = compressData(plainData);
        auto compressed = plainData;

        plainData = std::move(compressed);

        // 2) Prepare output & new FileHeader
        std::ofstream fout(outFilename, std::ios::binary);
        if (!fout.is_open()) {
            throw std::runtime_error("Cannot open output file: " + outFilename);
        }

        // 3) Determine searchModeEnum based on searchMode string
        uint8_t searchModeEnum = 0xFF; // Default for Stream Cipher Mode
        if (algot == HashAlgorithm::Rainbow || algot == HashAlgorithm::Rainstorm) { // Only for Block Cipher Modes
            if (searchMode == "prefix") {
                searchModeEnum = 0x00;
            }
            else if (searchMode == "sequence") {
                searchModeEnum = 0x01;
            }
            else if (searchMode == "series") {
                searchModeEnum = 0x02;
            }
            else if (searchMode == "scatter") {
                searchModeEnum = 0x03;
            }
            else if (searchMode == "mapscatter") {
                searchModeEnum = 0x04;
            }
            else if (searchMode == "parascatter") {
                searchModeEnum = 0x05;
            }
            else {
                throw std::runtime_error("Invalid search mode: " + searchMode);
            }
        }

        // 4) Build the new FileHeader
        FileHeader hdr{};
        hdr.magic        = MagicNumber;
        hdr.version      = 0x02;       // New version
        hdr.cipherMode   = 0x11;       // 0x11 for “Block Cipher + puzzle”
        hdr.blockSize    = static_cast<uint8_t>(blockSize);
        hdr.nonceSize    = static_cast<uint8_t>(nonceSize);
        hdr.hashSizeBits = hash_size;
        hdr.hashName     = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
        hdr.iv           = seed;       // The seed as our “IV”

        // 5) Assign provided salt
        hdr.saltLen = static_cast<uint8_t>(salt.size());
        hdr.salt    = salt;

        // 6) Assign original (compressed) size
        hdr.originalSize = plainData.size();

        // 7) Assign searchModeEnum
        hdr.searchModeEnum = searchModeEnum;

        // 8) Write the unified header
        writeFileHeader(fout, hdr);

        // 9) Prepare puzzle searching
        size_t totalBlocks = (hdr.originalSize + blockSize - 1) / blockSize;
        int progressInterval = 1'000'000;

        // 10) Derive a “master PRK” from (seed, salt, key)
        // Using your iterative KDF
        std::vector<uint8_t> keyBuf(key.begin(), key.end());
        // Convert seed (uint64_t) to vector<uint8_t>
        std::vector<uint8_t> seed_vec(8);
        for (size_t i = 0; i < 8; ++i) {
            seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
        }

        std::vector<uint8_t> prk = derivePRK(
            seed_vec,          // Converted seed as vector<uint8_t>
            salt,              // Provided salt
            keyBuf,            // IKM (Input Key Material)
            algot,             // Hash algorithm
            hash_size          // Hash size in bits
        );

        // 11) Extend that PRK into subkeys for each block
        // Each subkey is hash_size/8 bytes
        size_t subkeySize = hdr.hashSizeBits / 8;
        size_t totalNeeded = totalBlocks * subkeySize;
        std::vector<uint8_t> allSubkeys = extendOutputKDF(
            prk,
            totalNeeded,
            algot,
            hdr.hashSizeBits
        );

        // 12) Initialize RNG for non-deterministic nonce generation
        std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        uint64_t nonceCounter = 0;

        size_t remaining = hdr.originalSize;
        // For mapscatter arrays
        uint8_t reverseMap[256 * 64] = {0};
        uint8_t reverseMapOffsets[256] = {0};

        // 13) Iterate over each block to perform puzzle encryption
        for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
            size_t thisBlockSize = std::min(blockSize, remaining);
            remaining -= thisBlockSize;

            // Extract this block
            std::vector<uint8_t> block(
                plainData.begin() + blockIndex * blockSize,
                plainData.begin() + blockIndex * blockSize + thisBlockSize
            );

            // Extract subkey for this block
            const size_t offset = blockIndex * subkeySize;
            if (offset + subkeySize > allSubkeys.size()) {
                throw std::runtime_error("Subkey index out of range.");
            }
            std::vector<uint8_t> blockSubkey(
                allSubkeys.begin() + offset,
                allSubkeys.begin() + offset + subkeySize
            );

            // -----------------------
            // parascatter branch
            // -----------------------

            if (searchModeEnum == 0x05) { // parascatter
              parallelParascatter(
                blockIndex,             // Current block index
                thisBlockSize,          // Size of this block
                block,                  // Block data
                blockSubkey,            // Subkey for this block
                nonceSize,              // Nonce size
                hash_size,              // Hash size in bits
                seed,                   // Seed
                algot,                  // Hash algorithm
                fout,                   // Output file stream
                verbose,                // Verbose flag
                deterministicNonce,     // Deterministic nonce flag
                progressInterval        // Progress reporting interval
              );
            }

            // ------------------------------------------------------
            // other modes in else branch
            // ------------------------------------------------------
            else {
                // We'll fill these once a solution is found
                bool found = false;
                std::vector<uint8_t> chosenNonce(nonceSize, 0);
                std::vector<uint8_t> scatterIndices(thisBlockSize, 0);

                for (uint64_t tries = 0; ; tries++) {
                    // Generate the nonce
                    if (deterministicNonce) {
                        for (size_t i = 0; i < nonceSize; i++) {
                            chosenNonce[i] = static_cast<uint8_t>((nonceCounter >> (i * 8)) & 0xFF);
                        }
                        nonceCounter++;
                    }
                    else {
                        for (size_t i = 0; i < nonceSize; i++) {
                            chosenNonce[i] = dist(rng);
                        }
                    }

                    // Build trial buffer
                    std::vector<uint8_t> trial(blockSubkey);
                    trial.insert(trial.end(), chosenNonce.begin(), chosenNonce.end());

                    // Hash it
                    std::vector<uint8_t> hashOut(hash_size / 8);
                    invokeHash<false>(algot, seed, trial, hashOut, hash_size);

                    if (searchModeEnum == 0x00) { // prefix
                        if (hashOut.size() >= thisBlockSize &&
                            std::equal(block.begin(), block.end(), hashOut.begin())) {
                            scatterIndices.assign(thisBlockSize, 0);
                            found = true;
                        }
                    }
                    else if (searchModeEnum == 0x01) { // sequence
                        for (size_t i = 0; i <= hashOut.size() - thisBlockSize; i++) {
                            if (std::equal(block.begin(), block.end(), hashOut.begin() + i)) {
                                uint8_t startIdx = static_cast<uint8_t>(i);
                                scatterIndices.assign(thisBlockSize, startIdx);
                                found = true;
                                break;
                            }
                        }
                    }
                    else if (searchModeEnum == 0x02) { // series
                        bool allFound = true;
                        std::bitset<64> usedIndices;
                        usedIndices.reset();
                        auto it = hashOut.begin();

                        for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                            while (it != hashOut.end()) {
                                it = std::find(it, hashOut.end(), block[byteIdx]);
                                if (it != hashOut.end()) {
                                    uint8_t idx = static_cast<uint8_t>(std::distance(hashOut.begin(), it));
                                    if (!usedIndices.test(idx)) {
                                        scatterIndices[byteIdx] = idx;
                                        usedIndices.set(idx);
                                        break;
                                    }
                                    ++it;
                                }
                                else {
                                    allFound = false;
                                    break;
                                }
                            }
                            if (it == hashOut.end()) {
                                allFound = false;
                                break;
                            }
                        }

                        if (allFound) {
                            found = true;
                            if (verbose) {
                                std::cout << "Series Indices: ";
                                for (auto idx : scatterIndices) {
                                    std::cout << static_cast<int>(idx) << " ";
                                }
                                std::cout << std::endl;
                            }
                        }
                    }
                    else if (searchModeEnum == 0x03) { // scatter
                        bool allFound = true;
                        std::bitset<64> usedIndices;
                        usedIndices.reset();

                        for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                            auto it = hashOut.begin();
                            while (it != hashOut.end()) {
                                it = std::find(it, hashOut.end(), block[byteIdx]);
                                if (it != hashOut.end()) {
                                    uint8_t idx = static_cast<uint8_t>(std::distance(hashOut.begin(), it));
                                    if (!usedIndices.test(idx)) {
                                        scatterIndices[byteIdx] = idx;
                                        usedIndices.set(idx);
                                        break;
                                    }
                                    ++it;
                                }
                                else {
                                    allFound = false;
                                    break;
                                }
                            }
                            if (it == hashOut.end()) {
                                allFound = false;
                                break;
                            }
                        }

                        if (allFound) {
                            found = true;
                            if (verbose) {
                                std::cout << "Scatter Indices: ";
                                for (auto idx : scatterIndices) {
                                    std::cout << static_cast<int>(idx) << " ";
                                }
                                std::cout << std::endl;
                            }
                        }
                    }
                    else if (searchModeEnum == 0x04) { // mapscatter
                        // Reset offsets
                        std::fill(std::begin(reverseMapOffsets), std::end(reverseMapOffsets), 0);

                        // Fill the map with all positions of each byte in hashOut
                        for (uint8_t i = 0; i < hashOut.size(); i++) {
                            uint8_t b = hashOut[i];
                            reverseMap[b * 64 + reverseMapOffsets[b]] = i;
                            reverseMapOffsets[b]++;
                        }

                        bool allFound = true;
                        for (size_t byteIdx = 0; byteIdx < thisBlockSize; ++byteIdx) {
                            uint8_t targetByte = block[byteIdx];
                            if (reverseMapOffsets[targetByte] == 0) {
                                allFound = false;
                                break;
                            }
                            reverseMapOffsets[targetByte]--;
                            scatterIndices[byteIdx] =
                                reverseMap[targetByte * 64 + reverseMapOffsets[targetByte]];
                        }

                        if (allFound) {
                            found = true;
                            if (verbose) {
                                std::cout << "Scatter Indices: ";
                                for (auto idx : scatterIndices) {
                                    std::cout << static_cast<int>(idx) << " ";
                                }
                                std::cout << std::endl;
                            }
                        }
                    }

                    if (found) {
                        // For non-parallel modes, write nonce and indices
                        fout.write(reinterpret_cast<const char*>(chosenNonce.data()), nonceSize);
                        if (searchModeEnum == 0x02 || searchModeEnum == 0x03 || searchModeEnum == 0x04) {
                            fout.write(reinterpret_cast<const char*>(scatterIndices.data()), scatterIndices.size());
                        }
                        else {
                            // prefix or sequence
                            uint8_t startIdx = scatterIndices[0];
                            fout.write(reinterpret_cast<const char*>(&startIdx), sizeof(startIdx));
                        }
                        break; // done with this block
                    }

                    // Periodic progress for non-parallel tries
                    if (tries % progressInterval == 0) {
                        std::cerr << "\r[Enc] Mode: " << searchMode
                                  << ", Block " << (blockIndex + 1) << "/" << totalBlocks
                                  << ", " << tries << " tries..." << std::flush;
                    }
                } // end tries loop

                // Display block progress
                if (verbose) {
                    std::cerr << "\r[Enc] Block " << blockIndex + 1 << "/" << totalBlocks << " processed.\n";
                }

            } // end else (other modes)

        } // end block loop

        fout.close();
        std::cout << "[Enc] Block-based puzzle encryption with subkeys complete: " << outFilename << "\n";
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

        // Later in your main function, parse and convert the salt
        std::string salt_str = result["salt"].as<std::string>();
        std::vector<uint8_t> salt; // Existing salt vector
        if (!salt_str.empty()) {
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
            }
            else {
                // Regular string
                salt.assign(salt_str.begin(), salt_str.end());
            }
        }
        else {
            // Assign default zeroed salt to the existing salt vector
            salt.assign(hash_size / 8, 0x00); // Or another method
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
            puzzleEncryptFileWithHeader(inpath, encFile, key_input, algot, hash_size, seed, salt, blockSize, nonceSize, searchMode, verbose, deterministicNonce);
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
                    algot,
                    hash_size,
                    output_extension,
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

