#!/bin/sh
set -e

cd "$(dirname "$0")"

LIB_VERSION=28209

if [ -e .already-built ] && [ "$(cat .already-built)" = "${LIB_VERSION}" ]; then
	echo "Spidermonkey is already up to date."
	exit
fi

# fetch
svn co "https://svn.wildfiregames.com/public/source-libs/trunk/spidermonkey@${LIB_VERSION}" spidermonkey-svn

# unpack
rm -Rf spidermonkey-build
cp -R spidermonkey-svn spidermonkey-build

# build
(
	cd spidermonkey-build
	mkdir bin lib
	./build.sh
)

# install
rm -Rf bin include-unix-debug include-unix-release lib
cp -R spidermonkey-build/bin spidermonkey-build/include-unix-release spidermonkey-build/lib .
if [ "$(uname -s)" != "FreeBSD" ]; then
	cp -R spidermonkey-build/include-unix-debug .
fi

echo "${LIB_VERSION}" >.already-built
