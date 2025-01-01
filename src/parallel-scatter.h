// Result structure:
struct ParascatterResult {
  bool found;
  std::vector<uint8_t> chosenNonce;
  std::vector<uint8_t> scatterIndices;
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
    bool deterministicNonce
) {
  // 1) Prepare the result
  ParascatterResult result;
  result.found = false;
  result.chosenNonce.resize(nonceSize, 0);
  result.scatterIndices.resize(thisBlockSize, 0);

  // 2) Shared flag + shared final solution
  std::atomic<bool> found(false);
  std::vector<uint8_t> chosenNonceShared(nonceSize);
  std::vector<uint8_t> scatterIndicesShared(thisBlockSize);

  // 3) Launch parallel region
  #pragma omp parallel default(none) \
    shared(block, blockSubkey, found, chosenNonceShared, scatterIndicesShared) \
    firstprivate(nonceSize, hash_size, seed, algot, deterministicNonce, blockIndex, thisBlockSize)
  {
    // Each thread's preallocated buffers
    std::vector<uint8_t> localNonce(nonceSize);
    std::vector<uint8_t> localScatterIndices(thisBlockSize);

    // RNG
    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    uint64_t nonceCounter = 0;

    // Preallocate 'trial' and 'hashOut' buffers
    std::vector<uint8_t> trial(blockSubkey.size() + nonceSize);
    std::copy(blockSubkey.begin(), blockSubkey.end(), trial.begin()); // Copy subkey into trial

    std::vector<uint8_t> hashOut(hash_size / 8);

    // 4) Main loop
    while (!found.load(std::memory_order_acquire)) {
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

      // Attempt scatter match
      bool allFound = true;
      std::vector<bool> usedIndices(hashOut.size(), false);

      for (size_t byteIdx = 0; byteIdx < thisBlockSize; ++byteIdx) {
        uint8_t target = block[byteIdx];
        auto it = std::find(hashOut.begin(), hashOut.end(), target);
        while (it != hashOut.end()) {
          size_t idx = static_cast<size_t>(std::distance(hashOut.begin(), it));
          if (!usedIndices[idx]) {
            usedIndices[idx] = true;
            localScatterIndices[byteIdx] = static_cast<uint8_t>(idx);
            break;
          }
          it = std::find(std::next(it), hashOut.end(), target);
        }
        if (it == hashOut.end()) {
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

