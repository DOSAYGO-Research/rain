# Rain

This repository includes the rainbow and rainstorm hash functions. They were developed by Cris Stringfellow and are licensed under Apache-2.0. The 64-bit variants have been tested and pass all tests in [SMHasher3](https://gitlab.com/fwojcik/smhasher3). [Results](results) are in the `results/` subdirectory.

| Algorithm | Speed | Hash Size | Purpose | Core Mixing Function | Security |
| :- | :- | :- | :- | :- | :- |
| Rainbow | 13.2 GiB/sec | 64 to 256 bits | General purpose non-cryptographic hashing | Multiplication, subtraction/addition, rotation, XOR | None mentioned |
| Rainstorm | 4.7 GiB/sec (at 4 rounds) | 64 to 512 bits | Secure cryptographic hashing (use at your own risk) | Addition/subtraction, rotation, XOR | Not analyzed, no guarantees |

## Rainbow 

Fast hash (13.2 GiB/sec, 4.61 bytes/cycle on long messages, 24.8 cycles/hash for short messages), meant for general purpose non-cryptographic hashing. The core mixing function uses multiplication, subtraction/addition, rotation and XOR. 

## Rainstorm - **no security guarantees**

Slow hash with a tuneable-round function (with 4 rounds runs at 4.7 GiB/sec), meant for secure cryptographic hashing but it has not be analyzed so it has no security gurauntees at this time, use at your own risk. The core mixing function uses addition/subtraction, rotation and XOR.

Unoptimized rainstorm running at 4.7 GiB/sec is pretty fast for a cryptohash. If you want more "hypothetical security" (hypothetical right now as it's not analysed so we don't actually know if it's 'secure' or not!) you can apply more rounds. It doesn't have to be some special number, such as a power of two or even. You can apply any number of rounds. 

**Why say crypto if it's not analyzed?** 

It's intention is to be a cryptohash so it should be analyzed to verify and understand if it's secure and how secure. The inclusion of these words is to indicate the intention and invite people to analyze, break, or improve it. 

## Stability

The stability of these hashes is subject to change. We may update constants, mixing specifics and so on as more information becomes available. However if changes are made that result in different hashes, we will version the changes. Currently rainstorm is a v0, and rainbow is at v1.

## Contributions

Analysis is particularly welcome, as are faster implementations! Ideas for changes are also welcome, we're happy to collaborate. 

## Genesis

The core idea for mixing functions came from Discohash, but has been developed and extended. The overall architecture and processing flow of the hash was inspired by other hash functions.


