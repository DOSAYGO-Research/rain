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
#include "../tool.h" // for invokeHash, etc.
#include "../file-header.h"
#include "../block-cipher.h"
#include "../stream-cipher.h"

#ifdef __EMSCRIPTEN__
extern "C" {

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

}
#endif


#ifdef __EMSCRIPTEN__
extern "C" {

  KEEPALIVE void rainbowHash64(const void* in, const size_t len, const seed_t seed, void* out) {
    rainbow::rainbow<64, false>(in, len, seed, out);
  }

  KEEPALIVE void rainbowHash128(const void* in, const size_t len, const seed_t seed, void* out) {
    rainbow::rainbow<128, false>(in, len, seed, out);
  }

  KEEPALIVE void rainbowHash256(const void* in, const size_t len, const seed_t seed, void* out) {
    rainbow::rainbow<256, false>(in, len, seed, out);
  }

}
#endif

extern "C" {

  /**
   * Parse a file header from a buffer in WASM memory and return a JSON-like string
   * describing the parsed info. Caller must free the returned char* via wasmFree.
   */
  EMSCRIPTEN_KEEPALIVE
  char* wasmGetFileHeaderInfo(const uint8_t* data, size_t size) {
    try {
      // Load data into a std::vector
      std::vector<uint8_t> buffer(data, data + size);

      // We'll mimic the readFileHeader logic, but from memory.
      // This is just a quick approach (you might prefer to factor out a readFromMemory function).
      if (buffer.size() < (sizeof(FileHeader) - 32)) { // a rough check
        // You might do a more exact check using your known fields
        throw std::runtime_error("Buffer too small to contain valid header");
      }

      // We'll parse the header in a style similar to readFileHeader:
      // Just be sure you read exactly in the same order (magic, version, cipherMode, etc.)
      // For brevity, let's re-use serialize/deserialize logic if you prefer.

      std::stringstream ss;
      ss << std::hex << "{";  // Start JSON-ish

      FileHeader hdr{};
      size_t offset = 0;

      auto readAndAdvance = [&](void* dst, size_t len) {
        if (offset + len > buffer.size()) {
          throw std::runtime_error("Buffer overrun in parse");
        }
        std::memcpy(dst, &buffer[offset], len);
        offset += len;
      };

      // magic
      readAndAdvance(&hdr.magic, sizeof(hdr.magic));
      // version
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
        throw std::runtime_error("Invalid magic number in memory header.");
      }

      ss << "\"magic\":\"0x" << hdr.magic << "\",";
      ss << "\"version\":" << (int)hdr.version << ",";
      ss << "\"cipherMode\":\"0x" << (int)hdr.cipherMode << "\",";
      ss << "\"blockSize\":\"0x" << (int)hdr.blockSize << "\",";
      ss << "\"nonceSize\":\"0x" << (int)hdr.nonceSize << "\",";
      ss << "\"hashSizeBits\":\"0x" << hdr.hashSizeBits << "\",";
      ss << "\"outputExtension\":\"0x" << hdr.outputExtension << "\",";
      ss << "\"hashName\":\"" << hdr.hashName << "\",";
      ss << "\"iv\":\"0x" << hdr.iv << "\",";
      ss << "\"saltLen\":\"0x" << (int)hdr.saltLen << "\",";
      ss << "\"salt\":\"";
      for (auto b : hdr.salt) {
        ss << std::setw(2) << std::setfill('0') << (int)b << "";
      }
      ss << "\",";
      ss << "\"searchModeEnum\":\"0x" << (int)hdr.searchModeEnum << "\",";
      ss << "\"originalSize\":\"0x" << hdr.originalSize << "\",";
      ss << "\"hmac\":\"";
      for (auto b : hdr.hmac) {
        ss << std::setw(2) << std::setfill('0') << (int)b << "";
      }
      ss << "\"}";  // End JSON-ish

      std::string result = ss.str();
      // Allocate a new char* in WASM memory for returning
      char* output = (char*)malloc(result.size() + 1);
      std::memcpy(output, result.c_str(), result.size() + 1);
      return output;
    }
    catch (const std::exception &e) {
      // Return an error string. In real usage, might prefer a dedicated error code system.
      std::string err = std::string("{\"error\":\"") + e.what() + "\"}";
      char* output = (char*)malloc(err.size() + 1);
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

} // extern "C"
#endif // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__

  /**
   * Utility function to serialize a buffer into a WASM-allocated memory block.
   * Returns the pointer to the serialized buffer.
   */
  uint8_t* serializeBuffer(const std::vector<uint8_t>& buffer, size_t& outSize) {
      outSize = buffer.size();
      uint8_t* wasmBuffer = (uint8_t*)malloc(outSize);
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

extern "C" {


  /**
   * Buffer-based stream encryption for WASM.
   *
   * @param inBufferPtr        Pointer to the input buffer in WASM memory.
   * @param inBufferSize       Size of the input buffer.
   * @param keyPtr             Pointer to the key string in WASM memory.
   * @param keyLength          Length of the key string.
   * @param algorithmPtr       Pointer to the algorithm string in WASM memory.
   * @param algorithmLength    Length of the algorithm string.
   * @param hashBits           Hash size in bits.
   * @param seed               64-bit seed (IV).
   * @param saltPtr            Pointer to the salt buffer in WASM memory.
   * @param saltLen            Length of the salt buffer.
   * @param outputExtension    Extra bytes of keystream offset.
   * @param verbose            Verbose flag (0 or 1).
   * @param outBufferPtr       Pointer to store the output buffer address.
   * @param outBufferSizePtr   Pointer to store the output buffer size.
   */
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
          } else if (algorithm == "rainstorm") {
              algot = HashAlgorithm::Rainstorm;
          } else {
              throw std::runtime_error("Unsupported algorithm: " + algorithm);
          }

          // Call the buffer-based encryption function
          std::vector<uint8_t> encryptedBuffer = streamEncryptBuffer(
              plainData,
              key,
              algot,
              hashBits,
              seed,
              salt,
              outputExtension,
              verbose != 0
          );

          // Serialize the output buffer
          size_t encryptedSize;
          uint8_t* encryptedPtr = serializeBuffer(encryptedBuffer, encryptedSize);

          // Set output pointers
          *outBufferPtr = encryptedPtr;
          *outBufferSizePtr = encryptedSize;
      } catch (const std::exception& e) {
          // In case of error, set output pointers to nullptr and size to 0
          *outBufferPtr = nullptr;
          *outBufferSizePtr = 0;
          // Optionally, log the error to stderr
          fprintf(stderr, "Error in wasmStreamEncryptBuffer: %s\n", e.what());
      }
  }

  /**
   * Buffer-based stream decryption for WASM.
   *
   * @param inBufferPtr        Pointer to the input buffer in WASM memory.
   * @param inBufferSize       Size of the input buffer.
   * @param keyPtr             Pointer to the key string in WASM memory.
   * @param keyLength          Length of the key string.
   * @param verbose            Verbose flag (0 or 1).
   * @param outBufferPtr       Pointer to store the output buffer address.
   * @param outBufferSizePtr   Pointer to store the output buffer size.
   * @param errorPtr           Pointer to store the error message pointer.
   */
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

          // Convert C string to std::string
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
      } catch (const std::exception& e) {
          /*
          // In case of error, set output pointers to nullptr and size to 0
          *outBufferPtr = nullptr;
          *outBufferSizePtr = 0;

          // Allocate memory for the error message
          std::string errMsg = e.what();
          size_t errLen = errMsg.length();
          char* errCStr = (char*)malloc(errLen + 1);
          if (errCStr) {
              std::memcpy(errCStr, errMsg.c_str(), errLen);
              errCStr[errLen] = '\0'; // Null-terminate
              *errorPtr = errCStr;
          } else {
              *errorPtr = nullptr;
          }

          // Optionally, log the error to stderr
          fprintf(stderr, "Error in wasmStreamDecryptBuffer: %s\n", e.what());
          */
      }
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

} // extern "C"
#endif // __EMSCRIPTEN__

  static std::vector<uint8_t> encryptInternal(const uint8_t* data, size_t data_len, const char* keyPtr, size_t keyLen, const char* algorithm, const char* searchMode, uint32_t hashBits, uint64_t seed, uint8_t* saltPtr, size_t saltLen, uint16_t blockSize, uint32_t nonceSize, uint32_t outputExtension, int deterministicNonce, int verbose) {
      std::vector<uint8_t> plainData(data, data + data_len);
      std::vector<uint8_t> key(keyPtr, keyPtr + keyLen);
      std::string algorithmStr(algorithm);
      std::string searchModeStr(searchMode);
      std::vector<uint8_t> salt(saltPtr, saltPtr + saltLen); 

      // Determine HashAlgorithm enum
      HashAlgorithm algot;
      if (algorithmStr == "rainbow") {
          algot = HashAlgorithm::Rainbow;
      } else if (algorithmStr == "rainstorm") {
          algot = HashAlgorithm::Rainstorm;
      } else {
          throw std::runtime_error("Unsupported algorithm: " + algorithmStr);
      }

      return puzzleEncryptBufferWithHeader(
          plainData,
          key,
          algot,
          hashBits, // hash_size
          seed, // seed (example)
          salt, // salt (example empty)
          blockSize, // blockSize
          nonceSize, // nonceSize
          searchModeStr, // searchMode
          verbose == 1, // verbose
          deterministicNonce == 1,  // deterministicNonce
          outputExtension     // outputExtension
      );
  }

