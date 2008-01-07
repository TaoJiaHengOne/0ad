/**
 * =========================================================================
 * File        : stl.h
 * Project     : 0 A.D.
 * Description : fixes for various STL implementations
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_STL
#define INCLUDED_STL

#include "lib/config.h"
#include "compiler.h"
#include <cstdlib> // indirectly pull in bits/c++config.h on Linux, so __GLIBCXX__ is defined

// detect STL version
// .. Dinkumware
#if MSC_VERSION
# include <yvals.h>	// defines _CPPLIB_VER
#endif
#if defined(_CPPLIB_VER)
# define STL_DINKUMWARE _CPPLIB_VER
#else
# define STL_DINKUMWARE 0
#endif
// .. GCC
#if defined(__GLIBCPP__)
# define STL_GCC __GLIBCPP__
#elif defined(__GLIBCXX__)
# define STL_GCC __GLIBCXX__
#else
# define STL_GCC 0
#endif
// .. ICC
#if defined(__INTEL_CXXLIB_ICC)
# define STL_ICC __INTEL_CXXLIB_ICC
#else
# define STL_ICC 0
#endif


// disable (slow!) iterator checks in release builds (unless someone already defined this)
#if STL_DINKUMWARE && defined(NDEBUG) && !defined(_SECURE_SCL)
# define _SECURE_SCL 0
#endif


// pass "disable exceptions" setting on to the STL
#if CONFIG_DISABLE_EXCEPTIONS
# if STL_DINKUMWARE
#  define _HAS_EXCEPTIONS 0
# else
#  define STL_NO_EXCEPTIONS
# endif
#endif


// STL_HASH_MAP, STL_HASH_MULTIMAP, STL_HASH_SET
// these containers are useful but not part of C++98. most STL vendors
// provide them in some form; we hide their differences behind macros.

#if STL_GCC

# include <ext/hash_map>
# include <ext/hash_set>

# define STL_HASH_MAP __gnu_cxx::hash_map
# define STL_HASH_MULTIMAP __gnu_cxx::hash_multimap
# define STL_HASH_SET __gnu_cxx::hash_set
# define STL_HASH_MULTISET __gnu_cxx::hash_multiset
# define STL_SLIST __gnu_cxx::slist

template<typename T>
size_t STL_HASH_VALUE(T v)
{
	return __gnu_cxx::hash<T>()(v);
}

// Hack: GCC Doesn't have a hash instance for std::string included (and it looks
// like they won't add it - marked resolved/wontfix in the gcc bugzilla)
namespace __gnu_cxx
{
	template<> struct hash<std::string>
	{
		size_t operator()(const std::string& __x) const
		{
			return __stl_hash_string(__x.c_str());
		}
	};

	template<> struct hash<const void*>
	{
		// Nemesi hash function (see: http://mail-index.netbsd.org/tech-kern/2001/11/30/0001.html)
		// treating the pointer as an array of bytes that is hashed.
		size_t operator()(const void* __x) const
		{
			union {
				const void* ptr;
				unsigned char bytes[sizeof(void*)];
			} val;
			size_t h = 5381;

			val.ptr = __x;

			for(uint i = 0; i < sizeof(val); ++i)
				h = 257*h + val.bytes[i];

			return 257*h;
		}
	};
}

#elif STL_DINKUMWARE

# include <hash_map>
# include <hash_set>
// VC7 or above
# if MSC_VERSION >= 1300
#  define STL_HASH_MAP stdext::hash_map
#  define STL_HASH_MULTIMAP stdext::hash_multimap
#  define STL_HASH_SET stdext::hash_set
#  define STL_HASH_MULTISET stdext::hash_multiset
#  define STL_HASH_VALUE stdext::hash_value
// VC6 and anything else (most likely name)
# else
#  define STL_HASH_MAP std::hash_map
#  define STL_HASH_MULTIMAP std::hash_multimap
#  define STL_HASH_SET std::hash_set
#  define STL_HASH_MULTISET std::hash_multiset
#  define STL_HASH_VALUE std::hash_value
# endif	// MSC_VERSION >= 1300

#endif


// nonstandard STL containers
#define HAVE_STL_SLIST 0
#if STL_DINKUMWARE
# define HAVE_STL_HASH 1
#else
# define HAVE_STL_HASH 0
#endif

#endif	// #ifndef INCLUDED_STL
