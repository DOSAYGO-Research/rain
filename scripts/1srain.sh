#!/usr/bin/env bash

size=${1:-256}
alg=${2:-storm}
./../src/bin/rainsum -a $alg -s $size -l 400000000 -m stream rainsum.cpp  | xxd -b -c 256 | grep -o '[01]' | tr -d '\n' | tr '0' ' '

