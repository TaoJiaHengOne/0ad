/**
 * =========================================================================
 * File        : file_cache.h
 * Project     : 0 A.D.
 * Description : cache of file contents (supports zero-copy IO)
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_FILE_CACHE
#define INCLUDED_FILE_CACHE

#include "vfs_path.h"

/**
 * cache of file contents with support for zero-copy IO.
 * this works by reserving a region of the cache, using it as the IO buffer,
 * and returning the memory directly to users. optional write-protection
 * via MMU ensures that the shared contents aren't inadvertently changed.
 *
 * (unique copies of) VFS pathnames are used as lookup key and owner tag.
 *
 * to ensure efficient operation and prevent fragmentation, only one
 * reference should be active at a time. in other words, read a file,
 * process it, and only then start reading the next file.
 *
 * rationale: this is rather similar to BlockCache; however, the differences
 * (Reserve's size parameter, eviction policies) are enough to warrant
 * separate implementations.
 **/
class FileCache
{
public:
	/**
	 * @param size maximum amount [bytes] of memory to use for the cache.
	 * (managed as a virtual memory region that's committed on-demand)
	 **/
	FileCache(size_t size);

	/**
	 * Reserve a chunk of the cache's memory region.
	 *
	 * @param size required number of bytes (more may be allocated due to
	 * alignment and/or internal fragmentation)
	 * @return memory suitably aligned for IO; never fails.
	 *
	 * it is expected that this data will be Add()-ed once its IO completes.
	 **/
	shared_ptr<u8> Reserve(size_t size);

	/**
	 * Add a file's contents to the cache.
	 *
	 * the cache will be able to satisfy subsequent Retrieve() calls by
	 * returning this data; if CONFIG_READ_ONLY_CACHE, the buffer is made
	 * read-only. if need be and no references are currently attached to it,
	 * the memory can also be commandeered by Reserve().
	 *
	 * @param pathname key that will be used to Retrieve file contents.
	 * @param cost is the expected cost of retrieving the file again and
	 * influences how/when it is evicted from the cache.
	 **/
	void Add(const VfsPath& pathname, shared_ptr<u8> data, size_t size, uint cost = 1);

	/**
	 * Remove a file's contents from the cache (if it exists).
	 *
	 * this ensures subsequent reads of the files see the current, presumably
	 * recently changed, contents of the file.
	 *
	 * this would typically be called in response to a notification that a
	 * file has changed.
	 **/
	void Remove(const VfsPath& pathname);

	/**
	 * Attempt to retrieve a file's contents from the file cache.
	 *
	 * @return whether the contents were successfully retrieved; if so,
	 * data references the read-only file contents.
	 **/
	bool Retrieve(const VfsPath& pathname, shared_ptr<u8>& data, size_t& size);

private:
	class Impl;
	shared_ptr<Impl> impl;
};

#endif	// #ifndef INCLUDED_FILE_CACHE
