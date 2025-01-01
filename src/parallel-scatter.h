

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
    std::atomic<bool> found(false);
    std::atomic<uint64_t> tries(0);
    std::vector<uint8_t> chosenNonceShared(nonceSize, 0);
    std::vector<uint8_t> scatterIndicesShared(thisBlockSize, 0);

    // Mutex for console output
    static std::mutex consoleMutex;
    static std::mutex stateMutex;

    // Parallel region
    #pragma omp parallel default(none) \
        shared(block, blockSubkey, found, chosenNonceShared, scatterIndicesShared, tries, consoleMutex, std::cout, std::cerr) \
        firstprivate(nonceSize, hash_size, seed, algot, verbose, deterministicNonce, progressInterval, blockIndex, thisBlockSize)
    {
        // Thread-local variables
        std::vector<uint8_t> localNonce(nonceSize);
        std::vector<uint8_t> localScatterIndices(thisBlockSize);
        std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        uint64_t nonceCounter = 0;
        uint64_t localTries = 0;

        // Preallocate 'trial' and 'hashOut' vectors
        std::vector<uint8_t> trial;
        trial.reserve(blockSubkey.size() + nonceSize);
        trial.assign(blockSubkey.begin(), blockSubkey.end());
        trial.resize(blockSubkey.size() + nonceSize); // Ensure exact size

        std::vector<uint8_t> hashOut(hash_size / 8);

        // Get thread number for optional use
#ifdef _OPENMP
        int thread_num = omp_get_thread_num();
#else        
        int thread_num = 1;
#endif

        while (!found.load(std::memory_order_acquire)) {
            // Generate nonce
            if (deterministicNonce) {
                for (size_t i = 0; i < nonceSize; ++i) {
                    localNonce[i] = static_cast<uint8_t>((nonceCounter >> (i * 8)) & 0xFF);
                }
                nonceCounter++;
            }
            else {
                for (size_t i = 0; i < nonceSize; ++i) {
                    localNonce[i] = dist(rng);
                }
            }

            // Build trial buffer by reusing the 'trial' vector
            std::copy(localNonce.begin(), localNonce.end(), trial.begin() + blockSubkey.size());

            // Hash it
            invokeHash<false>(algot, seed, trial, hashOut, hash_size);

            bool allFound = true;
            std::bitset<64> usedIndices;
            usedIndices.reset();

            for (size_t byteIdx = 0; byteIdx < thisBlockSize; ++byteIdx) {
                auto it = hashOut.begin();
                while (it != hashOut.end()) {
                    it = std::find(it, hashOut.end(), block[byteIdx]);
                    if (it != hashOut.end()) {
                        uint8_t idx = static_cast<uint8_t>(std::distance(hashOut.begin(), it));
                        if (idx >= hashOut.size()) { // Safety check
                            allFound = false;
                            break;
                        }
                        if (!usedIndices.test(idx)) {
                            localScatterIndices[byteIdx] = idx;
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
                #pragma omp critical
                {
                    if (!found.exchange(true, std::memory_order_acq_rel)) {
                        chosenNonceShared = localNonce;
                        scatterIndicesShared = localScatterIndices;
                    }
                }
                break;
            }

            // Increment local and shared tries
            localTries++;
            tries.fetch_add(1, std::memory_order_relaxed);
        } // End of while loop

        // Optionally, threads can perform final updates or cleanups here
    } // End of parallel region

    // Populate the result structure
    result.found = true; // Since parallelParascatter exits only when found is true
    result.chosenNonce = chosenNonceShared;
    result.scatterIndices = scatterIndicesShared;
    result.totalTries = tries.load(std::memory_order_relaxed);

    return result;
}

