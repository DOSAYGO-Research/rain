#!/bin/bash

# Function to calculate and print hash
calculate_hash() {
    input=$1
    hash=$(echo "$input" | ./../src/bin/rainsum -s 256 | cut -d ' ' -f1 | tee /dev/stderr)
    echo $hash
}

# Initial input
input=""

# Get n from the command-line arguments, default to 50000
n=${1:-50000}

if [ "$n" == "-" ]; then
    # Infinite loop
    while true; do
        input=$(calculate_hash $input)
    done
else
    # Loop for n times
    for ((i=0; i<n; i++)); do
        input=$(calculate_hash $input)
    done
fi

