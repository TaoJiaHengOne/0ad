#!/usr/bin/env python3
#
# Copyright (C) 2024 Wildfire Games.
# This file is part of 0 A.D.
#
# 0 A.D. is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# 0 A.D. is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.

# ruff: noqa: E741

import io
import os
import subprocess
from typing import List

from i18n_helper import projectRootDirectory


def get_diff():
    """Return a diff using svn diff"""
    os.chdir(projectRootDirectory)

    diff_process = subprocess.run(["svn", "diff", "binaries"], capture_output=True, check=False)
    if diff_process.returncode != 0:
        print(f"Error running svn diff: {diff_process.stderr.decode('utf-8')}. Exiting.")
        return None
    return io.StringIO(diff_process.stdout.decode("utf-8"))


def check_diff(diff: io.StringIO, verbose=False) -> List[str]:
    """Run through a diff of .po files and check that some of the changes
    are real translations changes and not just noise (line changes....).
    The algorithm isn't extremely clever, but it is quite fast.
    """
    keep = set()
    files = set()

    curfile = None
    l = diff.readline()
    while l:
        if l.startswith("Index: binaries"):
            if not l.strip().endswith(".pot") and not l.strip().endswith(".po"):
                curfile = None
            else:
                curfile = l[7:].strip()
                files.add(curfile)
            # skip patch header
            diff.readline()
            diff.readline()
            diff.readline()
            diff.readline()
            l = diff.readline()
            continue
        if l[0] != "-" and l[0] != "+":
            l = diff.readline()
            continue
        if l[1:].strip() == "" or (l[1] == "#" and l[2] == ":"):
            l = diff.readline()
            continue
        if (
            "# Copyright (C)" in l
            or "POT-Creation-Date:" in l
            or "PO-Revision-Date" in l
            or "Last-Translator" in l
        ):
            l = diff.readline()
            continue
        # We've hit a real line
        if curfile:
            keep.add(curfile)
            curfile = None
        l = diff.readline()

    return list(files.difference(keep))


def revert_files(files: List[str], verbose=False):
    revert_process = subprocess.run(["svn", "revert", *files], capture_output=True, check=False)
    if revert_process.returncode != 0:
        print(
            "Warning: Some files could not be reverted. "
            f"Error: {revert_process.stderr.decode('utf-8')}"
        )
    if verbose:
        for file in files:
            print(f"Reverted {file}")


def add_untracked(verbose=False):
    """Add untracked .po files to svn"""
    diff_process = subprocess.run(["svn", "st", "binaries"], capture_output=True, check=False)
    if diff_process.stderr != b"":
        print(f"Error running svn st: {diff_process.stderr.decode('utf-8')}. Exiting.")
        return

    for line in diff_process.stdout.decode("utf-8").split("\n"):
        if not line.startswith("?"):
            continue
        # Ignore non PO files. This is important so that the translator credits
        # correctly be updated, note however the script assumes a pristine SVN otherwise.
        file = line[1:].strip()
        if not file.endswith(".po") and not file.endswith(".pot"):
            continue
        add_process = subprocess.run(
            ["svn", "add", file, "--parents"], capture_output=True, check=False
        )
        if add_process.stderr != b"":
            print(f"Warning: file {file} could not be added.")
        if verbose:
            print(f"Added {file}")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", help="Print reverted files.", action="store_true")
    args = parser.parse_args()
    need_revert = check_diff(get_diff(), args.verbose)
    revert_files(need_revert, args.verbose)
    add_untracked(args.verbose)
