// Result structure:
struct ParascatterResult {
  bool found;
  std::vector<uint8_t> chosenNonce;
  std::vector<uint16_t> scatterIndices;
};

// Parallel scatter function:
ParascatterResult parallelParascatter(
    size_t blockIndex,
    size_t thisBlockSize,
    const std::vector<uint8_t>& block,
    const std::vector<uint8_t>& blockSubkey,
    size_t nonceSize,
    size_t hash_size,
    uint64_t seed,
    HashAlgorithm algot,
    bool deterministicNonce,
    uint32_t outputExtension
) {
  // 1) Prepare the result
  ParascatterResult result;
  result.found = false;
  result.chosenNonce.resize(nonceSize, 0);
  result.scatterIndices.resize(thisBlockSize, 0);

  // 2) Shared flag + shared final solution
  std::atomic<bool> found(false);
  std::vector<uint8_t> chosenNonceShared(nonceSize);
  std::vector<uint16_t> scatterIndicesShared(thisBlockSize);

  // 3) Launch parallel region
  #pragma omp parallel default(none) \
    shared(block, blockSubkey, found, chosenNonceShared, scatterIndicesShared) \
    firstprivate(nonceSize, hash_size, seed, algot, deterministicNonce, blockIndex, thisBlockSize, outputExtension)
  {
    // Each thread's preallocated buffers
    std::vector<uint8_t> localNonce(nonceSize);
    std::vector<uint16_t> localScatterIndices(thisBlockSize);

    // RNG
    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    uint64_t nonceCounter = 0;

    // Preallocate 'trial' and 'hashOut' buffers
    std::vector<uint8_t> trial(blockSubkey.size() + nonceSize);
    std::copy(blockSubkey.begin(), blockSubkey.end(), trial.begin()); // Copy subkey into trial

    uint8_t resetFlag = 1;
    std::vector<uint8_t> hashOut(hash_size / 8);
    std::vector<uint8_t> extendedOutput(outputExtension);
    std::array<uint8_t, 65536> usedIndices = {};
    std::vector<uint8_t> finalHashOut;

    // 4) Main loop
    while (!found.load(std::memory_order_acquire)) {
      if (resetFlag == std::numeric_limits<uint8_t>::max()) {
        std::fill(usedIndices.begin(), usedIndices.end(), 0);
        resetFlag = 1;
      } else {
        ++resetFlag;
      }

      // Generate nonce
      if (deterministicNonce) {
        for (size_t i = 0; i < nonceSize; ++i) {
          localNonce[i] = static_cast<uint8_t>((nonceCounter >> (i * 8)) & 0xFF);
        }
        ++nonceCounter;
      } else {
        for (size_t i = 0; i < nonceSize; ++i) {
          localNonce[i] = dist(rng);
        }
      }

      // Build trial buffer
      std::copy(localNonce.begin(), localNonce.end(),
                trial.begin() + blockSubkey.size());

      // Hash it
      invokeHash<false>(algot, seed, trial, hashOut, hash_size);
      finalHashOut = hashOut;

      // Extend hashOut if outputExtension > 0
      if (outputExtension > 0) {
        /*
        std::vector<uint8_t> prk = derivePRK(
          trial,       // Input key material: subkey || nonce
          salt,        // Salt
          blockSubkey, // IKM (shared subkey as an example)
          algot,       // Hash algorithm
          hash_size    // Hash size
        );
        */

        extendedOutput = extendOutputKDF(
          trial,             // PRK derived from trial
          outputExtension, // Additional length
          algot,           // Hash algorithm
          hash_size        // Original hash size
        );

        finalHashOut.insert(finalHashOut.end(), extendedOutput.begin(), extendedOutput.end());
      }

      // Attempt scatter match
      bool allFound = true;

      for (size_t byteIdx = 0; byteIdx < thisBlockSize; ++byteIdx) {
        uint8_t target = block[byteIdx];
        auto it = std::find(finalHashOut.begin(), finalHashOut.end(), target);
        while (it != finalHashOut.end()) {
          size_t idx = static_cast<size_t>(std::distance(finalHashOut.begin(), it));
          if (usedIndices[idx] != resetFlag) {
            usedIndices[idx] = resetFlag;
            localScatterIndices[byteIdx] = static_cast<uint16_t>(idx);
            break;
          }
          it = std::find(std::next(it), finalHashOut.end(), target);
        }
        if (it == finalHashOut.end()) {
          allFound = false;
          break;
        }
      }

      // If success, mark found & copy results
      if (allFound) {
        bool expected = false;
        if (found.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
          chosenNonceShared = localNonce;
          scatterIndicesShared = localScatterIndices;
        }
        break; // This thread can stop
      }
    } // end while
  } // end parallel region

  // 5) Fill final result
  result.found = true; // By now a solution was found if the loop ended
  result.chosenNonce = chosenNonceShared;
  result.scatterIndices = scatterIndicesShared;

  return result;
}

