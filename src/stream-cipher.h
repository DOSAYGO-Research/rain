// The revised encryption and decryption functions:

// =================================================================
// Stream Encryption Function
// =================================================================
static void streamEncryptFileWithHeader(
    const std::string &inFilename,
    const std::string &outFilename,
    const std::string &key,
    HashAlgorithm algot,
    uint32_t hash_bits,
    uint64_t seed, // Used as IV
    const std::vector<uint8_t> &salt,
    uint32_t outputExtension,
    bool verbose
) {
  // 1) Read and compress the plaintext
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("[StreamEnc] Cannot open input file: " + inFilename);
  }
  std::vector<uint8_t> plainData((std::istreambuf_iterator<char>(fin)),
                                 (std::istreambuf_iterator<char>()));
  fin.close();

  auto compressed = compressData(plainData);

  // 2) Open output file
  std::ofstream fout(outFilename, std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("[StreamEnc] Cannot open output file: " + outFilename);
  }

  // 3) Prepare FileHeader
  FileHeader hdr{};
  hdr.magic = MagicNumber;
  hdr.version = 0x02;  // Example version
  hdr.cipherMode = 0x10;  // Stream Cipher Mode
  hdr.blockSize = 0;  // Not used for stream cipher
  hdr.nonceSize = 0;  // Not used for stream cipher
  hdr.outputExtension = outputExtension;
  hdr.hashSizeBits = hash_bits;
  hdr.hashName = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
  hdr.iv = seed; // Using seed as IV
  hdr.saltLen = static_cast<uint8_t>(salt.size());
  hdr.salt = salt;
  hdr.originalSize = compressed.size();

  // 4) Write FileHeader
  writeFileHeader(fout, hdr);

  // 5) Derive PRK
  std::vector<uint8_t> seed_vec(8);
  for (size_t i = 0; i < 8; ++i) {
    seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
  }
  std::vector<uint8_t> ikm(key.begin(), key.end());
  std::vector<uint8_t> prk = derivePRK(seed_vec, salt, ikm, algot, hash_bits, verbose);

  // 6) Generate Keystream
  size_t needed = compressed.size() + outputExtension;
  auto keystream = extendOutputKDF(prk, needed, algot, hash_bits);

  // Optional debug info
  if (verbose) {
    std::cerr << "\n[StreamEnc] --- ENCRYPT HEADER DATA ---\n";
    std::cerr << "[StreamEnc] Magic: 0x" << std::hex << hdr.magic << std::dec << "\n";
    std::cerr << "[StreamEnc] Version: " << static_cast<int>(hdr.version) << "\n";
    std::cerr << "[StreamEnc] CipherMode: 0x" << std::hex << static_cast<int>(hdr.cipherMode) << std::dec << "\n";
    std::cerr << "[StreamEnc] BlockSize: " << hdr.blockSize << "\n";
    std::cerr << "[StreamEnc] NonceSize: " << hdr.nonceSize << "\n";
    std::cerr << "[StreamEnc] outputExtension: " << hdr.outputExtension << "\n";
    std::cerr << "[StreamEnc] hashSizeBits: " << hdr.hashSizeBits << "\n";
    std::cerr << "[StreamEnc] hashName: " << hdr.hashName << "\n";
    std::cerr << "[StreamEnc] iv (seed): 0x" << std::hex << hdr.iv << std::dec << "\n";
    std::cerr << "[StreamEnc] saltLen: " << static_cast<int>(hdr.saltLen) << "\n";
    std::cerr << "[StreamEnc] salt: ";
    for (auto &s : hdr.salt) {
      std::cerr << std::hex << static_cast<int>(s) << " ";
    }
    std::cerr << std::dec << "\n";
    std::cerr << "[StreamEnc] originalSize: " << hdr.originalSize << "\n";

    std::cerr << "\n[StreamEnc] seed_vec: ";
    for (auto &b : seed_vec) {
      std::cerr << std::hex << static_cast<int>(b) << " ";
    }
    std::cerr << std::dec << "\n";

    std::cerr << "[StreamEnc] PRK (" << prk.size() << " bytes): ";
    for (auto &p : prk) {
      std::cerr << std::hex << static_cast<int>(p) << " ";
    }
    std::cerr << std::dec << "\n";

    std::cerr << "[StreamEnc] Keystream size: " << keystream.size() << "\n";
    std::cerr << "[StreamEnc] Keystream first 100 bytes:\n";
    for (size_t i = 0; i < std::min<size_t>(100, keystream.size()); i++) {
      std::cerr << std::hex << static_cast<int>(keystream[i]) << " ";
    }
    std::cerr << std::dec << "\n";

    std::cerr << "[StreamEnc] Compressed data size: " << compressed.size() << "\n";
  }

  // 7) XOR compressed data with keystream, skipping first 'outputExtension' bytes
  const size_t progress_step = 1000000; // 1,000,000 bytes
  size_t next_progress = progress_step;
  for (size_t i = 0; i < compressed.size(); ++i) {
      compressed[i] ^= keystream[i + outputExtension];

      // Progress update
      if (i + 1 == next_progress || i + 1 == compressed.size()) {
          std::cerr << "\r" << (i + 1) << "/" << compressed.size() << " bytes processed." << std::flush;
          next_progress += progress_step;
      }
  }
  std::cerr << std::endl; // Final newline after loop

  // 8) Write ciphertext
  fout.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
  fout.close();

  if (verbose) {
    std::cerr << "[StreamEnc] Encrypted " << compressed.size()
              << " bytes from " << inFilename
              << " to " << outFilename
              << " using IV=0x" << std::hex << seed << std::dec
              << " and salt of length " << static_cast<int>(hdr.saltLen) << " bytes.\n";
  }
}

