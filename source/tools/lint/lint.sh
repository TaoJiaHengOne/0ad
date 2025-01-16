#!/bin/sh
set -e

cd "$(dirname "$0")"

has_errors=false

if command -v cppcheck >/dev/null; then
	./cppcheck/cppcheck.sh || has_errors=true
else
	echo "Cppcheck not found in path"
fi

if [ ${has_errors} = true ]; then
	exit 1
fi
