# Rain

This repository includes the rainbow and rainstorm hash functions. They were developed by Cris Stringfellow and are licensed under Apache-2.0. The 64-bit variants have been tested and pass all tests in [SMHasher3](https://gitlab.com/fwojcik/smhasher3). [Results](results) are in the `results/` subdirectory.

| Algorithm | Speed | Hash Size | Purpose | Core Mixing Function | Security |
| :- | :- | :- | :- | :- | :- |
| Rainbow | 13.2 GiB/sec | 64 to 256 bits | General purpose non-cryptographic hashing | Multiplication, subtraction/addition, rotation, XOR | None mentioned |
| Rainstorm | 4.2 GiB/sec (with 4 rounds) | 64 to 512 bits | Secure cryptographic hashing (use at your own risk) | Addition/subtraction, rotation, XOR | Not analyzed, no guarantees |

## Rainbow

Fast hash (13.2 GiB/sec, 4.61 bytes/cycle on long messages, 24.8 cycles/hash for short messages), meant for general purpose non-cryptographic hashing. The core mixing function uses multiplication, subtraction/addition, rotation and XOR. 

## Rainstorm

Slow hash with a tuneable-round function (with 4 rounds runs at 4.2 GiB/sec), meant for secure cryptographic hashing but it has not be analyzed so it has no security gurauntees at this time, use at your own risk. The core mixing function uses addition/subtraction, rotation and XOR.



