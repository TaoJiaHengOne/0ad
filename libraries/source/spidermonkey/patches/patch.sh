#!/bin/sh
set -e

OS="${OS:=$(uname -s)}"

# This script gets called from inside the extracted SM tarball.
PATCHES="../patches"

# The rust code is only linked if the JS Shell is enabled,
# which fails now that rust is required in all cases.
# https://bugzilla.mozilla.org/show_bug.cgi?id=1588340
patch -p1 <"${PATCHES}"/FixRustLinkage.diff

# Differentiate debug/release library names.
patch -p1 <"${PATCHES}"/FixLibNames.diff

# Add needed debug define in pkg-config file.
patch -p1 <"${PATCHES}"/FixPkgConfigDebug.diff

# On macOS, embedded build is broken due to a faulty check for pkg-config
# https://bugzilla.mozilla.org/show_bug.cgi?id=1776255
# The fix above is included in ESR 128, but does not apply to 115
# Instead, require pkg-config on macOS even though it's not needed here
# (from https://bugzilla.mozilla.org/show_bug.cgi?id=1783570)
patch -p1 <"${PATCHES}"/FixMacOSPkgConfig.diff

# There is an issue on 32-bit linux builds sometimes.
# NB: the patch here is Comment 21 modified by Comment 25
# but that seems to imperfectly fix the issue with GCC.
# It also won't compile on windows - in doubt, apply only where relevant.
# https://bugzilla.mozilla.org/show_bug.cgi?id=1729459
if [ "$(uname -m)" = "i686" ] && [ "${OS}" != "Windows_NT" ]; then
	patch -p1 <"${PATCHES}"/FixFpNormIssue.diff
fi

# Fix build with clang19
# https://bugzilla.mozilla.org/show_bug.cgi?id=1894423
# Fixed in ESR 128
patch -p1 <"${PATCHES}"/FixClang19.diff

# Fix build with Python >=3.12.8 and Python >=3.13.1
# https://bugzilla.mozilla.org/show_bug.cgi?id=1935621
patch -p1 <"${PATCHES}"/FixPython3.12.8.diff

# Fix build for profiling with samply.
# https://bugzilla.mozilla.org/show_bug.cgi?id=1876415
# Fixed in ESR 128
patch -p1 <"${PATCHES}"/FixProfiling.diff

# Supress warning on newer GCC compilers.
patch -p1 <"${PATCHES}"/SupressDanglingPointerWarning.patch
