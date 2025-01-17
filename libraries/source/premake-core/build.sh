#!/bin/sh
set -e

: "${OS:=$(uname -s || true)}"
: "${MAKE:=make}"
: "${JOBS:=-j1}"

cd "$(dirname "$0")"

PV=5.0.0-beta3
LIB_VERSION=${PV}+wfg1

fetch()
{
	curl -fLo "premake-core-${PV}.tar.gz" \
		"https://github.com/premake/premake-core/archive/refs/tags/v${PV}.tar.gz"
}

echo "Building Premake..."
while [ "$#" -gt 0 ]; do
	case "$1" in
		--fetch-only)
			fetch
			exit
			;;
		--force-rebuild) rm -f .already-built ;;
		*)
			echo "Unknown option: $1"
			exit 1
			;;
	esac
	shift
done

if [ -e .already-built ] && [ "$(cat .already-built || true)" = "${LIB_VERSION}" ]; then
	echo "Skipping - already built (use --force-rebuild to override)"
	exit
fi

# fetch
if [ ! -e "premake-core-${PV}.tar.gz" ]; then
	fetch
fi

# unpack
rm -Rf "premake-core-${PV}"
tar -xf "premake-core-${PV}.tar.gz"

# patch
# ffcb7790f013bdceacc14ba5fda1c5cd107aac08
patch -d "premake-core-${PV}" -p1 <patches/0001-Use-_SC_NPROCESSORS_ONLN-for-CPU-detection-in-BSDs.-.patch
# https://github.com/premake/premake-core/issues/2338
patch -d "premake-core-${PV}" -p1 <patches/0002-Make-clang-default-toolset-for-BSD.patch
# 82c9d90495940e2d0d574e1c7849e9698f23b090
patch -d "premake-core-${PV}" -p1 <patches/0003-Add-support-for-riscv64-2356.patch
# 928397f72c00979d57ec4688cb1fb26ec7f2449b
patch -d "premake-core-${PV}" -p1 <patches/0004-Add-support-for-loongarch64-2363.patch
# 5c524b6d53307bcb4ba7b02c9dba20100df68943
patch -d "premake-core-${PV}" -p1 <patches/0005-premake.h-added-e2k-definition-2349.patch

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
