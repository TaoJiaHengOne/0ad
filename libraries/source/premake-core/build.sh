#!/bin/sh
set -e

: "${OS:=$(uname -s || true)}"
: "${MAKE:=make}"
: "${JOBS:=-j1}"

cd "$(dirname "$0")"

PV=5.0.0-beta2
LIB_VERSION=${PV}+wfg2

if [ -e .already-built ] && [ "$(cat .already-built || true)" = "${LIB_VERSION}" ]; then
	echo "premake is already up to date."
	exit
fi

# fetch
if [ ! -e "premake-core-${PV}.tar.gz" ]; then
	curl -fLo "premake-core-${PV}.tar.gz" \
		"https://github.com/premake/premake-core/archive/refs/tags/v${PV}.tar.gz"
fi

# unpack
rm -Rf "premake-core-${PV}"
tar -xf "premake-core-${PV}.tar.gz"

#patch
patch -d "premake-core-${PV}" -p1 <patches/0001-Require-unistd.h-for-macosx-in-libzip.patch
patch -d "premake-core-${PV}" -p1 <patches/0002-Forceinclude-unistd.h-on-all-Unixes.patch
patch -d "premake-core-${PV}" -p1 <patches/0003-Add-support-for-idirafter-flag-in-GCC-Clang.patch

#build
(
	cd "premake-core-${PV}"
	case "${OS}" in
		Windows)
			${MAKE} "${JOBS}" -f Bootstrap.mak windows
			;;
		Darwin)
			${MAKE} "${JOBS}" -f Bootstrap.mak osx
			;;
		*BSD)
			${MAKE} "${JOBS}" -f Bootstrap.mak bsd
			;;
		*)
			${MAKE} "${JOBS}" -f Bootstrap.mak linux
			;;
	esac
)

# install
rm -Rf bin
mkdir -p bin
cp "premake-core-${PV}/bin/release/premake5" bin/

echo "${LIB_VERSION}" >.already-built
