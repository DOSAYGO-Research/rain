// wasm_exports.cpp

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>   // for malloc/free
#include <cstring>   // for memcpy
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include "../tool.h"         // for invokeHash, etc.
#include "../file-header.h"  // for FileHeader serialization/deserialization
#include "../block-cipher.h"
#include "../stream-cipher.h"

extern "C" {

// -------------------------------------------------------------------
// Hash Functions
// -------------------------------------------------------------------
KEEPALIVE void rainstormHash64(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm::rainstorm<64, false>(in, len, seed, out);
}

KEEPALIVE void rainstormHash128(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm::rainstorm<128, false>(in, len, seed, out);
}

KEEPALIVE void rainstormHash256(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm::rainstorm<256, false>(in, len, seed, out);
}

KEEPALIVE void rainstormHash512(const void* in, const size_t len, const seed_t seed, void* out) {
    rainstorm::rainstorm<512, false>(in, len, seed, out);
}

KEEPALIVE void rainbowHash64(const void* in, const size_t len, const seed_t seed, void* out) {
    rainbow::rainbow<64, false>(in, len, seed, out);
}

KEEPALIVE void rainbowHash128(const void* in, const size_t len, const seed_t seed, void* out) {
    rainbow::rainbow<128, false>(in, len, seed, out);
}

KEEPALIVE void rainbowHash256(const void* in, const size_t len, const seed_t seed, void* out) {
    rainbow::rainbow<256, false>(in, len, seed, out);
}

// -------------------------------------------------------------------
// WASM Memory Management Utilities
// -------------------------------------------------------------------

/**
 * Utility function to serialize a buffer into a WASM-allocated memory block.
 * Returns the pointer to the serialized buffer.
 */
uint8_t* serializeBuffer(const std::vector<uint8_t>& buffer, size_t& outSize) {
    outSize = buffer.size();
    uint8_t* wasmBuffer = (uint8_t*)malloc(outSize);
    if (!wasmBuffer) {
        throw std::runtime_error("Memory allocation failed in serializeBuffer.");
    }
    std::memcpy(wasmBuffer, buffer.data(), outSize);
    return wasmBuffer;
}

/**
 * Utility function to deserialize a buffer from WASM memory.
 * Returns a std::vector<uint8_t> containing the data.
 */
std::vector<uint8_t> deserializeBuffer(const uint8_t* data, size_t size) {
    return std::vector<uint8_t>(data, data + size);
}

// -------------------------------------------------------------------
// WASM Export: wasmGetFileHeaderInfo
// Description: Parse a file header from a buffer in WASM memory and return a JSON-like string.
// -------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
char* wasmGetFileHeaderInfo(const uint8_t* data, size_t size) {
    try {
        // Deserialize input buffer
        std::vector<uint8_t> buffer(data, data + size);

        // Parse the header using readFileHeader
        std::istringstream inStream(std::string(buffer.begin(), buffer.end()), std::ios::binary);
        FileHeader hdr = readFileHeader(inStream);

        // Construct JSON-like string
        std::stringstream ss;
        ss << "{";
        ss << "\"magic\":\"0x" << std::hex << hdr.magic << "\",";
        ss << "\"version\":" << std::dec << static_cast<int>(hdr.version) << ",";
        ss << "\"cipherMode\":\"0x" << std::hex << static_cast<int>(hdr.cipherMode) << "\",";
        ss << "\"blockSize\":" << std::dec << hdr.blockSize << ",";
        ss << "\"nonceSize\":" << hdr.nonceSize << ",";
        ss << "\"hashSizeBits\":" << hdr.hashSizeBits << ",";
        ss << "\"outputExtension\":" << hdr.outputExtension << ",";
        ss << "\"hashName\":\"" << hdr.hashName << "\",";
        ss << "\"iv\":\"0x" << std::hex << hdr.iv << "\",";
        ss << "\"saltLen\":" << std::dec << static_cast<int>(hdr.saltLen) << ",";
        ss << "\"salt\":\"";
        for (auto b : hdr.salt) {
            ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(b);
        }
        ss << "\",";
        ss << "\"searchModeEnum\":\"0x" << std::hex << static_cast<int>(hdr.searchModeEnum) << "\",";
        ss << "\"originalSize\":" << std::dec << hdr.originalSize << ",";
        ss << "\"hmac\":\"";
        for (auto b : hdr.hmac) {
            ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(b);
        }
        ss << "\"";
        ss << "}";

        std::string result = ss.str();

        // Allocate memory for the output string
        char* output = (char*)malloc(result.size() + 1);
        if (!output) {
            throw std::runtime_error("Memory allocation failed in wasmGetFileHeaderInfo.");
        }
        std::memcpy(output, result.c_str(), result.size() + 1);
        return output;
    }
    catch (const std::exception &e) {
        // Return an error string. Caller must free this memory.
        std::string err = "{\"error\":\"" + std::string(e.what()) + "\"}";
        char* output = (char*)malloc(err.size() + 1);
        if (!output) {
            return nullptr; // Allocation failed
        }
        std::memcpy(output, err.c_str(), err.size() + 1);
        return output;
    }
}

/**
 * Utility function so JS can free the char* or buffers returned by
 * wasmGetFileHeaderInfo (or any other WASM-allocated pointers).
 */
EMSCRIPTEN_KEEPALIVE
void wasmFree(void* ptr) {
    free(ptr);
}

/**
 * Utility function to free a string allocated by WASM.
 */
EMSCRIPTEN_KEEPALIVE
void wasmFreeString(char* strPtr) {
    free(strPtr);
}

/**
 * Utility function to free a buffer allocated by WASM.
 */
EMSCRIPTEN_KEEPALIVE
void wasmFreeBuffer(uint8_t* bufferPtr) {
    free(bufferPtr);
}

// -------------------------------------------------------------------
// WASM Export: wasmStreamEncryptBuffer
// Description: Buffer-based stream encryption for WASM.
// -------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void wasmStreamEncryptBuffer(
    const uint8_t* inBufferPtr,
    size_t inBufferSize,
    const char* keyPtr,
    size_t keyLength,
    const char* algorithmPtr,
    size_t algorithmLength,
    uint32_t hashBits,
    uint64_t seed,
    const uint8_t* saltPtr,
    size_t saltLen,
    uint32_t outputExtension,
    int verbose,
    uint8_t** outBufferPtr,
    size_t* outBufferSizePtr
) {
    try {
        // Deserialize input buffer
        std::vector<uint8_t> plainData = deserializeBuffer(inBufferPtr, inBufferSize);

        // Convert C strings to std::string
        std::vector<uint8_t> key(keyPtr, keyPtr + keyLength);
        std::string algorithm(algorithmPtr, algorithmLength);

        // Convert salt to std::vector<uint8_t>
        std::vector<uint8_t> salt(saltPtr, saltPtr + saltLen);

        // Determine HashAlgorithm enum
        HashAlgorithm algot;
        if (algorithm == "rainbow") {
            algot = HashAlgorithm::Rainbow;
        }
        else if (algorithm == "rainstorm") {
            algot = HashAlgorithm::Rainstorm;
        }
        else {
            throw std::runtime_error("Unsupported algorithm: " + algorithm);
        }

        // Call the buffer-based encryption function
        std::vector<uint8_t> encryptedBuffer = streamEncryptBuffer(
            plainData,
            key,
            algot,
            hashBits,          // hash_size
            seed,              // seed (IV)
            salt,              // salt
            outputExtension,   // outputExtension
            verbose != 0       // verbose
        );

        // Serialize the output buffer
        size_t encryptedSize;
        uint8_t* encryptedPtr = serializeBuffer(encryptedBuffer, encryptedSize);

        // Set output pointers
        *outBufferPtr = encryptedPtr;
        *outBufferSizePtr = encryptedSize;
    }
    catch (const std::exception& e) {
        // In case of error, set output pointers to nullptr and size to 0
        *outBufferPtr = nullptr;
        *outBufferSizePtr = 0;
        // Optionally, log the error to stderr
        fprintf(stderr, "Error in wasmStreamEncryptBuffer: %s\n", e.what());
    }
}

