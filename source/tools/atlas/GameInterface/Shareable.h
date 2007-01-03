#ifndef SHAREABLE_H__
#define SHAREABLE_H__

/*

The Atlas UI DLL needs to share information with the game EXE. It's most
convenient if they can pass STL objects, like std::wstring and std::vector;
but that causes problems if the DLL and EXE were not compiled in exactly
the same way.

So, the Shareable<T> class is used to make things a bit safer:
Simple types (primitives, basic structs, etc) are passed as normal.
std::string is converted to an array, using a shared (and thread-safe) memory
allocation function so that it works when the DLL and EXE use different heaps. 
std::vector is done the same, though its element type must be Shareable too.

This ought to protect against:
* Different heaps (msvcr71 vs msvcr80, debug vs release, etc).
* Different STL class layout.
It doesn't protect against:
* Different data type sizes/ranges.
* Different packing in our structs. (But they're very simple structs,
only storing size_t and pointer values.)
* Vtable layout - this code doesn't actually care, but the rest of Atlas does.

Usage should be fairly transparent - conversions from T to Shareable<T> are
automatic, and the opposite is automatic for primitive types.
For basic structs, use operator-> to access members (e.g. "msg->sharedstruct->value").
For more complex things (strings, vectors), use the unary operator* to get back
an STL object (e.g. "std::string s = *msg->sharedstring").
(That conversion to an STL object is potentially expensive, so
Shareable<string>.c_str() and Shareable<vector>.GetBuffer/GetSize() can be used
if that's all you need.)

The supported list of primitive types is below (SHAREABLE_PRIMITIVE).
Structs are made shareable by manually ensuring that all their members are
shareable (i.e. primitives and Shareable<T>s) and writing
	SHAREABLE_STRUCTS(StructName);
after their definition.

*/

#include "SharedMemory.h"

#include <vector>

// We want to use placement new, which breaks when compiling Debug configurations
// in the game and in wx, and they both need different workarounds.
// (Duplicated in SharedMemory.h)
#ifdef new
# define SHAREABLE_USED_NOMMGR
# ifdef __WXWINDOWS__
#  undef new
# else
#  include "lib/nommgr.h"
# endif
#endif