// =================================================================
// Stream Decryption Function
// =================================================================
static void streamDecryptFileWithHeader(
    const std::string &inFilename,
    const std::string &outFilename,
    const std::string &key,
    bool verbose
) {
  // 1) Open input file and read FileHeader
  std::ifstream fin(inFilename, std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("[StreamDec] Cannot open input file: " + inFilename);
  }
  FileHeader hdr = readFileHeader(fin);
  if (hdr.magic != MagicNumber) {
    throw std::runtime_error("[StreamDec] Invalid magic number in header.");
  }
  if (hdr.cipherMode != 0x10) { // Stream Cipher Mode
    throw std::runtime_error("[StreamDec] File is not in Stream Cipher mode.");
  }

  if (verbose) {
    std::cerr << "\n[StreamDec] --- DECRYPT HEADER DATA ---\n";
    std::cerr << "[StreamDec] Magic: 0x" << std::hex << hdr.magic << std::dec << "\n";
    std::cerr << "[StreamDec] Version: " << static_cast<int>(hdr.version) << "\n";
    std::cerr << "[StreamDec] CipherMode: 0x" << std::hex << static_cast<int>(hdr.cipherMode) << std::dec << "\n";
    std::cerr << "[StreamDec] BlockSize: " << hdr.blockSize << "\n";
    std::cerr << "[StreamDec] NonceSize: " << hdr.nonceSize << "\n";
    std::cerr << "[StreamDec] outputExtension: " << hdr.outputExtension << "\n";
    std::cerr << "[StreamDec] hashSizeBits: " << hdr.hashSizeBits << "\n";
    std::cerr << "[StreamDec] hashName: " << hdr.hashName << "\n";
    std::cerr << "[StreamDec] iv (seed): 0x" << std::hex << hdr.iv << std::dec << "\n";
    std::cerr << "[StreamDec] saltLen: " << static_cast<int>(hdr.saltLen) << "\n";
    std::cerr << "[StreamDec] salt: ";
    for (auto &s : hdr.salt) {
      std::cerr << std::hex << static_cast<int>(s) << " ";
    }
    std::cerr << std::dec << "\n";
    std::cerr << "[StreamDec] originalSize: " << hdr.originalSize << "\n";
  }

  HashAlgorithm algot = HashAlgorithm::Unknown;
  if (hdr.hashName == "rainbow") {
      algot = HashAlgorithm::Rainbow;
  }
  else if (hdr.hashName == "rainstorm") {
      algot = HashAlgorithm::Rainstorm;
  }
  else {
      throw std::runtime_error("Unsupported hash algorithm in header: " + hdr.hashName);
  }

  // 2) Read ciphertext
  std::vector<uint8_t> cipherData((std::istreambuf_iterator<char>(fin)),
                                  (std::istreambuf_iterator<char>()));
  fin.close();

  if (verbose) {
    std::cerr << "[StreamDec] Cipher data size: " << cipherData.size() << "\n";
  }

  // 3) Derive PRK
  std::vector<uint8_t> seed_vec(8);
  for (size_t i = 0; i < 8; ++i) {
    seed_vec[i] = static_cast<uint8_t>((hdr.iv >> (i * 8)) & 0xFF);
  }
  std::vector<uint8_t> ikm(key.begin(), key.end());
  std::vector<uint8_t> prk = derivePRK(seed_vec, hdr.salt, ikm, algot, hdr.hashSizeBits, verbose);

  // 4) Generate Keystream
  size_t needed = cipherData.size() + hdr.outputExtension;
  auto keystream = extendOutputKDF(prk, needed, algot, hdr.hashSizeBits);

  if (verbose) {
    std::cerr << "\n[StreamDec] seed_vec: ";
    for (auto &b : seed_vec) {
      std::cerr << std::hex << static_cast<int>(b) << " ";
    }
    std::cerr << std::dec << "\n";

    std::cerr << "[StreamDec] PRK (" << prk.size() << " bytes): ";
    for (auto &p : prk) {
      std::cerr << std::hex << static_cast<int>(p) << " ";
    }
    std::cerr << std::dec << "\n";

    std::cerr << "[StreamDec] Keystream size: " << keystream.size() << "\n";
    std::cerr << "[StreamDec] Keystream first 100 bytes:\n";
    for (size_t i = 0; i < std::min<size_t>(100, keystream.size()); i++) {
      std::cerr << std::hex << static_cast<int>(keystream[i]) << " ";
    }
    std::cerr << std::dec << "\n";
  }

  // 5) XOR ciphertext with keystream (skipping first 'outputExtension' bytes)
  const size_t progress_step = 1000000; // 1,000,000 bytes
  size_t next_progress = progress_step;
  for (size_t i = 0; i < cipherData.size(); ++i) {
    cipherData[i] ^= keystream[i + hdr.outputExtension];
    // Progress update
    if (i + 1 == next_progress || i + 1 == cipherData.size()) {
      std::cerr << "\r" << (i + 1) << "/" << cipherData.size() << " bytes processed." << std::flush;
      next_progress += progress_step;
    }
  }
  std::cerr << std::endl; // Final newline after loop

  // 6) Decompress to get plaintext
  auto decompressed = decompressData(cipherData);

  // 7) Write plaintext to output file
  std::ofstream fout(outFilename, std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("[StreamDec] Cannot open output file: " + outFilename);
  }
  fout.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
  fout.close();

  if (verbose) {
    std::cerr << "[StreamDec] Decompressed data size: " << decompressed.size() << "\n";
    std::cerr << "[StreamDec] Decrypted " << decompressed.size()
              << " bytes from " << inFilename
              << " to " << outFilename
              << " using IV=0x" << std::hex << hdr.iv << std::dec
              << " and salt of length " << static_cast<int>(hdr.saltLen) << " bytes.\n\n";
  }
}


