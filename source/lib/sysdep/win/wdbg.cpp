/**
 * =========================================================================
 * File        : wdbg.cpp
 * Project     : 0 A.D.
 * Description : Win32 debug support code.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "precompiled.h"
#include "lib/debug.h"

#include "lib/bits.h"
#include "win.h"
#include "wutil.h"


// return 1 if the pointer appears to be totally bogus, otherwise 0.
// this check is not authoritative (the pointer may be "valid" but incorrect)
// but can be used to filter out obviously wrong values in a portable manner.
int debug_is_pointer_bogus(const void* p)
{
#if ARCH_IA32
	if(p < (void*)0x10000)
		return true;
	if(p >= (void*)(uintptr_t)0x80000000)
		return true;
#endif

	// notes:
	// - we don't check alignment because nothing can be assumed about a
	//   string pointer and we mustn't reject any actually valid pointers.
	// - nor do we bother checking the address against known stack/heap areas
	//   because that doesn't cover everything (e.g. DLLs, VirtualAlloc).
	// - cannot use IsBadReadPtr because it accesses the mem
	//   (false alarm for reserved address space).

	return false;
}


bool debug_is_code_ptr(void* p)
{
	uintptr_t addr = (uintptr_t)p;
	// totally invalid pointer
	if(debug_is_pointer_bogus(p))
		return false;
	// comes before load address
	static const HMODULE base = GetModuleHandle(0);
	if(addr < (uintptr_t)base)
		return false;

	return true;
}


static NT_TIB* get_tib()
{
#if ARCH_IA32
	NT_TIB* tib;
	// ICC 10 doesn't support the NT_TIB.Self syntax, so we have to use
	// a constant (asm code isn't 64-bit safe anyway).
	__asm
	{
		mov		eax, fs:[NT_TIB.Self]
		mov		[tib], eax
	}
	return tib;
#endif
}

bool debug_is_stack_ptr(void* p)
{
	uintptr_t addr = (uintptr_t)p;
	// totally invalid pointer
	if(debug_is_pointer_bogus(p))
		return false;
	// not aligned
	if(addr % sizeof(void*))
		return false;
	// out of bounds (note: IA-32 stack grows downwards)
	NT_TIB* tib = get_tib();
	if(!(tib->StackLimit < p && p < tib->StackBase))
		return false;

	return true;
}


void debug_puts(const char* text)
{
	OutputDebugStringA(text);
}


void wdbg_printf(const char* fmt, ...)
{
	char buf[1024];	// as required by wvsprintf
	va_list ap;
	va_start(ap, fmt);
	wvsprintf(buf, fmt, ap);
	va_end(ap);

	debug_puts(buf);
}


// inform the debugger of the current thread's description, which it then
// displays instead of just the thread handle.
//
// see "Setting a Thread Name (Unmanaged)": http://msdn2.microsoft.com/en-us/library/xcb2z8hs(vs.71).aspx
void debug_set_thread_name(const char* name)
{
	// we pass information to the debugger via a special exception it
	// swallows. if not running under one, bail now to avoid
	// "first chance exception" warnings.
	if(!IsDebuggerPresent())
		return;

	// presented by Jay Bazuzi (from the VC debugger team) at TechEd 1999.
	const struct ThreadNameInfo
	{
		DWORD type;
		const char* name;
		DWORD thread_id;	// any valid ID or -1 for current thread
		DWORD flags;
	}
	info = { 0x1000, name, (DWORD)-1, 0 };
	__try
	{
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD*)&info);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		// if we get here, the debugger didn't handle the exception.
		// this happens if profiling with Dependency Walker; debug_assert
		// must not be called because we may be in critical init.
	}
}
