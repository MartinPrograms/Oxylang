#!/bin/bash

OS=$(uname -s)
ARCH=$(uname -m)

qbe test.qbe > test.s || exit 1

if [ "$OS" = "Darwin" ] && [ "$ARCH" = "arm64" ]; then
    gcc test.s -o test \
        -L/opt/homebrew/lib \
        -lraylib \
        -framework CoreVideo \
        -framework IOKit \
        -framework Cocoa \
        -framework GLUT \
        -framework OpenGL \
        -lm -lpthread -ldl || exit 1

elif [ "$OS" = "Linux" ] && [ "$ARCH" = "x86_64" ]; then
    gcc test.s -o test \
        -lraylib -lm -lpthread -ldl -lrt -lX11 || exit 1

else
    echo "Unsupported platform: $OS $ARCH"
    exit 1
fi

./test
echo "Exit code: $?"