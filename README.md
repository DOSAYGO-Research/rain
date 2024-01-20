# Rain

- [Rain](#rain)
  - [Repository structure](#repository-structure)
  - [Rainbow](#rainbow)
  - [Rainstorm - Unvetted for Security](#rainstorm---unvetted-for-security)
  - [Note on Cryptographic Intent](#note-on-cryptographic-intent)
  - [Genesis](#genesis)
  - [License](#license)
- [Rainsum Field Manual](#rainsum-field-manual)
  - [1. Introduction](#1-introduction)
  - [2. Basic Usage](#2-basic-usage)
    - [2.1 Command Structure](#21-command-structure)
    - [2.2 Options](#22-options)
  - [3. Modes of Operation](#3-modes-of-operation)
    - [3.1 Digest Mode](#31-digest-mode)
    - [3.2 Stream Mode](#32-stream-mode)
  - [4. Hash Algorithms and Sizes](#4-hash-algorithms-and-sizes)
  - [5. Test Vectors](#5-test-vectors)
  - [6. Seed Values](#6-seed-values)
  - [7. Help and Version Information](#7-help-and-version-information)
  - [8. Compilation](#8-compilation)
  - [9. Conclusion](#9-conclusion)
- [Developer Information](#developer-information)
  - [Stability](#stability)
  - [Test vectors](#test-vectors)
  - [Building and Installing](#building-and-installing)
  - [Contributions](#contributions)

This repository houses the Rainbow and Rainstorm hash functions, developed by Cris Stringfellow and licensed under Apache-2.0. The 64-bit variants have passed all tests in the [SMHasher3](https://gitlab.com/fwojcik/smhasher3) suite. [Results](results) can be found in the `results/` subdirectory.

| Algorithm | Speed | Hash Size | Purpose | Core Mixing Function | Security |
| :- | :- | :- | :- | :- | :- |
| Rainbow | 13.2 GiB/sec | 64 to 256 bits | General-purpose non-cryptographic hashing | Multiplication, subtraction/addition, rotation, XOR | Not designed for cryptographic security |
| Rainstorm | 4.7 GiB/sec (at 4 rounds) | 64 to 512 bits | Potential cryptographic hashing | Addition/subtraction, rotation, XOR | No formal security analysis yet |

## Benchmark

```text
Rain hash functions C++ vs Node/WASM benchmark:

Test Input & Size (bytes)         Run        C++ Version       WASM Version          Fastest
input1 (10 bytes)                  1         4,790,708 ns     59,540,125 ns     12.00x (C++ wins!)
input2 (100 bytes)                 1         3,881,917 ns     59,735,458 ns     15.00x (C++ wins!)
input3 (1,000 bytes)               1         3,973,792 ns     60,026,667 ns     15.00x (C++ wins!)
input4 (10,000 bytes)              1         4,231,250 ns     59,217,250 ns     13.00x (C++ wins!)
input5 (100,000 bytes)             1         4,002,792 ns     61,180,500 ns     15.00x (C++ wins!)
input6 (1,000,000 bytes)           1         4,387,500 ns     60,962,209 ns     13.00x (C++ wins!)
input7 (10,000,000 bytes)          1         7,945,250 ns     66,440,416 ns      8.00x (C++ wins!)
input8 (100,000,000 bytes)         1        43,348,167 ns    118,088,750 ns      2.00x (C++ wins!)
input1 (10 bytes)                  2         3,835,875 ns     60,245,292 ns     15.00x (C++ wins!)
input2 (100 bytes)                 2         3,794,541 ns     60,314,583 ns     15.00x (C++ wins!)
input3 (1,000 bytes)               2         3,897,708 ns     59,611,417 ns     15.00x (C++ wins!)
input4 (10,000 bytes)              2         3,881,916 ns     61,785,041 ns     15.00x (C++ wins!)
input5 (100,000 bytes)             2         3,836,458 ns     60,081,083 ns     15.00x (C++ wins!)
input6 (1,000,000 bytes)           2         4,218,959 ns     60,323,458 ns     14.00x (C++ wins!)
input7 (10,000,000 bytes)          2         8,120,458 ns     65,705,458 ns      8.00x (C++ wins!)
input8 (100,000,000 bytes)         2        42,743,958 ns    116,511,708 ns      2.00x (C++ wins!)
input1 (10 bytes)                  3         4,075,750 ns     59,484,125 ns     14.00x (C++ wins!)
input2 (100 bytes)                 3         3,811,750 ns     59,731,250 ns     15.00x (C++ wins!)
input3 (1,000 bytes)               3         3,814,666 ns     59,546,625 ns     15.00x (C++ wins!)
input4 (10,000 bytes)              3         3,785,791 ns     60,626,000 ns     16.00x (C++ wins!)
input5 (100,000 bytes)             3         3,601,666 ns     59,802,625 ns     16.00x (C++ wins!)
input6 (1,000,000 bytes)           3         4,191,166 ns     60,641,416 ns     14.00x (C++ wins!)
input7 (10,000,000 bytes)          3         8,095,875 ns     68,071,500 ns      8.00x (C++ wins!)
input8 (100,000,000 bytes)         3        43,222,708 ns    117,334,333 ns      2.00x (C++ wins!)
```

## Rainbow 

Rainbow is a fast hash function (13.2 GiB/sec, 4.61 bytes/cycle on long messages, 24.8 cycles/hash for short messages). It's intended for general-purpose, non-cryptographic hashing. The core mixing function utilizes multiplication, subtraction/addition, rotation, and XOR. 

## Repository structure

Below is the repo structure before running make (but after npm install in `js/` and `scripts/`. 

```tree
.
|-- LICENSE.txt
|-- Makefile
|-- README.md
|-- js
|   |-- lib
|   |   `-- api.mjs
|   |-- node_modules
|   |   |-- ansi-regex
|   |   |-- ansi-styles
|   |   |-- cliui
|   |   |-- color-convert
|   |   |-- color-name
|   |   |-- emoji-regex
|   |   |-- escalade
|   |   |-- get-caller-file
|   |   |-- is-fullwidth-code-point
|   |   |-- require-directory
|   |   |-- string-width
|   |   |-- strip-ansi
|   |   |-- wrap-ansi
|   |   |-- y18n
|   |   |-- yargs
|   |   `-- yargs-parser
|   |-- package-lock.json
|   |-- package.json
|   |-- rainsum.mjs
|   `-- test.mjs
|-- rain
|-- results
|   |-- dieharder
|   |   |-- README.md
|   |   |-- rainbow-256.txt
|   |   |-- rainbow-64-infinite.txt
|   |   |-- rainstorm-256.txt
|   |   `-- rainstorm-64-infinite.txt
|   `-- smhasher3
|       |-- rainbow-064.txt
|       |-- rainbow-128.txt
|       |-- rainstorm-064.txt
|       `-- rainstorm-128.txt
|-- scripts
|   |-- 1srain.sh
|   |-- bench.mjs
|   |-- blockchain.sh
|   |-- build.sh
|   |-- node_modules
|   |   `-- chalk
|   |-- package-lock.json
|   |-- package.json
|   |-- testjs.sh
|   |-- vectors.sh
|   `-- verify.sh
|-- src
|   |-- common.h
|   |-- cxxopts.hpp
|   |-- rainbow.cpp
|   |-- rainstorm.cpp
|   |-- rainsum.cpp
|   `-- tool.h
`-- verification
    `-- vectors.txt

29 directories, 33 files
```

## Rainstorm - **Unvetted for Security**

Rainstorm is a slower hash function with a tunable-round feature (with 4 rounds runs at 4.7 GiB/sec). It's designed with cryptographic hashing in mind, but it hasn't been formally analyzed for security, so we provide no guarantees. The core mixing function uses addition/subtraction, rotation, and XOR.

Rainstorm's round number is adjustable, potentially offering additional security. However, please note that this is hypothetical until rigorous security analysis is completed. 

## Note on Cryptographic Intent

While Rainstorm's design reflects cryptographic hashing principles, it has not been formally analyzed and thus, cannot be considered 'secure.' We strongly encourage those interested to conduct an analysis and offer feedback.

Great! I see that the program `rainsum.cpp` is for calculating a Rainbow or Rainstorm hash. The software can operate in two modes, "digest" or "stream", which affects how it outputs the hash. It also allows the user to select different hash algorithms and sizes, specify the input and output files, use predefined test vectors, and set the output length in hashes.

## Genesis

The fundamental concept for the mixing functions derived from Discohash, but has been significantly developed and extended. The overall architecture and processing flow of the hash were inspired by existing hash functions.

## License

This repository and content is licensed under Apache-2.0 unless otherwise noted. It's copyright &copy; Cris Stringfellow and The Dosyago Corporation 2023. All rights reserved. 

# Rainsum Field Manual

## 1. Introduction
Rainsum is a powerful command-line utility for calculating Rainbow or Rainstorm hashes of input data. This tool can operate in two modes: "digest" or "stream". In "digest" mode, Rainsum outputs a fixed-length hash in hex, while in "stream" mode it produces variable-length binary output.

Rainsum also offers multiple hashing algorithms, sizes, and various configurable options to cater to a wide range of use cases.

### JavaScript WASM Version

There is also a JavaScript WASM version, consistent with the C++ version, and 8 - 16 times slower on small and medium inputs (100 bytes to 10MiB), and 2 - 3 times slower on large inputs (100MiB and up), at `js/rainsum.mjs`. This JavaScript version of rainsum can be used mostly like the C++ version, so the below guide and instrutions suffice essentially for both.

## 2. Basic Usage

### 2.1 Command Structure
The basic command structure of Rainsum is as follows:

```
rainsum [OPTIONS] [INFILE]
```
Here, `[OPTIONS]` is a list of options that modify the behavior of Rainsum and `[INFILE]` is the input file to hash. If no file is specified, Rainsum reads from standard input.

### 2.2 Options
Here are the options that you can use with Rainsum:

- `-m, --mode [digest|stream]`: Specifies the mode. Default is `digest`.
- `-a, --algorithm [bow|storm]`: Specify the hash algorithm to use. Default is `storm`.
- `-s, --size [64-256|64-512]`: Specify the bit size of the hash. Default is `256`.
- `-o, --output-file FILE`: Specifies the output file for the hash or stream.
- `-t, --test-vectors`: Calculates the hash of the standard test vectors.
- `-l, --output-length HASHES`: Sets the output length in hash iterations (stream only).
- `--seed`: Seed value (64-bit number or string). If a string is used, it is hashed with Rainstorm to a 64-bit number.
- `-h, --help`: Prints usage information.
- `-v, --version`: Prints out the version of the software.

## 3. Modes of Operation

### 3.1 Digest Mode
In digest mode, Rainsum calculates a fixed-length hash of the input data and outputs the result in hexadecimal form.

For example, to calculate a 256-bit Rainstorm hash of `input.txt` and output it to `output.txt`, you would use:

```
rainsum -m digest -a storm -s 256 -o output.txt input.txt
```

### 3.2 Stream Mode
In stream mode, Rainsum calculates a hash of the input data and then uses that hash as input to the next iteration of the hash function, repeating this process for a specified number of iterations. The result is a stream of binary data.

For example, to generate a 512-bit Rainstorm hash stream of `input.txt` for 1000000 iterations and output it to `output.txt`, you would use:

```
rainsum -m stream -a storm -s 512 -l 1000000 -o output.txt input.txt
```

## 4. Hash Algorithms and Sizes
Rainsum supports the following hash algorithms:

- `bow`: Rainbow hash
- `storm`: Rainstorm hash

And these sizes (in bits):

- Rainbow: `64`, `128`, `256`
- Rainstorm: `64`, `128`, `256`, `512`

## 5. Test Vectors
Rainsum includes a set of predefined test vectors that you can use to verify the correct operation of the hash functions. To use these test vectors, include the `-t` or `--test-vectors` option in your command.

## 6. Seed Values
You can provide a seed value for the hash function using the `--seed` option followed by a 64-bit number or a string. If a string is used, Rainsum will hash it with Rainstorm to generate a 64-bit number.

## 7. Help and Version Information
Use `-h` or `--help` to print usage information. Use `-v` or `--version` to print the version of the software.

## 8. Compilation
Rainsum is written in C++. Make sure to have a modern C++ compiler and appropriate libraries (like filesystem, iostream, etc.) installed to compile the code. A makefile or build system setup might be required depending on your specific project configuration.

## 9. Conclusion
Rainsum provides a powerful and flexible command-line interface for calculating Rainbow and Rainstorm hashes. Whether you're working on a small project or a large-scale system, Rainsum offers the features and options you need to get the job done.

# Developer Information

## Stability

The hashes' stability may change over time, as we might modify constants, mixing specifics, and more as we gather insights. Should such changes alter the hashes' output, we will denote the changes with new version numbers. 

## Test vectors

The current test vectors for Rainstorm and Rainbow are:

**Rainbow v1.0.4 Test Vectors**

`./rainsum -a bow --test-vectors`:

```test
b735f3165b474cf1a824a63ba18c7d087353e778b6d38bd1c26f7b027c6980d9 ""
c7343ac7ee1e4990b55227b0182c41e9a6bbc295a17e2194d4e0081124657c3c "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
53efdb8f046dba30523e9004fce7d194fdf6c79a59d6e3687907652e38e53123 "The quick brown fox jumps over the lazy dog"
95a3515641473aa3726bcc5f454c658bfc9b714736f3ffa8b347807775c2078e "The quick brown fox jumps over the lazy cog"
f27c10f32ae243afea08dfb15e0c86c0b601792d1cd195ca651fe5394c56f200 "The quick brown fox jumps over the lazy dog."
e21780122142956ff99d560069a123b75d014f0b110d307d9b23d79f58ebeb29 "After the rainstorm comes the rainbow."
a46a9e5cba400ed3e1deec852fb0667e8acbbcfeb71cf0f3a1901396aaae6e19 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
```

**Rainstorm v0.0.5 Test Vectors**

`./rainsum --test-vectors`:

```text
e3ea5f8885f7bb16468d08c578f0e7cc15febd31c27e323a79ef87c35756ce1e ""
9e07ce365903116b62ac3ac0a033167853853074313f443d5b372f0225eede50 "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
f88600f4b65211a95c6817d0840e0fc2d422883ddf310f29fa8d4cbfda962626 "The quick brown fox jumps over the lazy dog"
ec05208dd1fbf47b9539a761af723612eaa810762ab7a77b715fcfb3bf44f04a "The quick brown fox jumps over the lazy cog"
822578f80d46184a674a6069486b4594053490de8ddf343cc1706418e527bec8 "The quick brown fox jumps over the lazy dog."
410427b981efa6ef884cd1f3d812c880bc7a37abc7450dd62803a4098f28d0f1 "After the rainstorm comes the rainbow."
47b5d8cb1df8d81ed23689936d2edaa7bd5c48f5bc463600a4d7a56342ac80b9 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
```

## Building and Installing

You can build and install the `rainsum` utility using the provided Makefile. First, clone the repository and change into the root directory of the project:

```sh
git clone https://github.com/dosyago/rain
cd rain
```

Then, build the utility with make:

```sh
make
```

If you want to install `rainsum` globally, so it can be run from any directory, use the `make install` command:

```sh
make install
```

This command might require administrator privileges, depending on your system's configuration. If you encounter a permission error, try using `sudo`:

```sh
sudo make install
```

After installation, you can run `rainsum` from any directory:

```sh
rainsum --help
```

See the [Field Manual](#Rainsum-Field-Manual) for more information on usage. 

## Contributions

We warmly welcome any analysis, along with faster implementations or suggested modifications. Collaboration is highly encouraged!
