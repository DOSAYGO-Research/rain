#include "parallel-scatter.h"

/**
 * A helper to print out the puzzle encryption parameters for debugging.
 * This will ensure the logs appear when run under wasm.
 */
static void debugPrintPuzzleParams(
    const std::vector<uint8_t>& plainData,
    std::vector<uint8_t> key,
    HashAlgorithm algot,
    uint32_t hash_size,
    uint64_t seed,
    const std::vector<uint8_t>& salt,
    uint16_t blockSize,
    uint16_t nonceSize,
    const std::string& searchMode,
    bool verbose,
    bool deterministicNonce,
    uint16_t outputExtension)
{
  std::cerr << "[puzzleEncryptBufferWithHeader - DEBUG]\n"
            << "  plainData.size(): " << plainData.size() << "\n"
            << "  key: \"" << key << "\" (length: " << key.size() << ")\n"
            << "  algot: " << ((algot == HashAlgorithm::Rainbow) ? "Rainbow" :
                                (algot == HashAlgorithm::Rainstorm ? "Rainstorm" : "Unknown")) << "\n"
            << "  hash_size (bits): " << hash_size << "\n"
            << "  seed (iv): " << seed << "\n"
            << "  salt.size(): " << salt.size() << "\n"
            << "  blockSize: " << blockSize << "\n"
            << "  nonceSize: " << nonceSize << "\n"
            << "  searchMode: \"" << searchMode << "\"\n"
            << "  verbose: " << (verbose ? "true" : "false") << "\n"
            << "  deterministicNonce: " << (deterministicNonce ? "true" : "false") << "\n"
            << "  outputExtension: " << outputExtension << "\n";

  // Optionally print the first few bytes of salt to confirm
  if (!salt.empty()) {
    std::cerr << "  Salt bytes (up to 16): ";
    for (size_t i = 0; i < salt.size() && i < 16; i++) {
      std::cerr << std::hex << (int)salt[i] << " ";
    }
    std::cerr << std::dec << "\n";
  }
}

