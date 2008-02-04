/**
 * =========================================================================
 * File        : FileIo.h
 * Project     : 0 A.D.
 * Description : endian-safe binary file IO helpers.
 * =========================================================================
 */

// the file format has passing similarity to IFF. note however that
// "chunks" aren't identified by FOURCCs; only the file header is
// so marked.
// all > 8-bit integers are stored in little-endian format
// (hence the _le suffix). however, the caller is responsible for
// swapping their raw data before passing it to PackRaw.

#ifndef INCLUDED_FILEPACKER
#define INCLUDED_FILEPACKER

#include "CStr.h"
#include "lib/file/vfs/vfs_path.h"
#include "ps/Filesystem.h"	// WriteBuffer

#include "ps/Errors.h"
ERROR_GROUP(File);
ERROR_TYPE(File, OpenFailed);
ERROR_TYPE(File, WriteFailed);
ERROR_TYPE(File, InvalidType);
ERROR_TYPE(File, InvalidVersion);
ERROR_TYPE(File, ReadFailed);
ERROR_TYPE(File, UnexpectedEOF);


/**
 * helper class for writing binary files. this is basically a
 * resizable buffer that allows adding raw data and strings;
 * upon calling Write(), everything is written out to disk.
 **/
class CFilePacker 
{
public:
	/**
	 * adds version and signature (i.e. the header) to the buffer.
	 * this means Write() can write the entire buffer to file in one go,
	 * which is simpler and more efficient than writing in pieces.
	 **/
	CFilePacker(u32 version, const char magic[4]);

	~CFilePacker();

	/**
	 * write out to file all packed data added so far.
	 * it's safe to call this multiple times, but typically would
	 * only be done once.
	 **/
	void Write(const VfsPath& filename);

	/**
	 * pack given number of bytes onto the end of the data stream
	 **/
	void PackRaw(const void* rawData, size_t rawDataSize);

	/**
	 * pack a string onto the end of the data stream
	 * (encoded as a 32-bit length followed by the characters)
	 **/
	void PackString(const CStr& str);

private:
	/**
	 * the output data stream built during pack operations.
	 * contains the header, so we can write this out in one go.
	 **/
	WriteBuffer m_writeBuffer;
};


/**
 * helper class for reading binary files
 **/
class CFileUnpacker
{
public:
	CFileUnpacker();
	~CFileUnpacker();

	/**
	 * open and read in given file, check magic bits against those given;
	 * throw variety of exceptions if open failed / version incorrect, etc.
	 **/
	void Read(const VfsPath& filename, const char magic[4]);

	/**
	 * @return version number that was stored in the file's header.
	 **/
	u32 GetVersion() const
	{
		return m_version;
	}

	/**
	 * unpack given number of bytes from the input into the given array.
	 * throws PSERROR_File_UnexpectedEOF if the end of the data stream is
	 * reached before the given number of bytes have been read.
	 **/
	void UnpackRaw(void* rawData, size_t rawDataSize);

	/**
	 * unpack a string from the raw data stream.
	 * @param result is assigned a newly constructed CStr8 holding the
	 * string read from the input stream.
	 **/
	void UnpackString(CStr8& result);

private:
	// the data read from file and used during unpack operations
	shared_ptr<u8> m_buf;
	size_t m_bufSize;

	size_t m_unpackPos;	/// current unpack position in stream
	u32 m_version;	/// version that was stored in the file header
};

#endif
