

struct ParascatterResult {
    bool found;
    std::vector<uint8_t> chosenNonce;
    std::vector<uint8_t> scatterIndices;
    uint64_t totalTries;
};

ParascatterResult parallelParascatter(
    size_t blockIndex,
    size_t thisBlockSize,
    const std::vector<uint8_t>& block,
    const std::vector<uint8_t>& blockSubkey,
    size_t nonceSize,
    size_t hash_size,
    uint64_t seed,
    HashAlgorithm algot,
    bool verbose,
    bool deterministicNonce,
    size_t progressInterval
) {
    // Initialize result structure
    ParascatterResult result;
    result.found = false;
    result.totalTries = 0;
    result.chosenNonce.resize(nonceSize, 0);
    result.scatterIndices.resize(thisBlockSize, 0);

    // Shared data
    std::atomic<bool> found(false); // Global flag indicating solution found
    std::atomic<uint64_t> tries(0); // Total tries across threads
    std::vector<uint8_t> chosenNonceShared(nonceSize, 0); // Shared solution nonce
    std::vector<uint8_t> scatterIndicesShared(thisBlockSize, 0); // Shared scatter indices

    #pragma omp parallel default(none) \
        shared(block, blockSubkey, found, chosenNonceShared, scatterIndicesShared, tries) \
        firstprivate(nonceSize, hash_size, seed, algot, deterministicNonce, blockIndex, thisBlockSize)
    {
        // Thread-local variables
        std::vector<uint8_t> localNonce(nonceSize, 0);
        std::vector<uint8_t> localScatterIndices(thisBlockSize, 0);
        std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        uint64_t nonceCounter = 0;
        uint64_t localTries = 0;

        // Preallocate 'trial' and 'hashOut' buffers
        std::vector<uint8_t> trial(blockSubkey.size() + nonceSize, 0);
        std::vector<uint8_t> hashOut(hash_size / 8);

        // Copy blockSubkey to the first part of the trial buffer
        std::copy(blockSubkey.begin(), blockSubkey.end(), trial.begin());

        while (!found.load(std::memory_order_acquire)) {
            // Generate a nonce
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
            std::copy(localNonce.begin(), localNonce.end(), trial.begin() + blockSubkey.size());

            // Hash the trial buffer
            invokeHash<false>(algot, seed, trial, hashOut, hash_size);

            // Check for a valid solution
            bool allFound = true;
            std::vector<bool> usedIndices(hashOut.size(), false); // Track used indices

            for (size_t byteIdx = 0; byteIdx < thisBlockSize; ++byteIdx) {
                auto it = std::find(hashOut.begin(), hashOut.end(), block[byteIdx]);
                while (it != hashOut.end()) {
                    size_t idx = std::distance(hashOut.begin(), it);
                    if (!usedIndices[idx]) {
                        localScatterIndices[byteIdx] = static_cast<uint8_t>(idx);
                        usedIndices[idx] = true;
                        break;
                    }
                    it = std::find(it + 1, hashOut.end(), block[byteIdx]); // Search for next occurrence
                }
                if (it == hashOut.end()) {
                    allFound = false; // If we can't match this byte, the solution is invalid
                    break;
                }
            }

            // If a valid solution is found
            if (allFound) {
                if (!found.exchange(true, std::memory_order_acq_rel)) {
                    // Copy the solution to shared data
                    std::copy(localNonce.begin(), localNonce.end(), chosenNonceShared.begin());
                    std::copy(localScatterIndices.begin(), localScatterIndices.end(), scatterIndicesShared.begin());
                }
                break; // Exit loop as this thread has contributed the solution
            }

            // Increment local and shared tries
            ++localTries;
            tries.fetch_add(1, std::memory_order_relaxed);
        } // End of while loop
    } // End of parallel region

    // Populate result with shared data
    result.found = true;
    result.chosenNonce = chosenNonceShared;
    result.scatterIndices = scatterIndicesShared;
    result.totalTries = tries.load(std::memory_order_relaxed);

    return result;
}

