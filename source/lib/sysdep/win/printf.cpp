#include "precompiled.h"

/*
	See http://www.opengroup.org/onlinepubs/009695399/functions/fprintf.html
	for the specification (apparently an extension to ISO C) that was used
	when creating this code.
*/

/*
	Added features (compared to MSVC's printf):
		Positional parameters (e.g. "%1$d", where '1' means '1st in the parameter list')
		%lld (equivalent to %I64d in MSVC)

	Unsupported features (compared to a perfect implementation):
		'	<-- because MSVC doesn't support it
		%S	}
		%C	} because they're unnecessary and can cause confusion
		*	<-- probably works in some situations, but don't expect it to
		*m$ <-- positional precision parameters, because they're not worthwhile
		portability	<-- just use GCC
		efficiency	<-- nothing in the spec says it should be as fast as possible
						(the code could be made a lot faster if speed mattered more
						than non-fixed size buffers)

*/

#include <tchar.h>
#include <string>
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <sstream>
#include <stdarg.h>

enum
{
	SPECFLAG_THOUSANDS		= 1,	// '
	SPECFLAG_LEFTJUSTIFIED	= 2,	// -
	SPECFLAG_SIGNED			= 4,	// +
	SPECFLAG_SPACEPREFIX	= 8,	// <space>
	SPECFLAG_ALTERNATE		= 16,	// #
	SPECFLAG_ZEROPAD		= 32,	// 0
};

struct FormatChunk
{
	virtual int ChunkType() = 0; // 0 = FormatSpecification, 1 = FormatString
};

struct FormatVariable : public FormatChunk
{
	int ChunkType() { return 0; }
	int position;	// undefined if the format includes no positional elements
	char flags;		// ['-+ #0]
	int width;		// -1 for *, 0 for unspecified	
	int precision;	// -1 for *
	int length;		// "\0\0\0l", "\0\0hh", etc
	char type;		// 'd', etc
};

struct FormatString : public FormatChunk
{
	int ChunkType() { return 1; }
	FormatString(std::string t) : text(t) {}
	std::string text;
};


int get_flag(TCHAR c)
{
	switch (c)
	{
	case _T('\''): return SPECFLAG_THOUSANDS;
	case _T('-'):  return SPECFLAG_LEFTJUSTIFIED;
	case _T('+'):  return SPECFLAG_SIGNED;
	case _T(' '):  return SPECFLAG_SPACEPREFIX;
	case _T('#'):  return SPECFLAG_ALTERNATE;
	case _T('0'):  return SPECFLAG_ZEROPAD;
	}
	return 0;
}

std::string flags_to_string(char flags)
{
	std::string s;
	const char* c = "\'-+ #0";
	for (int i=0; i<6; ++i)
		if (flags & (1<<i))
			s += c[i];
	return s;
}

template<typename T>
std::string to_string(T n)
{
	std::string s;
	std::stringstream str;
	str << n;
	str >> s;
	return s;
}

int is_lengthmod(TCHAR c)
{
	return 
		c == _T('h') ||
		c == _T('l') ||
		c == _T('j') ||
		c == _T('z') ||
		c == _T('t') ||
		c == _T('L');
}

// _T2('l') == 'll'
#define _T2(a) ((a<<8) | a)

int type_size(TCHAR type, int length)
{
	switch (type)
	{
	case 'd':
	case 'i': 
		switch (length)
		{
		case _T ('l'):	return sizeof(long);
		case _T2('l'):	return sizeof(long long);
		case _T ('h'):	return sizeof(short);
		case _T2('h'):	return sizeof(char);
		default: return sizeof(int);
		}
	case 'o':
	case 'u':
	case 'x':
	case 'X': return sizeof(unsigned);
	case 'f':
	case 'F':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
	case 'a':
	case 'A':
		if (length == _T('L'))
			return sizeof(long double);
		else
			return sizeof(double);
		
	case 'c': 
		// "%lc" is a wide character, passed as a wint_t (ushort)
		if (length == _T('l'))
			return sizeof(wint_t);
		// "%c" is an int, apparently
		else
			return sizeof(int);

	case 's':
		if (length == _T('l'))
			return sizeof(wchar_t*);
		else
			return sizeof(char*);

	case 'p':
		return sizeof(void*);

	case 'n':
		return sizeof(int*);

	}
	return 0;
}

