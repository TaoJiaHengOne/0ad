#!/bin/sh
set -e

cd "$(dirname "$0")"

LIB_VERSION=28209

if [ -e .already-built ] && [ "$(cat .already-built)" = "${LIB_VERSION}" ]; then
	echo "NVTT is already up to date."
	exit
fi

# fetch
svn co "https://svn.wildfiregames.com/public/source-libs/trunk/nvtt@${LIB_VERSION}" nvtt-svn

# unpack
rm -Rf nvtt-build
cp -R nvtt-svn nvtt-build

# build
(
	cd nvtt-build
	mkdir bin lib
	./build.sh
)

# install
rm -Rf bin include lib
cp -R nvtt-build/bin nvtt-build/include nvtt-build/lib .

echo "${LIB_VERSION}" >.already-built
