#ifdef _OPENMP
#include <omp.h>
#endif
#include "../tool.h" // for invokeHash, etc.
#include "../file-header.h"
#include "../block-cipher.h"
#include "../stream-cipher.h"


// -------------------------------------------------------------------
// Optional: WASM bridging (if building with Emscripten)
// -------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>   // for malloc/free

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
    ss << "\"blockSize\":" << (int)hdr.blockSize << ",";
    ss << "\"nonceSize\":" << (int)hdr.nonceSize << ",";
    ss << "\"hashSizeBits\":" << hdr.hashSizeBits << ",";
    ss << "\"outputExtension\":" << hdr.outputExtension << ",";
    ss << "\"hashName\":\"" << hdr.hashName << "\",";
    ss << "\"iv\":\"0x" << hdr.iv << "\",";
    ss << "\"saltLen\":" << (int)hdr.saltLen << ",";
    ss << "\"salt\":\"";
    for (auto b : hdr.salt) {
      ss << (int)b << " ";
    }
    ss << "\",";
    ss << "\"searchModeEnum\":\"0x" << (int)hdr.searchModeEnum << "\",";
    ss << "\"originalSize\":" << hdr.originalSize << ",";
    ss << "\"hmac\":\"";
    for (auto b : hdr.hmac) {
      ss << std::setw(2) << std::setfill('0') << (int)b << " ";
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
