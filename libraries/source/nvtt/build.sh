#!/bin/sh
set -e

cd "$(dirname "$0")"

PV=28209
LIB_VERSION=${PV}

fetch()
{
	rm -Rf nvtt-${PV}
	svn export https://svn.wildfiregames.com/public/source-libs/trunk/nvtt@${PV} nvtt-${PV}
	tar cJf nvtt-${PV}.tar.xz nvtt-${PV}
	rm -R nvtt-${PV}
}

echo "Building NVTT..."
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
if [ ! -e "nvtt-${PV}.tar.xz" ]; then
	fetch
fi

# unpack
rm -Rf nvtt-${PV}
tar xf nvtt-${PV}.tar.xz

# build
(
	cd nvtt-${PV}
	mkdir bin lib
	./build.sh
)

# install
rm -Rf bin include lib
cp -R nvtt-${PV}/bin nvtt-${PV}/include nvtt-${PV}/lib .

echo "${LIB_VERSION}" >.already-built
