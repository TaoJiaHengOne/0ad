#!/bin/sh
set -ev

XZOPTS="-9 -e"
GZIP7ZOPTS="-mx=9"

BUNDLE_VERSION=${BUNDLE_VERSION:="0.27.0dev"}
PREFIX="0ad-${BUNDLE_VERSION}"

# Collect the relevant files
tar cf "$PREFIX"-unix-build.tar \
	--exclude='*.bat' --exclude='*.dll' --exclude='*.exe' --exclude='*.lib' --exclude='*.pdb' \
	--exclude='libraries/win32' \
	-s "|.|$PREFIX/~|" \
	source build libraries binaries/system/readme.txt binaries/data/l10n binaries/data/tests binaries/data/mods/_test.* ./*.txt

tar cf "$PREFIX"-unix-data.tar \
	--exclude='binaries/data/config/dev.cfg' \
	-s "|archives|$PREFIX/binaries/data/mods|" \
	-s "|binaries|$PREFIX/binaries|" \
	binaries/data/config binaries/data/tools archives/
# TODO: ought to include generated docs in here, perhaps?

# Compress
# shellcheck disable=SC2086
xz -kv ${XZOPTS} "$PREFIX"-unix-build.tar
# shellcheck disable=SC2086
xz -kv ${XZOPTS} "$PREFIX"-unix-data.tar
DO_GZIP=${DO_GZIP:=true}
if [ "$DO_GZIP" = true ]; then
	7zz a ${GZIP7ZOPTS} "$PREFIX"-unix-build.tar.gz "$PREFIX"-unix-build.tar
	7zz a ${GZIP7ZOPTS} "$PREFIX"-unix-data.tar.gz "$PREFIX"-unix-data.tar
fi

# Create Windows installer
# This needs nsisbi for files > 2GB
# nsisbi 3.09 and above fail to build on macOS, nsisbi 3.08.1 is used on the CD
# To build and install on macOS:
# - install mingw-w64 and scons with Homebrew
# - download the latest source at https://sourceforge.net/projects/nsisbi/files/
# - build with `scons SKIPUTILS="Makensisw","NSIS Menu","zip2exe"`
# - install with `sudo scons install SKIPUTILS="Makensisw","NSIS Menu","zip2exe"`
# Running makensis also needs a LANG workaround for https://sourceforge.net/p/nsis/bugs/1165/
LANG="en_GB.UTF-8" makensis -V4 -nocd \
	-dcheckoutpath="." \
	-dversion="${BUNDLE_VERSION}" \
	-dprefix="${PREFIX}" \
	-darchive_path="archives/" \
	source/tools/dist/0ad.nsi

# Fix permissions
chmod -f 644 "${PREFIX}-unix-build.tar.xz"
chmod -f 644 "${PREFIX}-unix-data.tar.xz"
chmod -f 644 "${PREFIX}-win32.exe"
if [ "$DO_GZIP" = true ]; then
	chmod -f 644 "${PREFIX}-unix-build.tar.gz"
	chmod -f 644 "${PREFIX}-unix-data.tar.gz"
fi
