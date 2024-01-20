# Comprehensive Overview of the Rainstorm Hash Function

## Abstract
The Rainstorm hash function, a creation of Cris at DOSYAGO in 2023, stands as a notable cryptographic hash function featuring a 1024-bit internal state and 512-bit blocks. Capable of producing outputs up to 512 bits, it doubles as an eXtensible Output Function (XOF) and a keystream generator. This paper presents a comprehensive overview of the Rainstorm algorithm, incorporating insights from recent empirical testing.

## Introduction
Critical to cryptographic applications, hash functions ensure data integrity, authentication, and non-repudiation. Rainstorm, with its novel approach, offers high security and efficiency. Its block-based mechanism leverages bitwise operations and modular arithmetic over prime numbers for robust cryptographic processing.

## Algorithm Overview
Rainstorm functions through 512-bit blocks while maintaining a 1024-bit internal state. Its design, incorporating selected primes and rotation amounts, aims for optimal avalanche properties. The algorithm's iterative nature involves multiple rounds of input data processing.

### Key Features:
- **Block Size**: 512 bits
- **Internal State**: 1024 bits
- **Output Size**: Up to 512 bits
- **Rounds**: 4 regular, 2 final per block
- **Primes and Rotations**: Prime numbers and specific rotation amounts for enhanced cryptographic security

## Implementation
Implemented in C++ for efficiency and portability, Rainstorm is optimized across various platforms, including Emscripten for WebAssembly. Inline functions and templates in its codebase contribute to its performance and flexibility.

## Empirical Testing and Cryptographic Properties
### Performance Analysis
Rainstorm's performance, as validated by SMHasher3 tests, showcases consistent efficiency across key sizes, averaging about 247 cycles per hash for small keys. In bulk speed tests, it achieved 1.92 GiB/sec at 3.5 GHz, demonstrating high throughput capabilities.

### Avalanche Effect
Testing confirmed Rainstorm's strong avalanche effect. Changes in input bits resulted in significant and unpredictable output changes, with bias percentages ranging from 0.632% to 0.877%, indicative of a robust avalanche effect.

### Collision Resistance
Rainstorm displayed excellent collision resistance in 'Zeroes', 'Cyclic', and 'Sparse' tests at higher bit levels (64-bit to 256-bit). Some deviations at lower bit levels (27-bit to 32-bit) were within acceptable ranges, affirming the algorithm's resistance to collision attacks.

### Bit Independence
The Bit Independence Criteria (BIC) tests showed low maximum bias values, demonstrating a high level of independence among output bits and reinforcing the algorithm's resilience against common cryptographic attacks.

## Conclusion
Integrating empirical testing results, the Rainstorm hash function emerges as a sophisticated, reliable candidate in the cryptographic domain. Its design demonstrates a thoughtful approach to achieving key cryptographic properties such as efficiency, strong avalanche effect, collision resistance, and bit independence. Continued evaluation and adaptation in response to new research are vital for its ongoing relevance and application in cryptographic solutions.


