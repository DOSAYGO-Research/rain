#pragma once
#ifndef FILE_HEADER_H
#define FILE_HEADER_H

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <string>
#include <cstring>   // for memcpy if needed

// -------------------------------------------------------------------
// FileHeader struct
// -------------------------------------------------------------------
#pragma pack(push, 1)
struct FileHeader {
  uint32_t magic;             // MagicNumber
  uint8_t version;            // Version
  uint8_t cipherMode;         // Mode: 0x10 = Stream, 0x11 = Block
  uint8_t blockSize;          // Block size in bytes (for block cipher)
  uint8_t nonceSize;          // Nonce size in bytes
  uint16_t hashSizeBits;      // Hash size in bits
  uint32_t outputExtension;   // How much to extend or handle
  std::string hashName;       // Hash algorithm name
  uint64_t iv;                // 64-bit IV (seed)
  uint8_t saltLen;            // Length of salt
  std::vector<uint8_t> salt;  // Salt data
  uint8_t searchModeEnum;     // Search mode enum (0x00 - 0x05 for block ciphers, 0xFF for stream)
  uint64_t originalSize;      // Compressed plaintext size
  std::array<uint8_t, 32> hmac; // HMAC (256-bit)
};
#pragma pack(pop)

// -------------------------------------------------------------------
// Write the unified FileHeader to a file
// -------------------------------------------------------------------
static void writeFileHeader(std::ofstream &out, const FileHeader &hdr) {
  out.write(reinterpret_cast<const char*>(&hdr.magic), sizeof(hdr.magic));
  out.write(reinterpret_cast<const char*>(&hdr.version), sizeof(hdr.version));
  out.write(reinterpret_cast<const char*>(&hdr.cipherMode), sizeof(hdr.cipherMode));
  out.write(reinterpret_cast<const char*>(&hdr.blockSize), sizeof(hdr.blockSize));
  out.write(reinterpret_cast<const char*>(&hdr.nonceSize), sizeof(hdr.nonceSize));
  out.write(reinterpret_cast<const char*>(&hdr.hashSizeBits), sizeof(hdr.hashSizeBits));
  out.write(reinterpret_cast<const char*>(&hdr.outputExtension), sizeof(hdr.outputExtension));

  uint8_t hnLen = static_cast<uint8_t>(hdr.hashName.size());
  out.write(reinterpret_cast<const char*>(&hnLen), sizeof(hnLen));
  out.write(hdr.hashName.data(), hnLen);

  out.write(reinterpret_cast<const char*>(&hdr.iv), sizeof(hdr.iv));

  out.write(reinterpret_cast<const char*>(&hdr.saltLen), sizeof(hdr.saltLen));
  if (hdr.saltLen > 0) {
    out.write(reinterpret_cast<const char*>(hdr.salt.data()), hdr.salt.size());
  }

  out.write(reinterpret_cast<const char*>(&hdr.searchModeEnum), sizeof(hdr.searchModeEnum));
  out.write(reinterpret_cast<const char*>(&hdr.originalSize), sizeof(hdr.originalSize));

  // Write HMAC
  out.write(reinterpret_cast<const char*>(hdr.hmac.data()), hdr.hmac.size());
}

// -------------------------------------------------------------------
// Read the unified FileHeader from a file
// -------------------------------------------------------------------
static FileHeader readFileHeader(std::istream &in) {
  FileHeader hdr{};
  in.read(reinterpret_cast<char*>(&hdr.magic), sizeof(hdr.magic));
  in.read(reinterpret_cast<char*>(&hdr.version), sizeof(hdr.version));
  in.read(reinterpret_cast<char*>(&hdr.cipherMode), sizeof(hdr.cipherMode));
  in.read(reinterpret_cast<char*>(&hdr.blockSize), sizeof(hdr.blockSize));
  in.read(reinterpret_cast<char*>(&hdr.nonceSize), sizeof(hdr.nonceSize));
  in.read(reinterpret_cast<char*>(&hdr.hashSizeBits), sizeof(hdr.hashSizeBits));
  in.read(reinterpret_cast<char*>(&hdr.outputExtension), sizeof(hdr.outputExtension));

  uint8_t hnLen = 0;
  in.read(reinterpret_cast<char*>(&hnLen), sizeof(hnLen));
  hdr.hashName.resize(hnLen);
  in.read(&hdr.hashName[0], hnLen);

  in.read(reinterpret_cast<char*>(&hdr.iv), sizeof(hdr.iv));

  in.read(reinterpret_cast<char*>(&hdr.saltLen), sizeof(hdr.saltLen));
  hdr.salt.resize(hdr.saltLen);
  if (hdr.saltLen > 0) {
    in.read(reinterpret_cast<char*>(hdr.salt.data()), hdr.saltLen);
  }

  in.read(reinterpret_cast<char*>(&hdr.searchModeEnum), sizeof(hdr.searchModeEnum));
  in.read(reinterpret_cast<char*>(&hdr.originalSize), sizeof(hdr.originalSize));

  // Read HMAC
  in.read(reinterpret_cast<char*>(hdr.hmac.data()), hdr.hmac.size());

  return hdr;
}