// -------------------------------------------------------------------
// WASM Export: wasmStreamDecryptBuffer
// Description: Buffer-based stream decryption for WASM.
// -------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void wasmStreamDecryptBuffer(
    const uint8_t* inBufferPtr,
    size_t inBufferSize,
    const char* keyPtr,
    size_t keyLength,
    int verbose,
    uint8_t** outBufferPtr,
    size_t* outBufferSizePtr,
    char** errorPtr // New parameter for error messages
) {
    try {
        // Deserialize input buffer
        std::vector<uint8_t> encryptedData = deserializeBuffer(inBufferPtr, inBufferSize);

        // Convert C string to std::vector<uint8_t>
        std::vector<uint8_t> key(keyPtr, keyPtr + keyLength);

        // Call the buffer-based decryption function
        std::vector<uint8_t> decryptedBuffer = streamDecryptBuffer(
            encryptedData,
            key,
            verbose != 0
        );

        // Serialize the output buffer
        size_t decryptedSize;
        uint8_t* decryptedPtr = serializeBuffer(decryptedBuffer, decryptedSize);

        // Set output pointers
        *outBufferPtr = decryptedPtr;
        *outBufferSizePtr = decryptedSize;

        // Indicate no error
        *errorPtr = nullptr;
    }
    catch (const std::exception& e) {
        // In case of error, set output pointers to nullptr and size to 0
        *outBufferPtr = nullptr;
        *outBufferSizePtr = 0;

        // Allocate memory for the error message
        std::string errMsg = std::string("{\"error\":\"") + e.what() + "\"}";
        size_t errLen = errMsg.length();
        char* errCStr = (char*)malloc(errLen + 1);
        if (errCStr) {
            std::memcpy(errCStr, errMsg.c_str(), errLen);
            errCStr[errLen] = '\0'; // Null-terminate
            *errorPtr = errCStr;
        }
        else {
            *errorPtr = nullptr; // Allocation failed
        }

        // Optionally, log the error to stderr
        fprintf(stderr, "Error in wasmStreamDecryptBuffer: %s\n", e.what());
    }
}

