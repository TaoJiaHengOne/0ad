#ifndef INCLUDED_WFILESYSTEM
#define INCLUDED_WFILESYSTEM

#include <sys/stat.h>

#include "no_crt_posix.h"


//
// sys/stat.h
//

// stat is defined by <sys/stat.h> (we allow this because VC8 declares
// inline macros that are worth keeping)

// defined by MinGW but not VC
#if MSC_VERSION
typedef unsigned int mode_t;
#endif

// mkdir is defined by posix_filesystem #if !HAVE_MKDIR

// (christmas-tree values because mkdir mode is ignored anyway)
#define S_IRWXO 0xFFFF
#define S_IRWXU 0xFFFF
#define S_IRWXG 0xFFFF

#define S_ISDIR(m) (m & S_IFDIR)
#define S_ISREG(m) (m & S_IFREG)


//
// dirent.h
//

typedef void DIR;

struct dirent
{
	// note: SUSv3 describes this as a "char array" but of unspecified size.
	// since that precludes using sizeof(), we may as well declare as a
	// pointer to avoid copying in the implementation.
	char* d_name;
};

extern DIR* opendir(const char* name);
extern struct dirent* readdir(DIR*);
extern int closedir(DIR*);

// return status for the file returned by the last successful
// readdir call from the given directory stream.
// currently sets st_size, st_mode, and st_mtime; the rest are zeroed.
// non-portable, but considerably faster than stat(). used by dir_ForEachSortedEntry.
extern int readdir_stat_np(DIR*, struct stat*);


//
// <stdlib.h>
//

extern char* realpath(const char*, char*);


//
// <unistd.h>
//

// values from MS _access() implementation. do not change.
#define F_OK 0
#define R_OK 4
#define W_OK 2
// .. MS implementation doesn't support this distinction.
//    hence, the file is reported executable if it exists.
#define X_OK 0

extern int access(const char* path, int mode);
extern int rmdir(const char* path);

#endif	// #ifndef INCLUDED_WFILESYSTEM
