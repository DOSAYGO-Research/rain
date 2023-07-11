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

The hashes' stability may change over time, as we might modify constants, mixing specifics, and more as we gather insights. Should such changes alter the hashes' output, we will denote the changes with new version numbers. As of now, Rainstorm is at v0.0.1, and Rainbow is at v1.0.3.

## Test vectors

The current test vectors for Rainstorm (v0.0.1) and Rainbow (v1.0.3) are:

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

**Rainstorm v0.0.2 Test Vectors**

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


