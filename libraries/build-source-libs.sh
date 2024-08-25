#!/bin/sh

die()
{
	echo ERROR: $*
	exit 1
}

# SVN revision to checkout for source-libs
# Update this line when you commit an update to source-libs
source_svnrev="28207"

if [ "$(uname -s)" = "Darwin" ]; then
	echo 'This script should not be used on macOS: use build-macos-libs.sh instead.'
	exit 1
fi

cd "$(dirname $0)"
# Now in libraries/ (where we assume this script resides)

# Check for whitespace in absolute path; this will cause problems in the
# SpiderMonkey build and maybe elsewhere, so we just forbid it
# Use perl as an alternative to readlink -f, which isn't available on BSD
SCRIPTPATH=$(perl -MCwd -e 'print Cwd::abs_path shift' "$0")
case "$SCRIPTPATH" in
	*\ *)
		die "Absolute path contains whitespace, which will break the build - move the game to a path without spaces"
		;;
esac

# Parse command-line options (download options and build options):
source_libs_dir="source"

without_nvtt=false
with_system_nvtt=false
with_system_mozjs=false

JOBS=${JOBS:="-j2"}

for i in "$@"; do
	case $i in
		--source-libs-dir=*) source_libs_dir=${1#*=} ;;
		--source-libs-dir) die "correct syntax is --source-libs-dir=/path/to/dir" ;;
		--without-nvtt) without_nvtt=true ;;
		--with-system-nvtt) with_system_nvtt=true ;;
		--with-system-mozjs) with_system_mozjs=true ;;
		-j*) JOBS=$i ;;
	esac
done

# Download source libs
echo "Downloading source libs..."
echo
if [ -e ${source_libs_dir}/.svn ]; then
	(cd ${source_libs_dir} && svn cleanup && svn up -r $source_svnrev)
else
	svn co -r $source_svnrev https://svn.wildfiregames.com/public/source-libs/trunk ${source_libs_dir}
fi

# Build/update bundled external libraries
echo "Building third-party dependencies..."
echo

# Some of our makefiles depend on GNU make, so we set some sane defaults if MAKE
# is not set.
case "$(uname -s)" in
	"FreeBSD" | "OpenBSD")
		MAKE=${MAKE:="gmake"}
		;;
	*)
		MAKE=${MAKE:="make"}
		;;
esac

(cd ${source_libs_dir}/fcollada && MAKE=${MAKE} JOBS=${JOBS} ./build.sh) || die "FCollada build failed"
echo
if [ "$with_system_nvtt" = "false" ] && [ "$without_nvtt" = "false" ]; then
	(cd ${source_libs_dir}/nvtt && MAKE=${MAKE} JOBS=${JOBS} ./build.sh) || die "NVTT build failed"
fi
echo
if [ "$with_system_mozjs" = "false" ]; then
	(cd ${source_libs_dir}/spidermonkey && MAKE=${MAKE} JOBS=${JOBS} ./build.sh) || die "SpiderMonkey build failed"
fi
echo

echo "Copying built files..."
# Copy built binaries to binaries/system/
cp ${source_libs_dir}/fcollada/bin/* ../binaries/system/
if [ "$with_system_nvtt" = "false" ] && [ "$without_nvtt" = "false" ]; then
	cp ${source_libs_dir}/nvtt/bin/* ../binaries/system/
fi
if [ "$with_system_mozjs" = "false" ]; then
	cp ${source_libs_dir}/spidermonkey/bin/* ../binaries/system/
fi
# If a custom source-libs dir was used, includes and static libs should be copied to libraries/source/
# and all other bundled content should be copied.
if [ "$source_libs_dir" != "source" ]; then
	rsync -avzq \
		--exclude fcollada \
		--exclude nvtt \
		--exclude spidermonkey \
		${source_libs_dir}/ source

	mkdir -p source/fcollada
	cp -r ${source_libs_dir}/fcollada/{include,lib} source/fcollada/
	if [ "$with_system_nvtt" = "false" ] && [ "$without_nvtt" = "false" ]; then
		mkdir -p source/nvtt
		cp -r ${source_libs_dir}/nvtt/{include,lib} source/nvtt/
	fi
	if [ "$with_system_mozjs" = "false" ]; then
		mkdir -p source/spidermonkey
		cp -r ${source_libs_dir}/spidermonkey/{include-unix-debug,include-unix-release,lib} source/spidermonkey/
	fi
fi
echo "Done."
