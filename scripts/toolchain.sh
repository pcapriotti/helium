#!/bin/bash -e

export PREFIX="$HOME/.local/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

export BINUTILS_VERSION="2.30"
export GCC_VERSION="9.1.0"

unset LIBRARY_PATH

if ! [ -f "binutils-${BINUTILS_VERSION}.tar.xz" ]; then
    wget "http://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
fi

if ! [ -d "binutils-${BINUTILS_VERSION}" ]; then
    tar xvJf "binutils-${BINUTILS_VERSION}.tar.xz"
fi

mkdir -p binutils-build
cd binutils-build
../binutils-${BINUTILS_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls -disable-werror
make ${MAKE_OPTS}
make install
cd ..

if ! [ -f "gcc-${GCC_VERSION}.tar.xz" ]; then
    wget "ftp://ftp.fu-berlin.de/unix/languages/gcc/releases/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
fi

if ! [ -d "gcc-${GCC_VERSION}" ]; then
    tar xvJf "gcc-${GCC_VERSION}.tar.xz"
fi

mkdir -p gcc-build
cd gcc-build
../gcc-${GCC_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make ${MAKE_OPTS} all-gcc
make ${MAKE_OPTS} all-target-libgcc
make install-gcc
