#!/bin/sh

# build/workspaces/

start_dir=$(pwd)
premake_dir=$(pwd)/../premake
workspace_dir=$(pwd)/gcc

cd $premake_dir

# build/premake/

./premake --target gnu --outpath $workspace_dir --atlas $*

# These files need to be linked; premake makefiles assume that the
# lua file is accessible from the makefile directory

cd $workspace_dir
ln -f -s $premake_dir/premake.lua $premake_dir/functions.lua .
if [ -x $premake_dir/premake ]; then
	ln -f -s $premake_dir/premake .
fi

cd $start_dir
