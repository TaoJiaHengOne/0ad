#!/bin/sh
set -e

cd "$(dirname "$0")"

LIB_VERSION=28209

echo "Building NVTT..."
while [ "$#" -gt 0 ]; do
	case "$1" in
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
