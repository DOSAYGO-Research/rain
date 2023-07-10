# Rain

This repository houses the Rainbow and Rainstorm hash functions, developed by Cris Stringfellow and licensed under Apache-2.0. The 64-bit variants have passed all tests in the [SMHasher3](https://gitlab.com/fwojcik/smhasher3) suite. [Results](results) can be found in the `results/` subdirectory.

| Algorithm | Speed | Hash Size | Purpose | Core Mixing Function | Security |
| :- | :- | :- | :- | :- | :- |
| Rainbow | 13.2 GiB/sec | 64 to 256 bits | General-purpose non-cryptographic hashing | Multiplication, subtraction/addition, rotation, XOR | Not designed for cryptographic security |
| Rainstorm | 4.7 GiB/sec (at 4 rounds) | 64 to 512 bits | Potential cryptographic hashing | Addition/subtraction, rotation, XOR | No formal security analysis yet |

## Rainbow 

Rainbow is a fast hash function (13.2 GiB/sec, 4.61 bytes/cycle on long messages, 24.8 cycles/hash for short messages). It's intended for general-purpose, non-cryptographic hashing. The core mixing function utilizes multiplication, subtraction/addition, rotation, and XOR. 

## Rainstorm - **Unvetted for Security**

Rainstorm is a slower hash function with a tunable-round feature (with 4 rounds runs at 4.7 GiB/sec). It's designed with cryptographic hashing in mind, but it hasn't been formally analyzed for security, so we provide no guarantees. The core mixing function uses addition/subtraction, rotation, and XOR.

Rainstorm's round number is adjustable, potentially offering additional security. However, please note that this is hypothetical until rigorous security analysis is completed. 

## Note on Cryptographic Intent

While Rainstorm's design reflects cryptographic hashing principles, it has not been formally analyzed and thus, cannot be considered 'secure.' We strongly encourage those interested to conduct an analysis and offer feedback.

## Stability

The hashes' stability may change over time, as we might modify constants, mixing specifics, and more as we gather insights. Should such changes alter the hashes' output, we will denote the changes with new version numbers. As of now, Rainstorm is at v0, and Rainbow is at v1.

## Contributions

We warmly welcome any analysis, along with faster implementations or suggested modifications. Collaboration is highly encouraged!

## Genesis

The fundamental concept for the mixing functions derived from Discohash, but has been significantly developed and extended. The overall architecture and processing flow of the hash were inspired by existing hash functions.

---

