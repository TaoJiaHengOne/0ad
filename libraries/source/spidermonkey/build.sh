#!/bin/sh
set -e

cd "$(dirname "$0")"

# This should match the version in config/milestone.txt
FOLDER="mozjs-91.13.1"
# If same-version changes are needed, increment this.
LIB_VERSION="91.13.1+2"
LIB_NAME="mozjs91"

if [ -e .already-built ] && [ "$(cat .already-built)" = "${LIB_VERSION}" ]; then
	echo "Spidermonkey is already up to date."
	exit
fi

OS="${OS:=$(uname -s)}"

# fetch
if [ ! -e "${FOLDER}.tar.xz" ]; then
	curl -fLo "${FOLDER}.tar.xz" \
		"https://svn.wildfiregames.com/public/source-libs/trunk/spidermonkey/${FOLDER}.tar.xz"
fi

# unpack
rm -Rf "${FOLDER}"
tar xfJ "${FOLDER}.tar.xz"

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

	# Prevent the SpiderMonkey build system from reading dependencies from
	# user-installed python packages.
	PYTHONNOUSERSITE=true

	# Use Mozilla make on Windows
	if [ "${OS}" = "Windows_NT" ]; then
		MAKE="mozmake"
	else
		MAKE=${MAKE:="make"}
	fi

	MAKE_OPTS="${JOBS}"

	# Standalone SpiderMonkey can not use jemalloc (see https://bugzilla.mozilla.org/show_bug.cgi?id=1465038)
	# Jitspew doesn't compile on VS17 in the zydis disassembler - since we don't use it, deactivate it.
	# Trace-logging doesn't compile for now.
	CONF_OPTS="--disable-tests
	           --disable-jemalloc
	           --disable-js-shell
	           --without-intl-api
	           --enable-shared-js"

	if [ -n "$PROFILE" ]; then
		CONF_OPTS="$CONF_OPTS --enable-profiling
		                      --enable-perf
		                      --enable-instruments
		                      --enable-jitspew
		                      --with-jitreport-granularity=3"
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

	LLVM_OBJDUMP=${LLVM_OBJDUMP:=$(command -v llvm-objdump || command -v objdump)}

	# Quick sanity check to print explicit error messages
	# (Don't run this on windows as it would likely fail spuriously)
	if [ "${OS}" != "Windows_NT" ]; then
		[ -n "$(command -v rustc)" ] || (echo "Error: rustc is not available. Install the rust toolchain (rust + cargo) before proceeding." && exit 1)
		[ -n "${LLVM_OBJDUMP}" ] || (echo "Error: LLVM objdump is not available. Install it (likely via LLVM-clang) before proceeding." && exit 1)
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

	echo "SpiderMonkey build options: ${CONF_OPTS}"

	# Build
	# Debug (broken on FreeBSD)
	if [ "${OS}" != "FreeBSD" ]; then
		mkdir -p build-debug
		(
			cd build-debug
			# llvm-objdump is searched for with the complete name, not simply 'objdump', account for that.
			CXXFLAGS="${CXXFLAGS}" ../js/src/configure \
				LLVM_OBJDUMP="${LLVM_OBJDUMP}" \
				${CONF_OPTS} \
				--enable-debug \
				--disable-optimize \
				--enable-gczeal
			${MAKE} ${MAKE_OPTS}
		)
	fi
	# Release
	mkdir -p build-release
	(
		cd build-release
		CXXFLAGS="${CXXFLAGS}" ../js/src/configure \
			LLVM_OBJDUMP="${LLVM_OBJDUMP}" \
			${CONF_OPTS} \
			--enable-optimize
		${MAKE} ${MAKE_OPTS}
	)
	
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
