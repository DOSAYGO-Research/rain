#include "parallel-scatter.h"

static std::vector<uint8_t> puzzleEncryptBufferWithHeader(
  const std::vector<uint8_t> &plainData,
  const std::string &key,
  HashAlgorithm algot,
  uint32_t hash_size,
  uint64_t seed,
  const std::vector<uint8_t> &salt,
  size_t blockSize,
  size_t nonceSize,
  const std::string &searchMode,
  bool verbose,
  bool deterministicNonce,
  uint32_t outputExtension
) {
#ifdef _OPENMP
  int halfCores = std::max(1, 1 + static_cast<int>(std::thread::hardware_concurrency()) / 2);
  omp_set_num_threads(halfCores);
#endif

  // 1) Compress plaintext
  //auto compressed = compressData(plainData);
  auto compressed = plainData;

  // 2) Prepare output buffer
  std::vector<uint8_t> outBuffer;
  outBuffer.reserve(compressed.size() + 1024); // Just a guess to reduce re-allocs

  // 3) Determine searchModeEnum
  uint8_t searchModeEnum = 0xFF; // Default for Stream (not used), but we'll set properly
  if (algot == HashAlgorithm::Rainbow || algot == HashAlgorithm::Rainstorm) {
    if (searchMode == "prefix") {
      searchModeEnum = 0x00;
    } else if (searchMode == "sequence") {
      searchModeEnum = 0x01;
    } else if (searchMode == "series") {
      searchModeEnum = 0x02;
    } else if (searchMode == "scatter") {
      searchModeEnum = 0x03;
    } else if (searchMode == "mapscatter") {
      searchModeEnum = 0x04;
    } else if (searchMode == "parascatter") {
      searchModeEnum = 0x05;
    } else {
      throw std::runtime_error("Invalid search mode: " + searchMode);
    }
  }

  // 4) Build FileHeader
  FileHeader hdr{};
  hdr.magic = MagicNumber;
  hdr.version = 0x02;
  hdr.cipherMode = 0x11; // "Block Cipher + puzzle"
  hdr.blockSize = static_cast<uint8_t>(blockSize);
  hdr.nonceSize = static_cast<uint8_t>(nonceSize);
  hdr.outputExtension = outputExtension;
  hdr.hashSizeBits = hash_size;
  hdr.hashName = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
  hdr.iv = seed;
  hdr.saltLen = static_cast<uint8_t>(salt.size());
  hdr.salt = salt;
  hdr.originalSize = compressed.size();
  hdr.searchModeEnum = searchModeEnum;

  // 5) Write the header into outBuffer
  {
    // We'll mimic writeFileHeader but do it directly into outBuffer
    // (You could factor out a writeHeaderToMemory if you prefer.)
    auto writeVec = [&](const void* ptr, size_t len) {
      const uint8_t* p = static_cast<const uint8_t*>(ptr);
      outBuffer.insert(outBuffer.end(), p, p + len);
    };

    writeVec(&hdr.magic, sizeof(hdr.magic));
    writeVec(&hdr.version, sizeof(hdr.version));
    writeVec(&hdr.cipherMode, sizeof(hdr.cipherMode));
    writeVec(&hdr.blockSize, sizeof(hdr.blockSize));
    writeVec(&hdr.nonceSize, sizeof(hdr.nonceSize));
    writeVec(&hdr.hashSizeBits, sizeof(hdr.hashSizeBits));
    writeVec(&hdr.outputExtension, sizeof(hdr.outputExtension));
    uint8_t hnLen = static_cast<uint8_t>(hdr.hashName.size());
    writeVec(&hnLen, sizeof(hnLen));
    if (hnLen > 0) {
      writeVec(hdr.hashName.data(), hnLen);
    }
    writeVec(&hdr.iv, sizeof(hdr.iv));
    writeVec(&hdr.saltLen, sizeof(hdr.saltLen));
    if (hdr.saltLen > 0) {
      writeVec(hdr.salt.data(), hdr.saltLen);
    }
    writeVec(&hdr.searchModeEnum, sizeof(hdr.searchModeEnum));
    writeVec(&hdr.originalSize, sizeof(hdr.originalSize));
    // hmac left zero-initialized, if you use it
    writeVec(hdr.hmac.data(), hdr.hmac.size());
  }

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

  // 8) Setup for puzzle searching
  std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<uint8_t> dist(0, 255);
  uint64_t nonceCounter = 0;
  size_t remaining = hdr.originalSize;
  int progressInterval = 1'000'000;

  // We'll store encryption results in outBuffer after the header
  for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
    size_t thisBlockSize = std::min(blockSize, remaining);
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
          for (size_t i = 0; i + thisBlockSize <= finalHashOut.size(); i++) {
            if (std::equal(block.begin(), block.end(), finalHashOut.begin() + i)) {
              uint16_t startIdx = static_cast<uint16_t>(i);
              scatterIndices.assign(thisBlockSize, startIdx);
              found = true;
              break;
            }
          }
        } else if (searchModeEnum == 0x02) { // series
          bool allFound = true;
          std::bitset<256 * 256> usedIndices;
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
        } else if (searchModeEnum == 0x03) { // scatter
          bool allFound = true;
          std::bitset<256 * 256> usedIndices;
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
            if (!allFound) {
              break;
            }
          }
          if (allFound) {
            found = true;
          }
        } else if (searchModeEnum == 0x04) { // mapscatter
          bool allFound = true;
          static uint8_t reverseMap[256 * 256];
          static uint8_t reverseMapOffsets[256];

          memset(reverseMapOffsets, 0, sizeof(reverseMapOffsets));

          // Build map
          for (uint16_t i = 0; i < finalHashOut.size(); i++) {
            uint8_t b = finalHashOut[i];
            reverseMap[b * 256 + reverseMapOffsets[b]] = i;
            reverseMapOffsets[b]++;
          }

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
  const std::vector<uint8_t> &cipherData,
  const std::string &key
) {
  // We'll parse the FileHeader from the front of cipherData
  // Then reconstruct the plaintext.

  if (cipherData.size() < sizeof(FileHeader) - 32) {
    throw std::runtime_error("Cipher data too small to contain valid header.");
  }

  size_t offset = 0;
  auto readAndAdvance = [&](void* dst, size_t len) {
    if (offset + len > cipherData.size()) {
      throw std::runtime_error("Buffer overrun reading cipher data.");
    }
    std::memcpy(dst, &cipherData[offset], len);
    offset += len;
  };

  FileHeader hdr{};
  // Magic
  readAndAdvance(&hdr.magic, sizeof(hdr.magic));
  // Version
  readAndAdvance(&hdr.version, sizeof(hdr.version));
  // cipherMode
  readAndAdvance(&hdr.cipherMode, sizeof(hdr.cipherMode));
  // blockSize
  readAndAdvance(&hdr.blockSize, sizeof(hdr.blockSize));
  // nonceSize
  readAndAdvance(&hdr.nonceSize, sizeof(hdr.nonceSize));
  // hashSizeBits
  readAndAdvance(&hdr.hashSizeBits, sizeof(hdr.hashSizeBits));
  // outputExtension
  readAndAdvance(&hdr.outputExtension, sizeof(hdr.outputExtension));
  // hashName length
  uint8_t hnLen = 0;
  readAndAdvance(&hnLen, sizeof(hnLen));
  hdr.hashName.resize(hnLen);
  if (hnLen > 0) {
    readAndAdvance(&hdr.hashName[0], hnLen);
  }
  // iv
  readAndAdvance(&hdr.iv, sizeof(hdr.iv));
  // saltLen
  readAndAdvance(&hdr.saltLen, sizeof(hdr.saltLen));
  hdr.salt.resize(hdr.saltLen);
  if (hdr.saltLen > 0) {
    readAndAdvance(hdr.salt.data(), hdr.saltLen);
  }
  // searchModeEnum
  readAndAdvance(&hdr.searchModeEnum, sizeof(hdr.searchModeEnum));
  // originalSize
  readAndAdvance(&hdr.originalSize, sizeof(hdr.originalSize));
  // hmac
  readAndAdvance(hdr.hmac.data(), hdr.hmac.size());

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

  // Remainder is actual block data
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

  // Reconstruct
  std::vector<uint8_t> plaintextAccumulated;
  plaintextAccumulated.reserve(hdr.originalSize);

  for (size_t blockIndex = 0; blockIndex < totalBlocks; blockIndex++) {
    size_t thisBlockSize = std::min<size_t>(hdr.blockSize, hdr.originalSize - plaintextAccumulated.size());

    // Read storedNonce
    if (offset + hdr.nonceSize > cipherData.size()) {
      throw std::runtime_error("Cipher data ended while reading nonce.");
    }
    std::vector<uint8_t> storedNonce(
      cipherData.begin() + offset,
      cipherData.begin() + offset + hdr.nonceSize
    );
    offset += hdr.nonceSize;

    // Read scatterIndices or startIndex
    std::vector<uint16_t> scatterIndices;
    uint16_t startIndex = 0;
    if (hdr.searchModeEnum == 0x02 || hdr.searchModeEnum == 0x03 ||
        hdr.searchModeEnum == 0x04 || hdr.searchModeEnum == 0x05) {
      size_t scatterDataSize = thisBlockSize * sizeof(uint16_t);
      if (offset + scatterDataSize > cipherData.size()) {
        throw std::runtime_error("Cipher data ended while reading scatter indices.");
      }
      scatterIndices.assign(
        reinterpret_cast<const uint16_t*>(&cipherData[offset]),
        reinterpret_cast<const uint16_t*>(&cipherData[offset + scatterDataSize])
      );
      offset += scatterDataSize;
    } else {
      // prefix or sequence
      if (offset + sizeof(uint16_t) > cipherData.size()) {
        throw std::runtime_error("Cipher data ended while reading start index.");
      }
      // We wrote a uint16_t, but for prefix/sequence we might have only written 1 byte,
      // so adapt as needed if your real code only wrote 1 byte. We'll assume full 2 bytes.
      std::memcpy(&startIndex, &cipherData[offset], sizeof(startIndex));
      offset += sizeof(startIndex);
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
  //std::vector<uint8_t> decompressedData = decompressData(plaintextAccumulated);
  std::vector<uint8_t> decompressedData = plaintextAccumulated;
  if (plaintextAccumulated.size() != hdr.originalSize) {
    throw std::runtime_error("Compressed data size mismatch vs. original size header.");
  }

  return decompressedData;
}

static void puzzleEncryptFileWithHeader(
  const std::string &inFilename,
  const std::string &outFilename,
  const std::string &key,
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
  const std::string &key
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

