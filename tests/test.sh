#!/bin/bash


file1="examples/$1.log"
file2="testing.log"

# Run program
./src/snob-factor < examples/$1.cmd | tee "$file2"


# Check if both files exist
if [ ! -e "$file1" ] || [ ! -e "$file2" ]; then
    echo -e "\e[31mFAILED:  Output File Missing!\e[0m"
    exit 1
fi

# Compare files
if cmp -s "$file1" "$file2"; then
    # Files are the same
    echo -e "\e[32mSUCCESS:  Outputs match!\e[0m"  # Green color
else
    # Files are different
    echo -e "\e[31mFAILED: Outputs do not match!\e[0m"  # Red color
fi
