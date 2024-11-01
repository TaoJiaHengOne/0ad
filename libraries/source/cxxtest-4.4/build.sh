#!/bin/sh
set -e

cd "$(dirname "$0")"

PV=4.4
LIB_VERSION=${PV}+wfg1

if [ -e .already-built ] && [ "$(cat .already-built || true)" = "${LIB_VERSION}" ]; then
	echo "CxxTest is already up to date."
	exit
fi

# fetch
if [ ! -e "cxxtest-${PV}.tar.gz" ]; then
	curl -fLo "cxxtest-${PV}.tar.gz" \
		"https://github.com/CxxTest/cxxtest/releases/download/${PV}/cxxtest-${PV}.tar.gz"
fi

# unpack
rm -Rf "cxxtest-${PV}"
tar -xf "cxxtest-${PV}.tar.gz"

# patch
patch -d "cxxtest-${PV}" -p1 <patches/0001-Add-Debian-python3-patch.patch

# nothing to actually build
# built as part of building tests

# install
rm -Rf bin cxxtest python
cp -R "cxxtest-${PV}/bin" "cxxtest-${PV}/cxxtest" "cxxtest-${PV}/python" .

echo "${LIB_VERSION}" >.already-built
