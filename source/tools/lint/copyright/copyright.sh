#!/bin/sh
set -e

cd "$(dirname "$0")/../../../.."

while [ "$#" -gt 0 ]; do
	case "$1" in
		--from)
			from_commitish=$2
			shift
			;;
		--to)
			to_commitish=$2
			git checkout --quiet "${to_commitish}"
			shift
			;;
		-j*) ;;
		*)
			printf "Unknown option: %s\n\n" "$1"
			exit 1
			;;
	esac
	shift
done

if [ -n "${from_commitish}" ]; then
	if [ -n "${to_commitish}" ]; then
		diff="${from_commitish}..${to_commitish}"
	else
		diff="${from_commitish}..$(git rev-parse HEAD)"
	fi
	printf "Running copyright linter for range\n%s\n\n" "${diff}"
fi

if [ -n "${diff}" ]; then
	for sha in $(git rev-list "${diff}"); do
		git diff-tree --no-commit-id --name-status -r "${sha}" |
			awk '!/^D/{$1=""; printf "%s\0", substr($0,2)}' |
			xargs -0 -L100 ./source/tools/lint/copyright/check_copyright_year.py
	done
else
	echo "WARNING: running copyright linter without base commit, likely not what you want."
	find . -type f -print0 |
		xargs -0 -L100 ./source/tools/lint/copyright/check_copyright_year.py
fi
