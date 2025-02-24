#!/bin/sh
set -e

: "${TAR:=tar}"

cd "$(dirname "$0")"

# This should match the version in config/milestone.txt
FOLDER="mozjs-115.16.1"
# If same-version changes are needed, increment this.
LIB_VERSION="115.16.1+5"
LIB_NAME="mozjs115"

fetch()
{
	curl -fLo "${FOLDER}.tar.xz" \
		"https://releases.wildfiregames.com/libs/${FOLDER}.tar.xz"
}

echo "Building SpiderMonkey..."
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

OS="${OS:=$(uname -s)}"

# fetch
# This tarball is built from https://ftp.mozilla.org/pub/firefox/releases/115.16.1esr/source/
# by running js/src/make-source-package.py
if [ ! -e "${FOLDER}.tar.xz" ]; then
	fetch
fi

# unpack
rm -Rf "${FOLDER}"
"${TAR}" xfJ "${FOLDER}.tar.xz"

# patch
(
	cd "${FOLDER}"

	# Fix build on recent macOS versions.
	# https://bugzilla.mozilla.org/show_bug.cgi?id=1844694
	# The --enable-bootstrap fix mentioned on the ticket does not fix the issue. Instead,
	# use Homebrew's workaround.
	if [ "${OS}" = "Darwin" ]; then
		sed -i '' 's/\["-Wl,--version"]/["-Wl,-ld_classic,--version"]/g' build/moz.configure/toolchain.configure
	fi

	# shellcheck disable=SC1091
	. ../patches/patch.sh
)

# build
(
	cd "${FOLDER}"

	# Silence notifications and warnings from the Mozilla build system
	export MOZ_NOSPAM=1
	export CFLAGS="${CFLAGS} -w"
	export CXXFLAGS="${CXXFLAGS} -w"

	# Do not let mach fetch python and rust packages online: use vendored content
	export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE="none"

	# Do not let mach install tools in the user's home, create a disposable folder
	export MOZBUILD_STATE_PATH="${MOZBUILD_STATE_PATH:=$(pwd)/mozbuild-state}"

	if [ -n "$PROFILE" ]; then
		CONF_OPTS="$CONF_OPTS
			--enable-profiling
			--enable-perf
			--enable-jitspew
			--with-jitreport-granularity=3"

		if [ "${OS}" = "Darwin" ]; then
			CONF_OPTS="$CONF_OPTS
				--enable-instruments"
		fi
	fi

	if [ "${OS}" = "Windows_NT" ]; then
		CONF_OPTS="${CONF_OPTS} --target=i686"
	elif [ "${OS}" = "Darwin" ]; then
		# Unless we are forcing an architecture, switch between ARM / x86 based on platform.
		if [ -z "${ARCH}" ]; then
			if [ "$(uname -m)" = "arm64" ]; then
				ARCH="aarch64"
			else
				ARCH="x86_64"
			fi
		fi
		CONF_OPTS="${CONF_OPTS} --target=$ARCH-apple-darwin"

		# Link to custom-built zlib
		export PKG_CONFIG_PATH="=${ZLIB_DIR}:${PKG_CONFIG_PATH}"
		CONF_OPTS="${CONF_OPTS} --with-system-zlib"
		# Specify target versions and SDK
		if [ "${MIN_OSX_VERSION}" ] && [ "${MIN_OSX_VERSION-_}" ]; then
			CONF_OPTS="${CONF_OPTS} --enable-macos-target=$MIN_OSX_VERSION"
		fi
		if [ "${SYSROOT}" ] && [ "${SYSROOT-_}" ]; then
			CONF_OPTS="${CONF_OPTS} --with-macos-sdk=${SYSROOT}"
		fi
	fi

	# If Valgrind looks like it's installed, then set up SM to support it
	# (else the JITs will interact poorly with it)
	if [ -e /usr/include/valgrind/valgrind.h ]; then
		CONF_OPTS="${CONF_OPTS} --enable-valgrind"
	fi

	# We need to be able to override CHOST in case it is 32bit userland on 64bit kernel
	CONF_OPTS="${CONF_OPTS} \
		${CBUILD:+--build=${CBUILD}} \
		${CHOST:+--host=${CHOST}} \
		${CTARGET:+--target=${CTARGET}}"

	# Build
	# Debug (broken on FreeBSD)
	if [ "${OS}" != "FreeBSD" ]; then
		# shellcheck disable=SC2086
		MOZCONFIG="$(pwd)/../mozconfig" \
		MOZCONFIG_OPTIONS="${CONF_OPTS} \
			--enable-debug \
			--disable-optimize \
			--enable-gczeal" \
		BUILD_DIR="build-debug" \
			./mach build ${JOBS}
	fi
	# Release
	# shellcheck disable=SC2086
	MOZCONFIG="$(pwd)/../mozconfig" \
	MOZCONFIG_OPTIONS="${CONF_OPTS} \
		--enable-optimize" \
	BUILD_DIR="build-release" \
		./mach build ${JOBS}
)

# install
rm -Rf bin include-debug include-release lib

mkdir bin lib

if [ "${OS}" = "Windows_NT" ]; then
	LIB_PREFIX=
	LIB_SUFFIX=.dll
	STATIC_LIB_SUFFIX=.lib
