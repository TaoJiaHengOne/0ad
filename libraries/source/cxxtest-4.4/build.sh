#!/bin/sh
set -e

cd "$(dirname "$0")"

LIB_VERSION=28209

if [ -e .already-built ] && [ "$(cat .already-built)" = "${LIB_VERSION}" ]; then
	echo "cxxtest-4.4 is already up to date."
	exit
fi

# fetch
svn co https://svn.wildfiregames.com/public/source-libs/trunk/cxxtest-4.4@${LIB_VERSION} cxxtest-4.4-svn

# unpack
rm -Rf cxxtest-4.4-build
cp -R cxxtest-4.4-svn cxxtest-4.4-build

# nothing to actually build
# built as part of building tests

# install
rm -Rf bin cxxtest python
cp -R cxxtest-4.4-build/bin cxxtest-4.4-build/cxxtest cxxtest-4.4-build/python .

echo "${LIB_VERSION}" >.already-built