// -------------------------------------------------------------------
// Display file header info on stdout
// -------------------------------------------------------------------
static void showFileFullInfo(const std::string &inFilename) {
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("Cannot open file for info: " + inFilename);
  }

  FileHeader hdr = readFileHeader(fin);
  fin.close();

  if (hdr.magic != MagicNumber) {
    throw std::runtime_error("Invalid magic number in file.");
  }

  std::string cipherModeStr;
  if (hdr.cipherMode == 0x10) {
    cipherModeStr = "StreamCipher";
  }
  else if (hdr.cipherMode == 0x11) {
    cipherModeStr = "BlockCipher";
  }
  else {
    cipherModeStr = "Unknown/LegacyPuzzle";
  }

  std::cout << "=== Unified File Header Info ===\n";
  std::cout << "Magic: RCRY (0x" << std::hex << hdr.magic << std::dec << ")\n";
  std::cout << "Version: " << static_cast<int>(hdr.version) << "\n";
  std::cout << "Cipher Mode: " << cipherModeStr << " (0x"
            << std::hex << static_cast<int>(hdr.cipherMode) << std::dec << ")\n";
  std::cout << "Block Size: " << static_cast<int>(hdr.blockSize) << "\n";
  std::cout << "Nonce Size: " << static_cast<int>(hdr.nonceSize) << "\n";
  std::cout << "Hash Size: " << hdr.hashSizeBits << " bits\n";
  std::cout << "Output Extension: " << hdr.outputExtension << " bytes\n";
  std::cout << "Hash Algorithm: " << hdr.hashName << "\n";
  std::cout << "IV (Seed): 0x" << std::hex << hdr.iv << std::dec << "\n";
  std::cout << "Salt Length: " << static_cast<int>(hdr.saltLen) << "\n";
  if (hdr.saltLen > 0) {
    std::cout << "Salt Data: ";
    for (auto b : hdr.salt) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << "\n";
  }
  std::cout << "Compressed Plaintext Size: " << hdr.originalSize << " bytes\n";
  std::cout << "Search Mode Enum: 0x" << std::hex << static_cast<int>(hdr.searchModeEnum) << std::dec << "\n";
  std::cout << "HMAC: ";
  for (auto b : hdr.hmac) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(b) << " ";
  }
  std::cout << std::dec << "\n";
  std::cout << "===============================\n";
}

// -------------------------------------------------------------------
// Serialize FileHeader into a raw byte buffer
// -------------------------------------------------------------------
static std::vector<uint8_t> serializeFileHeader(const FileHeader &hdr) {
  std::vector<uint8_t> data;
  data.reserve(
    sizeof(hdr.magic)
    + sizeof(hdr.version)
    + sizeof(hdr.cipherMode)
    + sizeof(hdr.blockSize)
    + sizeof(hdr.nonceSize)
    + sizeof(hdr.hashSizeBits)
    + sizeof(hdr.outputExtension)
    + 1 /* hashName length byte */
    + hdr.hashName.size()
    + sizeof(hdr.iv)
    + sizeof(hdr.saltLen)
    + hdr.salt.size()
    + sizeof(hdr.searchModeEnum)
    + sizeof(hdr.originalSize)
    + hdr.hmac.size()
  );

  auto append = [&](const void* ptr, size_t size) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(ptr);
    data.insert(data.end(), bytes, bytes + size);
  };

  append(&hdr.magic, sizeof(hdr.magic));
  append(&hdr.version, sizeof(hdr.version));
  append(&hdr.cipherMode, sizeof(hdr.cipherMode));
  append(&hdr.blockSize, sizeof(hdr.blockSize));
  append(&hdr.nonceSize, sizeof(hdr.nonceSize));
  append(&hdr.hashSizeBits, sizeof(hdr.hashSizeBits));
  append(&hdr.outputExtension, sizeof(hdr.outputExtension));

  uint8_t hnLen = static_cast<uint8_t>(hdr.hashName.size());
  append(&hnLen, sizeof(hnLen));
  if (hnLen > 0) {
    data.insert(data.end(), hdr.hashName.begin(), hdr.hashName.end());
  }

  append(&hdr.iv, sizeof(hdr.iv));
  append(&hdr.saltLen, sizeof(hdr.saltLen));
  if (hdr.saltLen > 0) {
    data.insert(data.end(), hdr.salt.begin(), hdr.salt.end());
  }

  append(&hdr.searchModeEnum, sizeof(hdr.searchModeEnum));
  append(&hdr.originalSize, sizeof(hdr.originalSize));

  append(hdr.hmac.data(), hdr.hmac.size());

  return data;
}


#endif // FILE_HEADER_H

