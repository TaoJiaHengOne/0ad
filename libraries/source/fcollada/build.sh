#!/bin/sh
set -e

cd "$(dirname "$0")"

LIB_VERSION=28209

echo "Building FCollada..."
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
svn co "https://svn.wildfiregames.com/public/source-libs/trunk/fcollada@${LIB_VERSION}" fcollada-svn

# unpack
rm -Rf fcollada-build
cp -R fcollada-svn fcollada-build

# build
(
	cd fcollada-build
	./build.sh
)

# install
rm -Rf include lib
cp -R fcollada-build/include fcollada-build/lib .

echo "${LIB_VERSION}" >.already-built
