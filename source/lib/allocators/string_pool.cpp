/**
 * =========================================================================
 * File        : string_pool.cpp
 * Project     : 0 A.D.
 * Description : shared storage for strings
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "precompiled.h"
#include "string_pool.h"

#include "lib/rand.h"
#include "lib/sysdep/cpu.h"	// cpu_memcpy


StringPool::StringPool(size_t maxSize)
{
	pool_create(&m_pool, maxSize, POOL_VARIABLE_ALLOCS);
}


StringPool::~StringPool()
{
	m_map.clear();
	(void)pool_destroy(&m_pool);
}


const char* StringPool::UniqueCopy(const char* string)
{
	// early out: check if it already lies in the pool
	if(Contains(string))
		return string;

	// check if equivalent to an existing string.
	//
	// rationale: the entire storage could be done via container,
	// rather than simply using it as a lookup mapping.
	// however, DynHashTbl together with Pool (see above) is more efficient.
	const char* existingString = m_map.find(string);
	if(existingString)
		return existingString;

	const size_t length = strlen(string);
	const char* uniqueCopy = (const char*)pool_alloc(&m_pool, length+1);
	if(!uniqueCopy)
		throw std::bad_alloc();
	cpu_memcpy((void*)uniqueCopy, string, length);
	((char*)uniqueCopy)[length] = '\0';

	m_map.insert(uniqueCopy, uniqueCopy);

	return uniqueCopy;
}


bool StringPool::Contains(const char* string) const
{
	return pool_contains(&m_pool, (void*)string);
}


const char* StringPool::RandomString() const
{
	// there had better be names in m_pool, else this will fail.
	debug_assert(m_pool.da.pos != 0);

again:
	const size_t start_ofs = (size_t)rand(0, (uint)m_pool.da.pos);

	// scan back to start of string (don't scan ahead; this must
	// work even if m_pool only contains one entry).
	const char* start = (const char*)m_pool.da.base + start_ofs;
	for(size_t i = 0; i < start_ofs; i++)
	{
		if(*start == '\0')
			break;
		start--;
	}

	// skip past the '\0' we found. loop is needed because there may be
	// several if we land in padding (due to pool alignment).
	size_t chars_left = m_pool.da.pos - start_ofs;
	for(; *start == '\0'; start++)
	{
		// we had landed in padding at the end of the buffer.
		if(chars_left-- == 0)
			goto again;
	}

	return start;
}
