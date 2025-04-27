#!/bin/sh
set -e

: "${TAR:=tar}"

cd "$(dirname "$0")"

PV=28209
LIB_VERSION=${PV}+wfg1

fetch()
{
	rm -Rf nvtt-${PV}
	svn export https://svn.wildfiregames.com/public/source-libs/trunk/nvtt@${PV} nvtt-${PV}
	"${TAR}" cJf nvtt-${PV}.tar.xz nvtt-${PV}
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
"${TAR}" xf nvtt-${PV}.tar.xz

# patch
patch -d nvtt-${PV} -p1 <patches/0001-Don-t-overspecify-flags.patch
patch -d nvtt-${PV} -p1 <patches/0002-Bump-cmake-min-version-to-3.10.patch
patch -d nvtt-${PV} -p1 <patches/0003-Use-execute_process-insted-of-exec_program.patch

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
