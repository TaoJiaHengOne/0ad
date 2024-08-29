#!/bin/sh
set -e

cd "$(dirname "$0")"

LIB_VERSION=28209

if [ -e .already-built ] && [ "$(cat .already-built)" = "${LIB_VERSION}" ]; then
	echo "FCollada is already up to date."
	exit
fi

# fetch
svn co "https://svn.wildfiregames.com/public/source-libs/trunk/fcollada@${LIB_VERSION}" fcollada-svn

# unpack
rm -Rf fcollada-build
cp -R fcollada-svn fcollada-build

# build
(
	cd fcollada-build
	./build.sh
)

# install
rm -Rf include lib
cp -R fcollada-build/include fcollada-build/lib .

echo "${LIB_VERSION}" >.already-built