namespace AtlasMessage
{

// By default, things are not shareable
template <typename T> class Shareable
{
public:
	Shareable();
	enum { TypeIsShareable = 0 };
};

// Primitive types just need a very simple wrapper
#define SHAREABLE_PRIMITIVE(T) \
	template<> class Shareable<T> \
	{ \
		T m; \
	public: \
		enum { TypeIsShareable = 1 }; \
		Shareable() {} \
		Shareable(T const& rhs) : m(rhs) {} \
		operator T() const { return m; } \
		T _Unwrap() const { return m; } \
	}

SHAREABLE_PRIMITIVE(unsigned char);
SHAREABLE_PRIMITIVE(int);
SHAREABLE_PRIMITIVE(long);
SHAREABLE_PRIMITIVE(bool);
SHAREABLE_PRIMITIVE(float);
SHAREABLE_PRIMITIVE(void*);

#undef SHAREABLE_PRIMITIVE

// Structs are similar to primitives, but with operator->
#define SHAREABLE_STRUCT(T) \
	template<> class Shareable<T> \
	{ \
		T m; \
	public: \
		enum { TypeIsShareable = 1 }; \
		Shareable() {} \
		Shareable(T const& rhs) { m = rhs; } \
		const T* operator->() const { return &m; } \
		operator const T() const { return m; } \
		const T _Unwrap() const { return m; } \
	}


// Shareable containers must have shareable contents - but it's easy to forget
// to declare them, so make sure the errors are almost readable, like:
//   "use of undefined type 'REQUIRE_TYPE_TO_BE_SHAREABLE_FAILURE<T,__formal>
//      with [ T=AtlasMessage::sTerrainGroupPreview, __formal=false ]"
//
// (Implementation based on boost/static_assert)
template <typename T, bool> struct REQUIRE_TYPE_TO_BE_SHAREABLE_FAILURE;
template <typename T> struct REQUIRE_TYPE_TO_BE_SHAREABLE_FAILURE<T, true>{};
template<int x> struct static_assert_test{};
#define ASSERT_TYPE_IS_SHAREABLE(T) typedef static_assert_test< \
	sizeof(REQUIRE_TYPE_TO_BE_SHAREABLE_FAILURE< T, (bool)(Shareable<T>::TypeIsShareable) >)> \
	static_assert_typedef_

// Should be an empty string when cast to either char* or wchar_t*
// (which requires length >= 4, since GCC's sizeof(wchar_t)==4)
static const char empty_str[4] = { 0, 0, 0, 0 };

// Shareable strings:
template<typename C> class Shareable< std::basic_string<C> >
{
	typedef std::basic_string<C> wrapped_type;

	C* buf; // null-terminated string (perhaps with embedded nulls)
	size_t length; // size of buf (including null)
public:
	enum { TypeIsShareable = 1 };

	Shareable() : buf(NULL), length(0) {}

	Shareable(const wrapped_type& rhs)
	{
		length = rhs.length()+1;
		buf = (C*)ShareableMallocFptr(sizeof(C)*length);
		memcpy(buf, rhs.c_str(), sizeof(C)*length);
	}

	~Shareable()
	{
		ShareableFreeFptr(buf);
	}

	Shareable<wrapped_type>& operator=(const Shareable<wrapped_type>& rhs)
	{
		if (&rhs == this)
			return *this;
		ShareableFreeFptr(buf);
		length = rhs.length;
		buf = (C*)ShareableMallocFptr(sizeof(C)*length);
		memcpy(buf, rhs.buf, sizeof(C)*length);
		return *this;
	}

	Shareable(const Shareable<wrapped_type>& rhs)
		: buf(NULL), length(0)
	{
		*this = rhs;
	}

	const wrapped_type _Unwrap() const
	{
		return (buf && length) ? wrapped_type(buf, buf+length-1) : wrapped_type();
	}

	const wrapped_type operator*() const
	{
		return _Unwrap();
	}

	// Minor optimisation for code that just wants to access the characters,
	// without constructing a new std::basic_string then calling c_str on that
	const C* c_str() const
	{
		return (buf && length) ? buf : (C*)empty_str;
	}
};

// Shareable vectors:
template<typename E> class Shareable<std::vector<E> >
{
	ASSERT_TYPE_IS_SHAREABLE(E);
	typedef std::vector<E> wrapped_type;
	typedef Shareable<E> element_type;
	element_type* array;
	size_t size;

	// Since we're allocating with malloc (roughly), but storing non-trivial
	// objects, we have to allocate with placement new and call destructors
	// manually. (At least the objects are usually just other Shareables, so it's
	// reasonably safe to assume there's no exceptions or other confusingness.)
	void Unalloc()
	{
		if (array == NULL)
			return;

		for (size_t i = 0; i < size; ++i)
			array[i].~element_type();
		ShareableFreeFptr(array);

		array = NULL;
		size = 0;
	}

public:
	enum { TypeIsShareable = 1 };

	Shareable() : array(NULL), size(0) {}

	Shareable(const wrapped_type& rhs)
	{
		size = rhs.size();
		array = static_cast<element_type*> (ShareableMallocFptr( sizeof(element_type)*size ));
		for (size_t i = 0; i < size; ++i)
			new (&array[i]) element_type (rhs[i]);
	}

	~Shareable()
	{
		Unalloc();
	}

	Shareable<wrapped_type>& operator=(const Shareable<wrapped_type>& rhs)
	{
		if (&rhs == this)
			return *this;
		Unalloc();
		size = rhs.size;
		array = static_cast<element_type*> (ShareableMallocFptr( sizeof(element_type)*size ));
		for (size_t i = 0; i < size; ++i)
			new (&array[i]) element_type (rhs.array[i]);
		return *this;
	}

	Shareable(const Shareable<wrapped_type>& rhs)
		: array(NULL), size(0)
	{
		*this = rhs;
	}

	const wrapped_type _Unwrap() const
	{
		wrapped_type r;
		r.reserve(size);
		for (size_t i = 0; i < size; ++i)
			r.push_back(array[i]._Unwrap());
		return r;
	}

	const wrapped_type operator*() const
	{
		return _Unwrap();
	}

	// Minor optimisation for code that just wants to access the buffer,
	// without constructing a new std::vector
	const element_type* GetBuffer() const
	{
		return array;
	}
	size_t GetSize() const
	{
		return size;
	}
};


// Shareable callbacks:
// (TODO - this is probably not really safely shareable, due to unspecified calling conventions)
template<typename T> struct Callback
{
	Callback(void (*cb) (const T*, void*), void* cbdata) : cb(cb), cbdata(cbdata) {}
	void (*cb) (const T*, void*);
	void* cbdata;
};

template<typename T> class Shareable<Callback<T> >
{
	ASSERT_TYPE_IS_SHAREABLE(T);
public:
	Shareable(Callback<T> cb) : cb(cb) {}
	Callback<T> cb;
	void Call(const T& data) const
	{
		cb.cb(&data, cb.cbdata);
	}
};


}

#ifdef SHAREABLE_USED_NOMMGR
# ifdef __WXWINDOWS__ // TODO: portability to non-Windows wx
#  define new  new( _NORMAL_BLOCK, __FILE__, __LINE__)
# else
#  include "mmgr.h"
# endif
# undef SHAREABLE_USED_NOMMGR
#endif

#endif // SHAREABLE_H__