// -------------------------------------------------------------------
// WASM Export: wasmBlockEncryptBuffer
// Description: Buffer-based block encryption for WASM.
// -------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
uint8_t* wasmBlockEncryptBuffer(
    const uint8_t* data, size_t data_len,
    const char* keyPtr,
    size_t keyLength,
    const char* algorithm,
    const char* searchMode,
    uint32_t hashBits,
    uint64_t seed,
    uint8_t* saltPtr,
    size_t saltLen,
    size_t blockSize,
    size_t nonceSize,
    uint32_t outputExtension,
    int deterministicNonce,
    int verbose,
    size_t* out_len
) {
    try {
        // Prepare input data
        std::vector<uint8_t> plainData(data, data + data_len);
        std::vector<uint8_t> key(keyPtr, keyPtr + keyLength);
        std::string algorithmStr(algorithm, strlen(algorithm));
        std::string searchModeStr(searchMode, strlen(searchMode));
        std::vector<uint8_t> salt(saltPtr, saltPtr + saltLen);

        // Determine HashAlgorithm enum
        HashAlgorithm algot;
        if (algorithmStr == "rainbow") {
            algot = HashAlgorithm::Rainbow;
        }
        else if (algorithmStr == "rainstorm") {
            algot = HashAlgorithm::Rainstorm;
        }
        else {
            throw std::runtime_error("Unsupported algorithm: " + algorithmStr);
        }

        // Call the buffer-based encryption function
        std::vector<uint8_t> encryptedBuffer = puzzleEncryptBufferWithHeader(
            plainData,
            key,
            algot,
            hashBits,          // hash_size
            seed,              // seed (IV)
            salt,              // salt
            static_cast<uint16_t>(blockSize),     // blockSize
            static_cast<uint16_t>(nonceSize),     // nonceSize
            searchModeStr,     // searchMode
            verbose == 1,      // verbose
            deterministicNonce == 1,  // deterministicNonce
            static_cast<uint16_t>(outputExtension)    // outputExtension
        );

        // Serialize the output buffer
        size_t encryptedSize;
        uint8_t* encryptedPtr = serializeBuffer(encryptedBuffer, encryptedSize);

        // Set output pointer
        *out_len = encryptedSize;
        return encryptedPtr;
    }
    catch (const std::exception &e) {
        fprintf(stderr, "wasmBlockEncryptBuffer error: %s\n", e.what());
        *out_len = 0;
        return nullptr;
    }
}

// -------------------------------------------------------------------
// WASM Export: wasmBlockDecryptBuffer
// Description: Buffer-based block decryption for WASM.
// -------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void wasmBlockDecryptBuffer(
    const uint8_t* inBufferPtr,
    size_t inBufferSize,
    const char* keyPtr,
    size_t keyLength,
    uint8_t** outBufferPtr,
    size_t* outBufferSizePtr
) {
    try {
        // Deserialize input buffer
        std::vector<uint8_t> cipherData = deserializeBuffer(inBufferPtr, inBufferSize);
        std::vector<uint8_t> key(keyPtr, keyPtr + keyLength);

        // Call the buffer-based decryption function
        std::vector<uint8_t> decryptedData = puzzleDecryptBufferWithHeader(cipherData, key);

        // Serialize output
        size_t decryptedSize;
        uint8_t* decryptedPtr = serializeBuffer(decryptedData, decryptedSize);

        // Set output pointers
        *outBufferPtr = decryptedPtr;
        *outBufferSizePtr = decryptedSize;
    }
    catch (const std::exception &e) {
        fprintf(stderr, "wasmBlockDecryptBuffer error: %s\n", e.what());
        *outBufferPtr = nullptr;
        *outBufferSizePtr = 0;
    }
}

} // extern "C"

#endif // __EMSCRIPTEN__

