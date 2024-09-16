#!/bin/sh
set -e

cd "$(dirname "$0")"

PV=1.3.290.0
LIB_VERSION=${PV}

if [ -e .already-built ] && [ "$(cat .already-built || true)" = "${LIB_VERSION}" ]; then
	echo "spirv-reflect is already up to date."
	exit
fi

# fetch
if [ ! -e "vulkan-sdk-${PV}.tar.gz" ]; then
	curl -fLo "vulkan-sdk-${PV}.tar.gz" \
		"https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-${PV}.tar.gz"
fi

# unpack
rm -Rf "SPIRV-Reflect-vulkan-sdk-${PV}"
tar xf "vulkan-sdk-${PV}.tar.gz"

# configure
cmake -B build -S "SPIRV-Reflect-vulkan-sdk-${PV}" \
	-DCMAKE_INSTALL_PREFIX="$(realpath . || true)"

# build
cmake --build build

# install
rm -Rf bin
cmake --install build

echo "${LIB_VERSION}" >.already-built