static std::vector<uint8_t> puzzleEncryptBufferWithHeader(
  const std::vector<uint8_t> &plainData,
  std::vector<uint8_t> key,
  HashAlgorithm algot,
  uint32_t hash_size,
  uint64_t seed,
  const std::vector<uint8_t> &salt,
  uint16_t blockSize,
  uint16_t nonceSize,
  const std::string &searchMode,
  bool verbose,
  bool deterministicNonce,
  uint16_t outputExtension
) {
#ifdef _OPENMP
  int halfCores = std::max(1, 1 + static_cast<int>(std::thread::hardware_concurrency()) / 2);
  omp_set_num_threads(halfCores);
#endif

  // Uncomment for debugging
  /*
  debugPrintPuzzleParams(
      plainData, key, algot, hash_size, seed,
      salt, blockSize, nonceSize, searchMode,
      verbose, deterministicNonce, outputExtension
  );
  */

  // 1) Compress plaintext
  auto compressed = compressData(plainData);
  //auto compressed = plainData;

  // 3) Prepare FileHeader
  FileHeader hdr{};
  hdr.magic = MagicNumber;
  hdr.version = 0x02;
  hdr.cipherMode = 0x11; // "Block Cipher + puzzle"
  hdr.blockSize = blockSize;
  hdr.nonceSize = nonceSize;
  hdr.outputExtension = outputExtension;
  hdr.hashSizeBits = hash_size;
  hdr.hashName = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
  hdr.iv = seed;
  hdr.saltLen = static_cast<uint8_t>(salt.size());
  hdr.salt = salt;
  hdr.originalSize = compressed.size();

  // Determine searchModeEnum
  if (algot == HashAlgorithm::Rainbow || algot == HashAlgorithm::Rainstorm) {
    if (searchMode == "prefix") {
      hdr.searchModeEnum = 0x00;
    } else if (searchMode == "sequence") {
      hdr.searchModeEnum = 0x01;
    } else if (searchMode == "series") {
      hdr.searchModeEnum = 0x02;
    } else if (searchMode == "scatter") {
      hdr.searchModeEnum = 0x03;
    } else if (searchMode == "mapscatter") {
      hdr.searchModeEnum = 0x04;
    } else if (searchMode == "parascatter") {
      hdr.searchModeEnum = 0x05;
    } else {
      throw std::runtime_error("Invalid search mode: " + searchMode);
    }
  } else {
    hdr.searchModeEnum = 0xFF; // Default or undefined
  }

  auto searchModeEnum = hdr.searchModeEnum;

  // 4) Serialize FileHeader using file-header.h's serializeFileHeader
  std::vector<uint8_t> headerData = serializeFileHeader(hdr);

  // 6) Derive PRK
  std::vector<uint8_t> keyBuf(key.begin(), key.end());
  std::vector<uint8_t> seed_vec(8);
  for (size_t i = 0; i < 8; ++i) {
    seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
  }
  std::vector<uint8_t> prk = derivePRK(seed_vec, salt, keyBuf, algot, hash_size);

  // 7) Extend PRK into subkeys
  size_t totalBlocks = (hdr.originalSize + blockSize - 1) / blockSize;
  size_t subkeySize = hdr.hashSizeBits / 8;
  size_t totalNeeded = totalBlocks * subkeySize;
  std::vector<uint8_t> allSubkeys = extendOutputKDF(prk, totalNeeded, algot, hdr.hashSizeBits);

  // 5) Concatenate header and encrypted data
  std::vector<uint8_t> outBuffer;
  outBuffer.reserve(headerData.size() + totalBlocks* (nonceSize + subkeySize));
  outBuffer.insert(outBuffer.end(), headerData.begin(), headerData.end());

  // 8) Setup for puzzle searching
  std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<uint8_t> dist(0, 255);
  uint64_t nonceCounter = 0;
  size_t remaining = hdr.originalSize;
  int progressInterval = 1'000'000;
  static uint16_t reverseMap[256 * 65536];
  static uint16_t reverseMapOffsets[256];
  std::bitset<65536> usedIndices;

  // We'll store encryption results in outBuffer after the header
  for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
    size_t thisBlockSize = std::min<size_t>(blockSize, remaining);
    remaining -= thisBlockSize;

    // Extract block
    std::vector<uint8_t> block(
      compressed.begin() + blockIndex * blockSize,
      compressed.begin() + blockIndex * blockSize + thisBlockSize
    );

    // Extract subkey
    size_t offset = blockIndex * subkeySize;
    if (offset + subkeySize > allSubkeys.size()) {
      throw std::runtime_error("Subkey index out of range.");
    }
    std::vector<uint8_t> blockSubkey(
      allSubkeys.begin() + offset,
      allSubkeys.begin() + offset + subkeySize
    );

    // If parascatter
    if (searchModeEnum == 0x05) {
      auto result = parallelParascatter(
        blockIndex,
        thisBlockSize,
        block,
        blockSubkey,
        nonceSize,
        hash_size,
        seed,
        algot,
        deterministicNonce,
        outputExtension
      );
      // Write nonce
      outBuffer.insert(outBuffer.end(), result.chosenNonce.begin(), result.chosenNonce.end());
      // Write scatter indices
      {
        const uint8_t* si = reinterpret_cast<const uint8_t*>(result.scatterIndices.data());
        outBuffer.insert(outBuffer.end(), si, si + result.scatterIndices.size() * sizeof(uint16_t));
      }
      if (verbose) {
        std::cerr << "\r[Enc] Mode: parascatter, Block " << (blockIndex + 1) << "/" << totalBlocks << " ";
      }
    } else {
      // Other modes
      bool found = false;
      std::vector<uint8_t> chosenNonce(nonceSize, 0);
      std::vector<uint16_t> scatterIndices(thisBlockSize, 0);

      for (uint64_t tries = 0; ; tries++) {
        // Generate nonce
        if (deterministicNonce) {
          for (size_t i = 0; i < nonceSize; i++) {
            chosenNonce[i] = static_cast<uint8_t>((nonceCounter >> (i * 8)) & 0xFF);
          }
          nonceCounter++;
        } else {
          for (size_t i = 0; i < nonceSize; i++) {
            chosenNonce[i] = dist(rng);
          }
        }

        // Build trial
        std::vector<uint8_t> trial(blockSubkey);
        trial.insert(trial.end(), chosenNonce.begin(), chosenNonce.end());

        // Hash it
        std::vector<uint8_t> hashOut(hash_size / 8);
        invokeHash<false>(algot, seed, trial, hashOut, hash_size);

        // Possibly extend output
        std::vector<uint8_t> finalHashOut = hashOut;
        if (outputExtension > 0) {
          // For demonstration, use the subkey+nonce as the "PRK" input
          std::vector<uint8_t> extendedOutput = extendOutputKDF(trial, outputExtension, algot, hash_size);
          finalHashOut.insert(finalHashOut.end(), extendedOutput.begin(), extendedOutput.end());
        }

        // Check the search mode
        if (searchModeEnum == 0x00) { // prefix
          if (finalHashOut.size() >= thisBlockSize &&
              std::equal(block.begin(), block.end(), finalHashOut.begin())) {
            scatterIndices.assign(thisBlockSize, 0);
            found = true;
          }
        } else if (searchModeEnum == 0x01) { // sequence
          for (size_t i = 0; i <= finalHashOut.size() - thisBlockSize; i++) {
            if (std::equal(block.begin(), block.end(), finalHashOut.begin() + i)) {
              uint16_t startIdx = static_cast<uint16_t>(i);
              scatterIndices.assign(thisBlockSize, startIdx);
              found = true;
              break;
            }
          }
        } else if (searchModeEnum == 0x02) { // series
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
        } else if (searchModeEnum == 0x03) { // scatter
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
              } else {
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
          }
        } else if (searchModeEnum == 0x04) { // mapscatter
          // Reset offsets
          std::fill(std::begin(reverseMapOffsets), std::end(reverseMapOffsets), 0);

          // Fill the map with all positions of each byte in finalHashOut
          for (uint16_t i = 0; i < finalHashOut.size(); i++) {
              uint8_t b = finalHashOut[i];
              reverseMap[b * 65536 + reverseMapOffsets[b]] = i;
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
                  reverseMap[targetByte * 65536 + reverseMapOffsets[targetByte]];
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
          // Write nonce
          outBuffer.insert(outBuffer.end(), chosenNonce.begin(), chosenNonce.end());
          // Write indices
          if (searchModeEnum == 0x02 || searchModeEnum == 0x03 ||
              searchModeEnum == 0x04) {
            const uint8_t* si = reinterpret_cast<const uint8_t*>(scatterIndices.data());
            outBuffer.insert(outBuffer.end(), si, si + scatterIndices.size() * sizeof(uint16_t));
          } else if (searchModeEnum == 0x00 || searchModeEnum == 0x01) {
            uint16_t startIdx = scatterIndices[0];
            const uint8_t* idxPtr = reinterpret_cast<const uint8_t*>(&startIdx);
            outBuffer.insert(outBuffer.end(), idxPtr, idxPtr + sizeof(startIdx));
          }
          if (verbose) {
            std::cerr << "\r[Enc] Block " << blockIndex + 1 << "/" << totalBlocks << " found pattern.\n";
          }
          break;
        }

        if (tries % progressInterval == 0 && verbose) {
          std::cerr << "\r[Enc] Mode: " << searchMode
                    << ", Block " << (blockIndex + 1) << "/" << totalBlocks
                    << ", " << tries << " tries..." << std::flush;
        }
      }
    }
  }

  // Return the fully built buffer
  return outBuffer;
}

