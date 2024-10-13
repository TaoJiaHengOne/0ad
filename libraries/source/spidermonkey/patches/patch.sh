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

# Fix Windows build environment under MozillaBuild 4, fixed in ESR 115
# https://bugzilla.mozilla.org/show_bug.cgi?id=1790540
patch -p1 <"${PATCHES}"/FixMozillaBuild4.diff

# Fix Windows build, fixed in ESR 115
# https://bugzilla.mozilla.org/show_bug.cgi?id=1802675
patch -p1 <"${PATCHES}"/FixWinHeap.diff

# Fix linking on Windows with mozglue, fixed in ESR 115
# https://bugzilla.mozilla.org/show_bug.cgi?id=1751561
patch -p1 <"${PATCHES}"/FixDllMain.diff

# There is an issue on 32-bit linux builds sometimes.
# NB: the patch here is Comment 21 modified by Comment 25
# but that seems to imperfectly fix the issue with GCC.
# It also won't compile on windows - in doubt, apply only where relevant.
# https://bugzilla.mozilla.org/show_bug.cgi?id=1729459
if [ "$(uname -m)" = "i686" ] && [ "${OS}" != "Windows_NT" ]; then
	patch -p1 <"${PATCHES}"/FixFpNormIssue.diff
fi
