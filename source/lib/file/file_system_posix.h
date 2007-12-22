/**
 * =========================================================================
 * File        : file_system_posix.h
 * Project     : 0 A.D.
 * Description : file layer on top of POSIX. avoids the need for
 *             : absolute paths and provides fast I/O.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_FILE_SYSTEM_POSIX
#define INCLUDED_FILE_SYSTEM_POSIX

#include "lib/file/path.h"
#include "lib/file/file_system.h"

// jw 2007-12-20: we'd love to replace this with boost::filesystem,
// but basic_directory_iterator does not yet cache file_size and
// last_write_time in file_status. (they each entail a stat() call,
// which is unacceptably slow.)

struct FileSystem_Posix
{
	virtual LibError GetFileInfo(const Path& pathname, FileInfo* fileInfo) const;
	virtual LibError GetDirectoryEntries(const Path& path, FileInfos* files, DirectoryNames* subdirectoryNames) const;

	LibError DeleteDirectory(const Path& dirPath);
};

#endif	// #ifndef INCLUDED_FILE_SYSTEM_POSIX
