#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

file1="examples/$1.log"
file2="testing.log"

# Run program
./src/auto-snob examples/$1.{v,s} | tee "$file2"

$SCRIPT_DIR/compare.py "$file1" "$file2"