static std::vector<uint8_t> puzzleDecryptBufferWithHeader(
  const std::vector<uint8_t> &cipherText,
  std::vector<uint8_t> key
) {
  // Parse the FileHeader from the front of cipherData
  // Then reconstruct the plaintext.

  if (cipherText.size() < sizeof(PackedHeader)) {
    throw std::runtime_error("Cipher data too small to contain valid header.");
  }

  std::istringstream inStream(std::string(cipherText.begin(), cipherText.end()), std::ios::binary);

  // Read the header
  FileHeader hdr = readFileHeader(inStream);

  if (hdr.magic != MagicNumber) {
    throw std::runtime_error("Invalid magic number.");
  }
  if (hdr.cipherMode != 0x11) {
    throw std::runtime_error("Not block cipher mode (expected 0x11).");
  }

  // Determine HashAlgorithm
  HashAlgorithm algot = HashAlgorithm::Unknown;
  if (hdr.hashName == "rainbow") {
    algot = HashAlgorithm::Rainbow;
  } else if (hdr.hashName == "rainstorm") {
    algot = HashAlgorithm::Rainstorm;
  } else {
    throw std::runtime_error("Unsupported hash algorithm: " + hdr.hashName);
  }

  // Derive PRK
  std::vector<uint8_t> ikm(key.begin(), key.end());
  std::vector<uint8_t> seed_vec(8);
  for (size_t i = 0; i < 8; ++i) {
    seed_vec[i] = static_cast<uint8_t>((hdr.iv >> (i * 8)) & 0xFF);
  }
  std::vector<uint8_t> prk = derivePRK(seed_vec, hdr.salt, ikm, algot, hdr.hashSizeBits);

  // Extend into subkeys
  size_t totalBlocks = (hdr.originalSize + hdr.blockSize - 1) / hdr.blockSize;
  size_t subkeySize = hdr.hashSizeBits / 8;
  size_t totalNeeded = totalBlocks * subkeySize;
  std::vector<uint8_t> allSubkeys = extendOutputKDF(prk, totalNeeded, algot, hdr.hashSizeBits);

  // Reconstruct plaintext
  std::vector<uint8_t> plaintextAccumulated;
  plaintextAccumulated.reserve(hdr.originalSize);

  for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
    size_t thisBlockSize = std::min<size_t>(hdr.blockSize, hdr.originalSize - plaintextAccumulated.size());

    // Read storedNonce directly from inStream
    std::vector<uint8_t> storedNonce(hdr.nonceSize);
    inStream.read(reinterpret_cast<char*>(storedNonce.data()), hdr.nonceSize);
    if (inStream.gcount() != hdr.nonceSize) {
      throw std::runtime_error("Cipher data ended while reading nonce.");
    }

    // Read scatterIndices or startIndex
    std::vector<uint16_t> scatterIndices;
    uint16_t startIndex = 0;
    if (hdr.searchModeEnum == 0x02 || hdr.searchModeEnum == 0x03 ||
        hdr.searchModeEnum == 0x04 || hdr.searchModeEnum == 0x05) {
      size_t scatterDataSize = thisBlockSize * sizeof(uint16_t);
      scatterIndices.resize(thisBlockSize);
      inStream.read(reinterpret_cast<char*>(scatterIndices.data()), scatterDataSize);
      if (inStream.gcount() != scatterDataSize) {
        throw std::runtime_error("Cipher data ended while reading scatter indices.");
      }
    } else {
      // prefix or sequence
      inStream.read(reinterpret_cast<char*>(&startIndex), sizeof(startIndex));
      if (inStream.gcount() != sizeof(startIndex)) {
        throw std::runtime_error("Cipher data ended while reading start index.");
      }
    }

    // Subkey
    size_t subkeyOffset = blockIndex * subkeySize;
    if (subkeyOffset + subkeySize > allSubkeys.size()) {
      throw std::runtime_error("Subkey index out of range in decryption.");
    }
    std::vector<uint8_t> blockSubkey(
      allSubkeys.begin() + subkeyOffset,
      allSubkeys.begin() + subkeyOffset + subkeySize
    );

    // Recompute hash
    std::vector<uint8_t> trial(blockSubkey);
    trial.insert(trial.end(), storedNonce.begin(), storedNonce.end());

    std::vector<uint8_t> hashOut(hdr.hashSizeBits / 8);
    invokeHash<false>(algot, hdr.iv, trial, hashOut, hdr.hashSizeBits);

    std::vector<uint8_t> finalHashOut = hashOut;
    if (hdr.outputExtension > 0) {
      std::vector<uint8_t> extended = extendOutputKDF(trial, hdr.outputExtension, algot, hdr.hashSizeBits);
      finalHashOut.insert(finalHashOut.end(), extended.begin(), extended.end());
    }

    // Reconstruct block
    std::vector<uint8_t> block;
    block.reserve(thisBlockSize);

    if (hdr.searchModeEnum == 0x00) { // prefix
      if (finalHashOut.size() < thisBlockSize) {
        throw std::runtime_error("Hash output smaller than block size in prefix mode.");
      }
      block.assign(finalHashOut.begin(), finalHashOut.begin() + thisBlockSize);
    } else if (hdr.searchModeEnum == 0x01) { // sequence
      if (startIndex + thisBlockSize > finalHashOut.size()) {
        throw std::runtime_error("Start index out of bounds in sequence mode.");
      }
      block.assign(finalHashOut.begin() + startIndex,
                   finalHashOut.begin() + startIndex + thisBlockSize);
    } else if (hdr.searchModeEnum == 0x02 || hdr.searchModeEnum == 0x03 ||
               hdr.searchModeEnum == 0x04 || hdr.searchModeEnum == 0x05) {
      for (size_t j = 0; j < thisBlockSize; j++) {
        uint16_t idx = scatterIndices[j];
        if (idx >= finalHashOut.size()) {
          throw std::runtime_error("Scatter index out of range in finalHashOut.");
        }
        block.push_back(finalHashOut[idx]);
      }
    } else {
      throw std::runtime_error("Invalid searchModeEnum in decryption.");
    }

    plaintextAccumulated.insert(plaintextAccumulated.end(), block.begin(), block.end());

    if (blockIndex % 100 == 0) {
      std::cerr << "\r[Dec] Processing block " << (blockIndex + 1) << "/" << totalBlocks << "...";
    }
  }

  // Done reading, now decompress
  std::vector<uint8_t> decompressedData = decompressData(plaintextAccumulated);
  if (plaintextAccumulated.size() != hdr.originalSize) {
    throw std::runtime_error("Compressed data size mismatch vs. original size header.");
  }

  return decompressedData;
}

