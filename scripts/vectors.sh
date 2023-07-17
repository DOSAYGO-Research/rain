#!/usr/bin/env bash

echo "RAIN HASHES"
echo "C++ test vectors"
echo "Rainbow test vectors:"
./rainsum --test-vectors
echo "Rainstorm test vectors:"
./rainsum --test-vectors -a storm
echo "JavaScript/WASM test vectors"
echo "Rainbow test vectors:"
./js/rainsum.mjs --test-vectors
echo "Rainstorm test vectors:"
./js/rainsum.mjs --test-vectors -a storm
