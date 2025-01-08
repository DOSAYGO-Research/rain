#!/bin/bash

# Run the vectors script and save the output
./scripts/vectors.sh > actual_output.txt

valid=False
# Compare the actual output to the expected output
if diff -Z verification/vectors.txt actual_output.txt >/dev/null ; then
    echo "The outputs match."
    valid=True
else
    echo "The outputs do not match:"
    diff verification/vectors.txt actual_output.txt
fi

# Clean up
rm actual_output.txt

if $valid ; then
  exit 0
else
  exit 1
fi