else
	LIB_PREFIX=lib
	LIB_SUFFIX=.so
	STATIC_LIB_SUFFIX=.a
	if [ "${OS}" = "OpenBSD" ]; then
		LIB_SUFFIX=.so.1.0
	elif [ "${OS}" = "Darwin" ]; then
		LIB_SUFFIX=.a
	fi
fi

if [ "${OS}" = "Windows_NT" ]; then
	# Bug #776126
	# SpiderMonkey uses a tweaked zlib when building, and it wrongly copies its own files to include dirs
	# afterwards, so we have to remove them to not have them conflicting with the regular zlib
	(
		cd "${FOLDER}"/build-debug/dist/include
		rm -f mozzconf.h zconf.h zlib.h
	)
	(
		cd "${FOLDER}"/build-release/dist/include
		rm -f mozzconf.h zconf.h zlib.h
	)

	# SpiderMonkey can be linked/included in projects built with MSVC, however, since clang is now the only
	# supported compiler on Windows, the codebase has accumulated some divergences with MSVC.
	# Upstream tries on a best-effort basis to keep the SM headers MSVC-compatible.
	patch -d "${FOLDER}"/build-debug/dist/include -p1 <patches/FixHeadersForMSVC.diff
	patch -d "${FOLDER}"/build-release/dist/include -p1 <patches/FixHeadersForMSVC.diff
fi

# js-config.h is different for debug and release builds, so we need different include directories for both
mkdir include-release
cp -R -L "${FOLDER}"/build-release/dist/include/* include-release/

if [ "${OS}" != "FreeBSD" ]; then
	mkdir include-debug
	cp -R -L "${FOLDER}"/build-debug/dist/include/* include-debug/
fi

# These align the ligns below, making it easier to check for mistakes.
DEB="debug"
REL="release"

# Fetch the jsrust static library. Path is grepped from the build file as it varies by rust toolset.
rust_path=$(grep jsrust <"${FOLDER}/build-release/js/src/build/backend.mk" | cut -d = -f 2 | cut -c2-)
cp -L "${rust_path}" "lib/${LIB_PREFIX}${LIB_NAME}-rust${STATIC_LIB_SUFFIX}"

if [ "${OS}" = "Darwin" ]; then
	# On MacOS, copy the static libraries only.
	cp -L "${FOLDER}/build-${DEB}/js/src/build/${LIB_PREFIX}js_static${LIB_SUFFIX}" "lib/${LIB_PREFIX}${LIB_NAME}-${DEB}${LIB_SUFFIX}"
	cp -L "${FOLDER}/build-${REL}/js/src/build/${LIB_PREFIX}js_static${LIB_SUFFIX}" "lib/${LIB_PREFIX}${LIB_NAME}-${REL}${LIB_SUFFIX}"
elif [ "${OS}" = "Windows_NT" ]; then
	# Windows needs DLLs to binaries/, static stubs to lib/ and debug symbols
	cp -L "${FOLDER}/build-${DEB}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${DEB}${LIB_SUFFIX}" "bin/${LIB_PREFIX}${LIB_NAME}-${DEB}${LIB_SUFFIX}"
	cp -L "${FOLDER}/build-${REL}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${REL}${LIB_SUFFIX}" "bin/${LIB_PREFIX}${LIB_NAME}-${REL}${LIB_SUFFIX}"
	cp -L "${FOLDER}/build-${DEB}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${DEB}${STATIC_LIB_SUFFIX}" "lib/${LIB_PREFIX}${LIB_NAME}-${DEB}${STATIC_LIB_SUFFIX}"
	cp -L "${FOLDER}/build-${REL}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${REL}${STATIC_LIB_SUFFIX}" "lib/${LIB_PREFIX}${LIB_NAME}-${REL}${STATIC_LIB_SUFFIX}"
	# Copy debug symbols as well.
	cp -L "${FOLDER}/build-${DEB}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${DEB}.pdb" "bin/${LIB_PREFIX}${LIB_NAME}-${DEB}.pdb"
	cp -L "${FOLDER}/build-${REL}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${REL}.pdb" "bin/${LIB_PREFIX}${LIB_NAME}-${REL}.pdb"
	# Copy the debug jsrust library.
	rust_path=$(grep jsrust <"${FOLDER}/build-debug/js/src/build/backend.mk" | cut -d = -f 2 | cut -c2-)
	cp -L "${rust_path}" "lib/${LIB_PREFIX}${LIB_NAME}-rust-debug${STATIC_LIB_SUFFIX}"
else
	# Copy shared libs to lib/, they will also be copied to binaries/system, so the compiler and executable (resp.) can find them.
	cp -L "${FOLDER}/build-${REL}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${REL}${LIB_SUFFIX}" "lib/${LIB_PREFIX}${LIB_NAME}-${REL}${LIB_SUFFIX}"
	if [ "${OS}" != "FreeBSD" ]; then
		cp -L "${FOLDER}/build-${DEB}/js/src/build/${LIB_PREFIX}${LIB_NAME}-${DEB}${LIB_SUFFIX}" "lib/${LIB_PREFIX}${LIB_NAME}-${DEB}${LIB_SUFFIX}"
	fi
fi

# cleanup
rm -Rf "$FOLDER"

echo "${LIB_VERSION}" >.already-built
