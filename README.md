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

The hashes' stability may change over time, as we might modify constants, mixing specifics, and more as we gather insights. Should such changes alter the hashes' output, we will denote the changes with new version numbers. As of now, Rainstorm is at v0.0.1, and Rainbow is at v1.0.1.

## Test vectors

The current test vectors for Rainstorm (v0.0.1) and Rainbow (v1.0.1) are:

**Rainbow v1.0.1 Test Vectors**

```test
f14c475b16f335b7087d8ca13ba624a8d18bd3b678e75373d980697c027b6fc2 ""
408e76ac2d5fcd5005ccfd759ace5251fc2eda0378891863910f49c68d559cc6 "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
72545293e6099750aa66b317722e72afc92a5b3e4ec5fd27812ebed3082327c7 "The quick brown fox jumps over the lazy dog"
da179c1578f4f5ad46e26f0c2b34061b54fa21b98a40cf9166a2806ace8b8631 "The quick brown fox jumps over the lazy cog"
0ab0b1e49a18724d995d70b2f449a8caa3f1a4240438ab83ad2277b0bb419065 "The quick brown fox jumps over the lazy dog."
297158e4ec29b1bdbad5b438679e74c9ad282dac08191c3a54b172ef2c6e51fd "After the rainstorm comes the rainbow."
d30e40ba5c9e6aa47e66b02f85ecdee1f3f01cb7febccb8a196eaeaa961390a1 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
```

**Rainstorm v0.0.1 Test Vectors**

```text
16bbf785885feae3cce7f078c5088d463a327ec231bdfe151ece5657c387ef79 ""
6b11035936ce079e781633a0c03aac623d443f317430855350deee25022f375b "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
a91152b6f40086f8c20f0e84d017685c290f31df3d8822d4262696dabf4c8dfa "The quick brown fox jumps over the lazy dog"
7bf4fbd18d2005ec123672af61a739957ba7b72a7610a8ea4af044bfb3cf5f71 "The quick brown fox jumps over the lazy cog"
4a18460df878258294456b4869604a673c34df8dde903405c8be27e5186470c1 "The quick brown fox jumps over the lazy dog."
efa6ef81b927044180c812d8f3d14c88d60d45c7ab377abcf1d0288f09a40328 "After the rainstorm comes the rainbow."
1ed8f81dcbd8b547a7da2e6d938936d2003646bcf5485cbdb980ac4263a5d7a4 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
```

## Building

```sh
git clone https://github.com/dosyago/rain
cd rain/src
make
```

## Contributions

We warmly welcome any analysis, along with faster implementations or suggested modifications. Collaboration is highly encouraged!

## Genesis

The fundamental concept for the mixing functions derived from Discohash, but has been significantly developed and extended. The overall architecture and processing flow of the hash were inspired by existing hash functions.


