#!/bin/bash

# First compile using qbe test.qbe > test.S
# Then compile the generated assembly code using gcc test.S -o test
# Run the compiled program using ./test and log the output to the console
qbe test.qbe > test.S
gcc test.S -o test

# Print output using echo -e to enable interpretation of backslash escapes
echo -e "Running the compiled program...\n"
output=$(./test)
echo -e "Program output:\n$output"

echo -e "Exit code: $?"
