#!/bin/sh
set -e

cd "$(dirname "$0")"

PV=28209
LIB_VERSION=${PV}

fetch()
{
	rm -Rf fcollada-${PV}
	svn export https://svn.wildfiregames.com/public/source-libs/trunk/fcollada@${PV} fcollada-${PV}
	tar cJf fcollada-${PV}.tar.xz fcollada-${PV}
	rm -R fcollada-${PV}
}

echo "Building FCollada..."
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
if [ ! -e "fcollada-${PV}.tar.xz" ]; then
	fetch
fi

# unpack
rm -Rf fcollada-${PV}
tar xf fcollada-${PV}.tar.xz

# build
(
	cd fcollada-${PV}
	./build.sh
)

# install
rm -Rf include lib
cp -R fcollada-${PV}/include fcollada-${PV}/lib .

echo "${LIB_VERSION}" >.already-built
