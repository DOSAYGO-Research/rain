// =================================================================
// ADDED: Stream Encryption and Decryption Functions
// =================================================================

  // ADDED: Stream Encryption Function
  static void streamEncryptFileWithHeader(
      const std::string &inFilename,
      const std::string &outFilename,
      const std::string &key,
      HashAlgorithm algot,
      uint32_t hash_bits,
      uint64_t seed, // Used as IV
      const std::vector<uint8_t> &salt,
      uint32_t output_extension,
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
      auto compressed = compressData(plainData); // Assuming compressData is defined in tool.h

      // 2) Open output file
      std::ofstream fout(outFilename, std::ios::binary);
      if (!fout.is_open()) {
          throw std::runtime_error("[StreamEnc] Cannot open output file: " + outFilename);
      }

      // 3) Prepare FileHeader
      FileHeader hdr{};
      hdr.magic = MagicNumber;
      hdr.version = 0x02; // Example version
      hdr.cipherMode = 0x10; // Stream Cipher Mode
      hdr.blockSize = 0; // Not used for stream cipher
      hdr.nonceSize = 0; // Not used for stream cipher
      hdr.hashSizeBits = hash_bits;
      hdr.hashName = (algot == HashAlgorithm::Rainbow) ? "rainbow" : "rainstorm";
      hdr.iv = seed; // Using seed as IV
      hdr.saltLen = static_cast<uint8_t>(salt.size());
      hdr.salt = salt;
      hdr.originalSize = compressed.size();

      // 4) Write FileHeader
      writeFileHeader(fout, hdr);

      // 5) Derive PRK
      // Convert seed to vector<uint8_t>
      std::vector<uint8_t> seed_vec(8);
      for (size_t i = 0; i < 8; ++i) {
          seed_vec[i] = static_cast<uint8_t>((seed >> (i * 8)) & 0xFF);
      }

      std::vector<uint8_t> ikm(key.begin(), key.end());
      std::vector<uint8_t> prk = derivePRK(seed_vec, salt, ikm, algot, hash_bits); // Using seed_vec

      // 6) Generate Keystream with KDF
      size_t needed = compressed.size();
      auto keystream = extendOutputKDF(prk, needed, algot, hash_bits);

      // 7) XOR compressed data with keystream
      for (size_t i = 0; i < compressed.size(); ++i) {
          compressed[i] ^= keystream[i];
      }

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

  // ADDED: Stream Decryption Function
  static void streamDecryptFileWithHeader(
      const std::string &inFilename,
      const std::string &outFilename,
      const std::string &key,
      HashAlgorithm algot,
      uint32_t hash_bits,
      uint32_t output_extension,
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

      // 2) Read ciphertext
      std::vector<uint8_t> cipherData(
          (std::istreambuf_iterator<char>(fin)),
          (std::istreambuf_iterator<char>())
      );
      fin.close();

      // 3) Derive PRK
      // Convert seed to vector<uint8_t>
      std::vector<uint8_t> seed_vec(8);
      for (size_t i = 0; i < 8; ++i) {
          seed_vec[i] = static_cast<uint8_t>((hdr.iv >> (i * 8)) & 0xFF);
      }

      std::vector<uint8_t> ikm(key.begin(), key.end());
      std::vector<uint8_t> prk = derivePRK(seed_vec, hdr.salt, ikm, algot, hash_bits); // Using seed_vec

      // 4) Generate Keystream with KDF
      size_t needed = cipherData.size();
      auto keystream = extendOutputKDF(prk, needed, algot, hash_bits);

      // 5) XOR ciphertext with keystream
      for (size_t i = 0; i < cipherData.size(); ++i) {
          cipherData[i] ^= keystream[i];
      }

      // 6) Decompress to get plaintext
      auto decompressed = decompressData(cipherData); // Assuming decompressData is defined in tool.h

      // 7) Write plaintext to output file
      std::ofstream fout(outFilename, std::ios::binary);
      if (!fout.is_open()) {
          throw std::runtime_error("[StreamDec] Cannot open output file: " + outFilename);
      }
      fout.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
      fout.close();

      if (verbose) {
          std::cerr << "[StreamDec] Decrypted " << decompressed.size() 
                    << " bytes from " << inFilename 
                    << " to " << outFilename 
                    << " using IV=0x" << std::hex << hdr.iv << std::dec 
                    << " and salt of length " << static_cast<int>(hdr.saltLen) << " bytes.\n";
      }
  }
