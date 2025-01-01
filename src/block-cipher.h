#include "parallel-scatter.h"
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
        bool deterministicNonce,
        uint32_t outputExtension
    ) {
        if (hash_size / 8 + outputExtension > 65536) {
          std::cerr << "[Warning] Total output length ("
                      << hash_size / 8 + outputExtension
                      << ") exceeds the cap of 65536 bytes.\n";
          outputExtension = std::min(outputExtension, 65536 - hash_size / 8);
          std::cerr << "[Warning] Capping outputExtension to " << outputExtension << "\n" ;
        }
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

#ifdef _OPENMP
        int halfCores = std::max(1, 1 + static_cast<int>(std::thread::hardware_concurrency()) / 2);
        omp_set_num_threads(halfCores);
#endif

        auto compressed = compressData(plainData);

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
        hdr.outputExtension = outputExtension;
        std::cout << "Output ext" << outputExtension << std::flush;
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
        uint8_t reverseMap[256 * 256] = {0};
        uint8_t reverseMapOffsets[256] = {0};
        std::bitset<256 * 256> usedIndices;

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
                 auto result = parallelParascatter(
                    blockIndex,             // Current block index
                    thisBlockSize,          // Size of this block
                    block,                  // Block data
                    blockSubkey,            // Subkey for this block
                    nonceSize,              // Nonce size
                    hash_size,              // Hash size in bits
                    seed,                   // Seed
                    algot,                  // Hash algorithm
                    deterministicNonce,     // Deterministic nonce flag
                    outputExtension         // how much output is extended by per block
                );

                // Write the nonce and scatter indices
                fout.write(reinterpret_cast<const char*>(result.chosenNonce.data()), nonceSize);
                fout.write(reinterpret_cast<const char*>(result.scatterIndices.data()), result.scatterIndices.size() * sizeof(uint16_t));

                // Progress Reporting
                std::cerr << "\r[Enc] Mode: parascatter, Block " << (blockIndex + 1)
                          << "/" << totalBlocks << " " << std::flush;
            }           

            // ------------------------------------------------------
            // other modes in else branch
            // ------------------------------------------------------
            else {
                // We'll fill these once a solution is found
                bool found = false;
                std::vector<uint8_t> chosenNonce(nonceSize, 0);
                std::vector<uint16_t> scatterIndices(thisBlockSize, 0);

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

                    // If outputExtension > 0, extend the output
                      std::vector<uint8_t> finalHashOut = hashOut; // Default to OG hashOut
                      if (outputExtension > 0) {
                        // Derive PRK using trial, salt, and seed
                        /*
                        std::vector<uint8_t> prk = derivePRK(
                          trial,              // Input key material: subkey || nonce
                          salt,               // Salt from encryption call
                          keyBuf,             // IKM from encryption call
                          algot,              // Hash algorithm
                          hash_size           // Base hash size
                        );
                        */

                        // Extend the PRK to derive additional bytes (extendedOutput)
                        size_t extendedSize = outputExtension; // Additional bytes required
                        std::vector<uint8_t> extendedOutput = extendOutputKDF(
                          trial,                // PRK derived from trial
                          extendedSize,       // Length of the extension
                          algot,              // Hash algorithm
                          hash_size           // Original hash size
                        );

                        // Combine hashOut and extendedOutput
                        finalHashOut.insert(finalHashOut.end(), extendedOutput.begin(), extendedOutput.end());
                      }                    

                    if (searchModeEnum == 0x00) { // prefix
                        if (finalHashOut.size() >= thisBlockSize &&
                            std::equal(block.begin(), block.end(), finalHashOut.begin())) {
                            scatterIndices.assign(thisBlockSize, 0);
                            found = true;
                        }
                    }
                    else if (searchModeEnum == 0x01) { // sequence
                        for (size_t i = 0; i <= finalHashOut.size() - thisBlockSize; i++) {
                            if (std::equal(block.begin(), block.end(), finalHashOut.begin() + i)) {
                                uint16_t startIdx = static_cast<uint16_t>(i);
                                scatterIndices.assign(thisBlockSize, startIdx);
                                found = true;
                                break;
                            }
                        }
                    }
                    else if (searchModeEnum == 0x02) { // series
                        bool allFound = true;
                        usedIndices.reset();
                        auto it = finalHashOut.begin();

                        for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                            while (it != finalHashOut.end()) {
                                it = std::find(it, finalHashOut.end(), block[byteIdx]);
                                if (it != finalHashOut.end()) {
                                    uint16_t idx = static_cast<uint16_t>(std::distance(finalHashOut.begin(), it));
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
                            if (it == finalHashOut.end()) {
                                allFound = false;
                                break;
                            }
                        }

                        if (allFound) {
                            found = true;
                            if (verbose) {
                                std::cout << "Series Indices: ";
                                for (auto idx : scatterIndices) {
                                    std::cout << static_cast<uint16_t>(idx) << " ";
                                }
                                std::cout << std::endl;
                            }
                        }
                    }
                    else if (searchModeEnum == 0x03) { // scatter
                        bool allFound = true;
                        usedIndices.reset();

                        for (size_t byteIdx = 0; byteIdx < thisBlockSize; byteIdx++) {
                            auto it = finalHashOut.begin();
                            while (it != finalHashOut.end()) {
                                it = std::find(it, finalHashOut.end(), block[byteIdx]);
                                if (it != finalHashOut.end()) {
                                    uint16_t idx = static_cast<uint16_t>(std::distance(finalHashOut.begin(), it));
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
                            if (it == finalHashOut.end()) {
                                allFound = false;
                                break;
                            }
                        }

                        if (allFound) {
                            found = true;
                            if (verbose) {
                                std::cout << "Scatter Indices: ";
                                for (auto idx : scatterIndices) {
                                    std::cout << static_cast<uint16_t>(idx) << " ";
                                }
                                std::cout << std::endl;
                            }
                        }
                    }
                    else if (searchModeEnum == 0x04) { // mapscatter
                        // Reset offsets
                        std::fill(std::begin(reverseMapOffsets), std::end(reverseMapOffsets), 0);

                        // Fill the map with all positions of each byte in finalHashOut
                        for (uint16_t i = 0; i < finalHashOut.size(); i++) {
                            uint8_t b = finalHashOut[i];
                            reverseMap[b * 256 + reverseMapOffsets[b]] = i;
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
                                reverseMap[targetByte * 256 + reverseMapOffsets[targetByte]];
                        }

                        if (allFound) {
                            found = true;
                            if (verbose) {
                                std::cout << "Scatter Indices: ";
                                for (auto idx : scatterIndices) {
                                    std::cout << static_cast<uint16_t>(idx) << " ";
                                }
                                std::cout << std::endl;
                            }
                        }
                    }

                    if (found) {
                        // For non-parallel modes, write nonce and indices
                        fout.write(reinterpret_cast<const char*>(chosenNonce.data()), nonceSize);
                        if (searchModeEnum == 0x02 || searchModeEnum == 0x03 || searchModeEnum == 0x04) {
                            fout.write(reinterpret_cast<const char*>(scatterIndices.data()), scatterIndices.size() * sizeof(uint16_t));

                        }
                        else {
                            // prefix or sequence
                            uint16_t startIdx = scatterIndices[0];
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
        std::cout << "\n[Enc] Block-based puzzle encryption with subkeys complete: " << outFilename << "\n";
    }

  // encrypt/decrypt block mode
    static void puzzleDecryptFileWithHeader(
        const std::string &inFilename,
        const std::string &outFilename,
        const std::string &key
    ) {
        // 1) Open input file & read FileHeader
        std::ifstream fin(inFilename, std::ios::binary);
        if (!fin.is_open()) {
            throw std::runtime_error("Cannot open ciphertext file: " + inFilename);
        }

        // Read unified FileHeader
        FileHeader hdr = readFileHeader(fin);
        if (hdr.magic != MagicNumber) { // "RCRY"
            throw std::runtime_error("Invalid magic number in file header.");
        }

        // Determine cipher mode
        if (hdr.cipherMode != 0x11) { // 0x11 = Block Cipher Mode
            throw std::runtime_error("File is not in Block Cipher mode (expected cipherMode=0x11).");
        }

        // Validate hash algorithm
        HashAlgorithm algot = HashAlgorithm::Unknown;
        if (hdr.hashName == "rainbow") {
            algot = HashAlgorithm::Rainbow;
        }
        else if (hdr.hashName == "rainstorm") {
            algot = HashAlgorithm::Rainstorm;
        }
        else {
            throw std::runtime_error("Unsupported hash algorithm in header: " + hdr.hashName);
        }

        // Read the rest of the file as cipher data
        std::vector<uint8_t> cipherData(
            (std::istreambuf_iterator<char>(fin)),
            (std::istreambuf_iterator<char>())
        );
        fin.close();

        // 2) Derive PRK using KDF
        // Assume 'derivePRK' and 'extendOutputKDF' are defined in tool.h and in scope
        std::vector<uint8_t> ikm(key.begin(), key.end());
        // Convert seed (uint64_t) to vector<uint8_t>
        std::vector<uint8_t> seed_vec(8);
        uint64_t seed = hdr.iv;
        for (size_t i = 0; i < 8; ++i) {
            seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
        }

        std::vector<uint8_t> prk = derivePRK(
            seed_vec,          // Converted seed as vector<uint8_t>
            hdr.salt,              // Provided salt
            ikm,            // IKM (Input Key Material)
            algot,             // Hash algorithm
            hdr.hashSizeBits          // Hash size in bits
        );

        // 3) Extend PRK into subkeys for each block
        size_t totalBlocks = (hdr.originalSize + hdr.blockSize - 1) / hdr.blockSize;
        size_t subkeySize = hdr.hashSizeBits / 8;
        size_t totalNeeded = totalBlocks * subkeySize;
        std::vector<uint8_t> allSubkeys = extendOutputKDF(
            prk,
            totalNeeded,
            algot,
            hdr.hashSizeBits
        );

        // 4) Prepare to reconstruct plaintext
        std::vector<uint8_t> plaintextAccumulated;
        plaintextAccumulated.reserve(hdr.originalSize);

        size_t cipherOffset = 0;
        size_t recoveredSoFar = 0;

        for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
            size_t thisBlockSize = std::min<size_t>(static_cast<size_t>(hdr.blockSize), hdr.originalSize - recoveredSoFar);
            std::vector<uint8_t> block;

            // 4.1) Extract storedNonce
            if (cipherOffset + hdr.nonceSize > cipherData.size()) {
                throw std::runtime_error("Unexpected end of cipher data when reading nonce.");
            }
            std::vector<uint8_t> storedNonce(
                cipherData.begin() + cipherOffset,
                cipherData.begin() + cipherOffset + hdr.nonceSize
            );
            cipherOffset += hdr.nonceSize;

            // 4.2) Read scatterIndices or startIndex based on searchModeEnum
            std::vector<uint16_t> scatterIndices;
            uint16_t startIndex = 0;
            if (hdr.searchModeEnum == 0x02 || hdr.searchModeEnum == 0x03 ||
                hdr.searchModeEnum == 0x04 || hdr.searchModeEnum == 0x05) {
                // Series, Scatter, MapScatter, Parascatter
                size_t scatterDataSize = thisBlockSize * sizeof(uint16_t);
                if (cipherOffset + scatterDataSize > cipherData.size()) {
                    throw std::runtime_error("Unexpected end of cipher data when reading scatter indices.");
                }
                scatterIndices.assign(
                    reinterpret_cast<const uint16_t*>(&cipherData[cipherOffset]),
                    reinterpret_cast<const uint16_t*>(&cipherData[cipherOffset + scatterDataSize])
                );
                cipherOffset += scatterDataSize;
            }
            else {
                // Prefix or Sequence
                if (cipherOffset + 1 > cipherData.size()) {
                    throw std::runtime_error("Unexpected end of cipher data when reading start index.");
                }
                startIndex = cipherData[cipherOffset];
                cipherOffset += 1;
            }

            // 4.3) Derive subkey for this block
            size_t subkeyOffset = blockIndex * subkeySize;
            if (subkeyOffset + subkeySize > allSubkeys.size()) {
                throw std::runtime_error("Subkey index out of range.");
            }
            std::vector<uint8_t> blockSubkey(
                allSubkeys.begin() + subkeyOffset,
                allSubkeys.begin() + subkeyOffset + subkeySize
            );

            // 4.4) Recompute the hash using blockSubkey and storedNonce
            std::vector<uint8_t> trial(blockSubkey);
            trial.insert(trial.end(), storedNonce.begin(), storedNonce.end());

            std::vector<uint8_t> hashOut(hdr.hashSizeBits / 8);
            invokeHash<false>(algot, hdr.iv, trial, hashOut, hdr.hashSizeBits); // Assuming 'hdr.iv' is passed correctly

            // If outputExtension > 0, extend the output
            std::vector<uint8_t> finalHashOut = hashOut; // Default to OG hashOut
            if (hdr.outputExtension > 0) {
              // Derive PRK using trial, salt, and seed
              /*
              std::vector<uint8_t> prk = derivePRK(
                trial,              // Input key material: subkey || nonce
                hdr.salt,           // Salt from file header
                ikm,                // IKM derived from decryption call
                algot,              // Hash algorithm
                hdr.hashSizeBits    // Base hash size
              );
              */

              // Extend the PRK to derive additional bytes (extendedOutput)
              size_t extendedSize = hdr.outputExtension; // Additional bytes from header
              std::vector<uint8_t> extendedOutput = extendOutputKDF(
                trial,                // PRK derived from trial
                extendedSize,       // Length of the extension
                algot,              // Hash algorithm
                hdr.hashSizeBits    // Original hash size
              );

              // Combine hashOut and extendedOutput
              finalHashOut.insert(finalHashOut.end(), extendedOutput.begin(), extendedOutput.end());
            }

            // 4.5) Reconstruct plaintext block based on searchModeEnum
            if (hdr.searchModeEnum == 0x00) { // Prefix
                if (finalHashOut.size() < thisBlockSize/sizeof(uint16_t)) {
                    throw std::runtime_error("[Dec] Hash output smaller than block size for Prefix mode.");
                }
                block.assign(finalHashOut.begin(), finalHashOut.begin() + thisBlockSize);
            }
            else if (hdr.searchModeEnum == 0x01) { // Sequence
                if (startIndex + thisBlockSize/sizeof(uint16_t) > finalHashOut.size()) {
                    throw std::runtime_error("[Dec] Start index out of bounds in Sequence mode.");
                }
                block.assign(finalHashOut.begin() + startIndex, finalHashOut.begin() + startIndex + thisBlockSize);
            }
            else if (hdr.searchModeEnum == 0x02 || hdr.searchModeEnum == 0x03 ||
                     hdr.searchModeEnum == 0x04 || hdr.searchModeEnum == 0x05) { // Series, Scatter, MapScatter, Parascatter
                block.reserve(thisBlockSize);
                for (size_t j = 0; j < thisBlockSize; j++) {
                    uint16_t idx = scatterIndices[j];
                    if (idx >= finalHashOut.size()) {
                        std::cerr << "idx " << idx << " final hash out size " << finalHashOut.size() << std::flush;
                        throw std::runtime_error("[Dec] Scatter index out of range.");
                    }
                    block.push_back(finalHashOut[idx]);
                }
            }
            else {
                throw std::runtime_error("Invalid search mode enum in decryption.");
            }

            // 4.6) Accumulate the reconstructed block
            plaintextAccumulated.insert(
                plaintextAccumulated.end(),
                block.begin(),
                block.end()
            );
            recoveredSoFar += thisBlockSize;

            // 4.7) Progress Reporting
            if (blockIndex % 100 == 0) {
                std::cerr << "\r[Dec] Processing block " << (blockIndex + 1) << " / " << totalBlocks << "..." << std::flush;
            }
        } // end for(blockIndex)

        std::cout << "\n[Dec] Ciphertext blocks decrypted successfully.\n";

        // 5) Decompress the accumulated plaintext
        std::vector<uint8_t> decompressedData = decompressData(plaintextAccumulated);
        if (plaintextAccumulated.size() != hdr.originalSize) {
            throw std::runtime_error("Compressed data size does not match original size.");
        }

        // 6) Write the decompressed plaintext to output file
        std::ofstream fout(outFilename, std::ios::binary);
        if (!fout.is_open()) {
            throw std::runtime_error("Cannot open output file for decompressed plaintext: " + outFilename);
        }

        fout.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
        fout.close();

        std::cout << "[Dec] Decompressed plaintext written to: " << outFilename << "\n";
    }