static void puzzleEncryptFileWithHeader(
  const std::string &inFilename,
  const std::string &outFilename,
  std::vector<uint8_t> key,
  HashAlgorithm algot,
  uint32_t hash_size,
  uint64_t seed,
  std::vector<uint8_t> salt,
  size_t blockSize,
  size_t nonceSize,
  const std::string &searchMode,
  bool verbose,
  bool deterministicNonce,
  uint32_t outputExtension
) {
  // 1) Read & compress plaintext from file
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("Cannot open input file: " + inFilename);
  }
  std::vector<uint8_t> plainData(
    (std::istreambuf_iterator<char>(fin)),
    (std::istreambuf_iterator<char>())
  );
  fin.close();

  // 2) Call the new buffer-based API
  std::vector<uint8_t> encrypted = puzzleEncryptBufferWithHeader(
    plainData,
    key,
    algot,
    hash_size,
    seed,
    salt,
    blockSize,
    nonceSize,
    searchMode,
    verbose,
    deterministicNonce,
    outputExtension
  );

  // 3) Write the resulting ciphertext to file
  std::ofstream fout(outFilename, std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("Cannot open output file: " + outFilename);
  }
  fout.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
  fout.close();

  std::cout << "\n[Enc] Block-based puzzle encryption with subkeys complete: " << outFilename << "\n";
}

static void puzzleDecryptFileWithHeader(
  const std::string &inFilename,
  const std::string &outFilename,
  std::vector<uint8_t> key
) {
  // 1) Read the ciphertext file
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("Cannot open ciphertext file: " + inFilename);
  }
  std::vector<uint8_t> cipherData(
    (std::istreambuf_iterator<char>(fin)),
    (std::istreambuf_iterator<char>())
  );
  fin.close();

  // 2) Call the new buffer-based API
  std::vector<uint8_t> decompressedData = puzzleDecryptBufferWithHeader(
    cipherData,
    key
  );

  // 3) Write the decompressed plaintext to file
  std::ofstream fout(outFilename, std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("Cannot open output file for plaintext: " + outFilename);
  }
  fout.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
  fout.close();

  std::cout << "[Dec] Decompressed plaintext written to: " << outFilename << "\n";
}