extern "C" int vsnprintf2(TCHAR* buffer, size_t count, const TCHAR* format, va_list argptr)
{
	/*
	
	Format 'variable' specifications are (in pseudo-Perl regexp syntax):

	% (\d+$)? ['-+ #0]? (* | \d+)?  (. (* | \d*) )? (hh|h|l|ll|j|z|t|L)? [diouxXfFeEgGaAcspnCS%]
      position  flags     width       precision          length                   type

	*/

	/**** Parse the format string into constant/variable chunks ****/

	std::vector<FormatChunk*> specs;
	std::string stringchunk;

	TCHAR chr;

#define readchar(x) if ((x = *format++) == '\0') { delete s; goto finished_reading; }

	while ((chr = *format++) != '\0')
	{
		if (chr == _T('%'))
		{
			// Handle %% correctly
			if (*format == _T('%'))
			{
				stringchunk += _T('%');
				continue;
			}

			// End the current string and start a new spec chunk
			if (stringchunk.length())
			{
				specs.push_back(new FormatString(stringchunk));
				stringchunk = _T("");
			}

			FormatVariable *s = new FormatVariable;
			s->position = -1;
			s->flags = 0;
			s->width = 0;
			s->precision = 0;
			s->length = 0;
			s->type = 0;

			// Read either the position or the width
			int number = 0;

			while (1)
			{
				readchar(chr);

				// Read flags (but not if it's a 0 appearing after other digits)
				if (!number && get_flag(chr))
					s->flags |= get_flag(chr);

				// Read decimal numbers (position or width)
				else if (isdigit(chr))
					number = number*10 + (chr-'0');

				// If we've reached a $, 'number' was the position,
				// so remember it and start getting the width
				else if (chr == _T('$'))
				{
					s->position = number;
					number = 0;
				}

				// End of the number section
				else
				{
					// Remember the width
					s->width = number;

					// Start looking for a precision
					if (chr == _T('.'))
					{
						// Found a precision: read the digits

						number = 0;

						while (1)
						{
							readchar(chr);

							if (isdigit(chr))
								number = number*10 + (chr-'0');
							else
							{
								s->precision = number;
								break;
							}
						}
					}

					// Finished dealing with any precision.

					// Now check for length and type codes.

					if (chr == _T('I'))
					{
						assert(! "MSVC-style \"%I64\" is not allowed!");
					}

					if (is_lengthmod(chr))
					{
						s->length = chr;

						// Check for ll and hh
						if (chr == _T('l') || chr == _T('h'))
						{
							if (*format == chr)
							{
								s->length |= (chr << 8);
								++format;
							}
						}

						readchar(chr);
					}

					s->type = chr;

					specs.push_back(s);

					break;
				}
			}
		}
		else
		{
			stringchunk += chr;
		}
	}

#undef readchar

finished_reading:

	if (stringchunk.length())
	{
		specs.push_back(new FormatString(stringchunk));
		stringchunk = _T("");
	}

	/**** Build a new format string (to pass to the system printf) ****/

	std::string newformat;

	std::vector<int> varsizes; // stores the size of each variable type, to allow stack twiddling

	typedef std::vector<FormatChunk*>::iterator ChunkIt;

	for (ChunkIt it = specs.begin(); it != specs.end(); ++it)
	{
		if ((*it)->ChunkType() == 0)
		{
			FormatVariable* s = static_cast<FormatVariable*>(*it);

			if (s->position > 0)
			{
				// Grow if necessary
				if (s->position >= (int)varsizes.size())
					varsizes.resize(s->position+1, -1);
				// Store the size of the current type
				varsizes[s->position] = type_size(s->type, s->length);
			}


			newformat += "%";

			if (s->flags)
				newformat += flags_to_string(s->flags);
			
			if (s->width == -1)
				newformat += '*';
			else if (s->width)
				newformat += to_string(s->width);

			if (s->precision)
			{
				newformat += '.';
				if (s->precision == -1)
					newformat += '*';
				else
					newformat += to_string(s->precision);
			}

			if (s->length)
			{
				if (s->length > 256)
				{
					if (s->length == 0x00006c6c)
						#ifdef _MSC_VER
						 newformat += "I64"; // MSVC compatibility
						#else
						 newformat += "ll";
						#endif
					else if (s->length == 0x00006868)
						newformat += "hh";
				}
				else
				{
					newformat += (char) s->length;
				}
			}

			newformat += s->type;
		}
		else
		{
			FormatString* s = static_cast<FormatString*>(*it);
			newformat += s->text;
		}
	}


/*
	varargs on x86:

	All types are padded to 4 bytes, so size-in-stack == _INTSIZEOF(type).
	No special alignment is required.

	first+_INTSIZEOF(first) == first item in stack

	Keep adding _INTSIZEOF(item) to get the next item
*/

// Because of those dangerous assumptions about varargs:
#ifndef _M_IX86
#error SLIGHTLY FATAL ERROR: Only x86 is supported!
#endif

	// Highly efficient buffer to store the rearranged copy of the stack
	std::string newstack;

	std::vector< std::pair<char*, char*> > stackitems;

	va_list arglist = argptr;
	//va_start(arglist, format);

	const char* newstackptr;

	if (varsizes.size())
	{

		for (size_t i = 1; i < varsizes.size(); ++i)
		{
			if (varsizes[i] <= 0)
			{
				assert(! "Invalid variable type somewhere - make sure all variable things are positional and defined");
				return -1;
			}

			// Based on _INTSIZEOF in stdarg.h:
			// (Warning - slightly non-portable. But we use gcc's default printf
			// when portability matters.)
			#define INTSIZE(n)   ( (n + sizeof(int) - 1) & ~(sizeof(int) - 1) )

			size_t size = INTSIZE(varsizes[i]);
			stackitems.push_back( std::pair<char*, char*>( arglist, arglist+size ));
			arglist += size;
		}


		for (ChunkIt it = specs.begin(); it != specs.end(); ++it)
		{
			if ((*it)->ChunkType() == 0)
			{
				FormatVariable* s = static_cast<FormatVariable*>(*it);
				if (s->position <= 0)
				{
					assert(! "Invalid use of positional elements - make sure all variable things are positional and defined");
					return -1;
				}
				newstack += std::string( stackitems[s->position-1].first, stackitems[s->position-1].second );
			}
		}
		
		newstackptr = newstack.c_str();
	}
	else
	{
		newstackptr = arglist;
	}

	for (ChunkIt it = specs.begin(); it != specs.end(); ++it)
		if ((*it)->ChunkType() == 0)
			delete static_cast<FormatVariable*>(*it);
		else
			delete static_cast<FormatString*>(*it);

	int ret = _vsnprintf(buffer, count, newformat.c_str(), (va_list)newstackptr);

	return ret;
}


