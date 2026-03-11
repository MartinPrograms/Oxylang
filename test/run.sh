qbe test.qbe -o test.s || exit 1
gcc test.s -o test -lraylib -lm -lpthread -ldl -lrt -lX11 || exit 1
./test
echo "Exit code: $?"