#ifdef __EMSCRIPTEN__
extern "C" {

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
      auto encrypted = encryptInternal(data, data_len, keyPtr, keyLength, algorithm, searchMode, hashBits, seed, saltPtr, saltLen, blockSize, nonceSize, outputExtension, deterministicNonce, verbose);
      *out_len = encrypted.size();

      // Allocate memory for the result and copy data
      uint8_t* result = static_cast<uint8_t*>(malloc(encrypted.size()));
      memcpy(result, encrypted.data(), encrypted.size());
      return result;
    } catch (const std::exception &e) {
      fprintf(stderr, "wasmBlockEncryptBuffer error: %s\n", e.what());
      *out_len = 0;
    }
  }

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
      // Deserialize inputs
      std::vector<uint8_t> cipherData(inBufferPtr, inBufferPtr + inBufferSize);
      std::vector<uint8_t> key(keyPtr, keyPtr + keyLength);

      // Call refactored function
      std::vector<uint8_t> decryptedData = puzzleDecryptBufferWithHeader(cipherData, key);

      // Serialize output
      *outBufferSizePtr = decryptedData.size();
      *outBufferPtr = (uint8_t*)malloc(*outBufferSizePtr);
      std::memcpy(*outBufferPtr, decryptedData.data(), *outBufferSizePtr);
    } catch (const std::exception &e) {
      fprintf(stderr, "wasmBlockDecryptBuffer error: %s\n", e.what());
      *outBufferPtr = nullptr;
      *outBufferSizePtr = 0;
    }
  }

} // extern "C"

#endif // __EMSCRIPTEN__
