#!/bin/sh

# This short script allows one to reset source libraries to a clean state without
# having to redownload everything.
# (We don't attempt to clean up every last file here - output in binaries/system/
# will still be there, etc. This is mostly just to quickly fix problems in the
# bundled dependencies.)

cd "$(dirname $0)"
# Now in libraries/ (where we assume this script resides)

echo "Cleaning bundled third-party dependencies..."

if [ -e source/.svn ]; then
    (cd source && svn revert -R . && svn st --no-ignore | cut -c 9- | xargs rm -rf)
else
    rm -rf source
fi

echo
echo "Done. Try running build-source-libs.sh again now."
