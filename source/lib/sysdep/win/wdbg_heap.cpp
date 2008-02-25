#include "precompiled.h"
#include "wdbg_heap.h"

#include "lib/sysdep/win/win.h"
#include <crtdbg.h>
#include <excpt.h>
#include <dbghelp.h>

#include "lib/sysdep/cpu.h"	// cpu_AtomicAdd
#include "winit.h"
#include "wdbg.h"       // wdbg_printf
#include "wdbg_sym.h"   // wdbg_sym_WalkStack


WINIT_REGISTER_EARLY_INIT2(wdbg_heap_Init);	// wutil -> wdbg_heap


void wdbg_heap_Enable(bool enable)
{
	uint flags = 0;
	if(enable)
	{
		flags |= _CRTDBG_ALLOC_MEM_DF;	// enable checks at deallocation time
		flags |= _CRTDBG_LEAK_CHECK_DF;	// report leaks at exit
#if CONFIG_PARANOIA
		flags |= _CRTDBG_CHECK_ALWAYS_DF;	// check during every heap operation (slow!)
		flags |= _CRTDBG_DELAY_FREE_MEM_DF;	// blocks cannot be reused
#endif
	}
	_CrtSetDbgFlag(flags);

	// Send output to stdout as well as the debug window, so it works during
	// the normal build process as well as when debugging the test .exe
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
}


void wdbg_heap_Validate()
{
	int ret = _HEAPOK;
	__try
	{
		// NB: this is a no-op if !_CRTDBG_ALLOC_MEM_DF.
		// we could call _heapchk but that would catch fewer errors.
		ret = _CrtCheckMemory();
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		ret = _HEAPBADNODE;
	}

	if(ret != _HEAPOK)
		DEBUG_DISPLAY_ERROR(L"Heap is corrupted!");
}


//-----------------------------------------------------------------------------
// improved leak detection
//-----------------------------------------------------------------------------

// leak detectors often rely on macro redirection to determine the file and
// line of allocation owners (see _CRTDBG_MAP_ALLOC). unfortunately this
// breaks code that uses placement new or functions called free() etc.
//
// we avoid this problem by using stack traces. this implementation differs
// from other approaches, e.g. Visual Leak Detector (the safer variant
// before DLL hooking was used) in that no auxiliary storage is needed.
// instead, the trace is stashed within the memory block header.
//
// to avoid duplication of effort, the CRT's leak detection code is not
// modified; we only need an allocation and report hook. the latter
// mixes the improved file/line information into the normal report.


//-----------------------------------------------------------------------------
// memory block header

// the one disadvantage of our approach is that it requires knowledge of
// the internal memory block header structure. it is hoped that IsValid will
// uncover any changes. the following definition was adapted from dbgint.h:
struct _CrtMemBlockHeader
{
	struct _CrtMemBlockHeader* next;
	struct _CrtMemBlockHeader* prev;
	char* file;
	int line;
	// fields reversed on Win64 to ensure size % 16 == 0
#if OS_WIN64
	int blockType;
	size_t userDataSize;
#else
	size_t userDataSize;
	int blockType;
#endif
	long allocationNumber;
	unsigned char gap[4];

	bool IsValid() const
	{
		__try
		{
			if(prev && prev->next != this)
				return false;
			if(next && next->prev != this)
				return false;
			if((unsigned)blockType > 4)
				return false;
			if(userDataSize > 1*GiB)
				return false;
			if(allocationNumber == 0)
				return false;
			for(int i = 0; i < 4; i++)
			{
				if(gap[i] != 0xFD)
					return false;
			}

			// this is a false alarm if there is exactly one extant allocation,
			// but also a valuable indication of a block that has been removed
			// from the list (i.e. freed).
			if(prev == next)
				return false;
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}

		return true;
	}
};

static _CrtMemBlockHeader* HeaderFromData(void* userData)
{
	_CrtMemBlockHeader* const header = ((_CrtMemBlockHeader*)userData)-1;
	wdbg_assert(header->IsValid());
	return header;
}


/**
 * update our idea of the head of the linked list of heap blocks.
 * called from the allocation hook (see explanation there)
 *
 * @return the current head (most recent allocation).
 * @param operation the current heap operation
 * @param userData allocation address (if reallocating or deallocating)
 * @param hasChanged a convenient indication of whether the return value is
 * different than that of the last call.
 **/
static _CrtMemBlockHeader* GetHeapListHead(int operation, void* userData, bool& hasChanged)
{
	static _CrtMemBlockHeader* s_heapListHead;

	// first call: get the heap block list head
	// notes:
	// - there is no O(1) accessor for this, so we maintain a copy.
	// - must be done here instead of in an initializer to guarantee
	//   consistency, since we are now under the _HEAP_LOCK.
	if(!s_heapListHead)
	{
		_CrtMemState state = {0};
		_CrtMemCheckpoint(&state);	// O(N)
		s_heapListHead = state.pBlockHeader;
		wdbg_assert(s_heapListHead->IsValid());
	}

	// the last operation was an allocation or expanding reallocation;
	// exactly one block has been prepended to the list.
	if(s_heapListHead->prev)
	{
		s_heapListHead = s_heapListHead->prev;	// set to new head of list
		wdbg_assert(s_heapListHead->IsValid());
		wdbg_assert(s_heapListHead->prev == 0);
		hasChanged = true;
	}
	// the list head remained unchanged, so the last operation was a
	// non-expanding reallocation or free.
	else
		hasChanged = false;

	// special case: handle invalidation of the list head
	// note: even shrinking reallocations cause deallocation.
	if(operation != _HOOK_ALLOC && userData == s_heapListHead+1)
	{
		s_heapListHead = s_heapListHead->next;
		wdbg_assert(s_heapListHead->IsValid());

		hasChanged = false;	// (head is now the same as last time)
	}

	return s_heapListHead;
}


//-----------------------------------------------------------------------------
// call stack filter

// we need to make the most out of the limited amount of frames. to that end,
// only user functions are stored; we skip known library and helper functions.
// these are determined by recording frames encountered in a backtrace.

/**
 * extents of a module in memory; used to ignore callers that lie within
 * the C runtime library.
 **/
class ModuleExtents
{
public:
	ModuleExtents()
		: m_address(0), m_length(0)
	{
	}

	ModuleExtents(const char* dllName)
	{
		HMODULE hModule = GetModuleHandle(dllName);
		PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((u8*)hModule + ((PIMAGE_DOS_HEADER)hModule)->e_lfanew);
		m_address = (uintptr_t)hModule + ntHeaders->OptionalHeader.BaseOfCode;
		MEMORY_BASIC_INFORMATION mbi = {0};
		VirtualQuery((void*)m_address, &mbi, sizeof(mbi));
		m_length = mbi.RegionSize;
	}

	uintptr_t Address() const
	{
		return m_address;
	}

	uintptr_t Length() const
	{
		return m_length;
	}

	bool Contains(uintptr_t address) const
	{
		return (address - m_address) < m_length;
	}

private:
	uintptr_t m_address;
	size_t m_length;
};


/**
 * set data structure that avoids dynamic allocations because they would
 * cause the allocation hook to be reentered (bad).
 **/
template<typename T, size_t maxItems>
class ArraySet
{
public:
	ArraySet()
	{
		m_arrayEnd = m_array;
	}

	void Add(T t)
	{
		if(m_arrayEnd == m_array+maxItems)
		{
			RemoveDuplicates();
			wdbg_assert(m_arrayEnd < m_array+maxItems);
		}
		*m_arrayEnd++ = t;
	}

	bool Find(T t) const
	{
		return std::find(m_array, const_cast<const T*>(m_arrayEnd), t) != m_arrayEnd;
	}

	void RemoveDuplicates()
	{
		std::sort(m_array, m_arrayEnd);
		m_arrayEnd = std::unique(m_array, m_arrayEnd);
	}

private:
	T m_array[maxItems];
	T* m_arrayEnd;
};


class CallerFilter
{
public:
	CallerFilter()
	{
		AddRuntimeLibraryToIgnoreList();

		m_isRecordingKnownCallers = true;
		CallHeapFunctions();
		m_isRecordingKnownCallers = false;
		m_knownCallers.RemoveDuplicates();
	}

	LibError NotifyOfCaller(uintptr_t pc)
	{
		if(!m_isRecordingKnownCallers)
			return INFO::SKIPPED;	// do not affect the stack walk

		// last 'known' function has been reached
		if(pc == (uintptr_t)&CallerFilter::CallHeapFunctions)
			return INFO::OK;	// stop stack walk

		// pc is a 'known' function on the allocation hook's back-trace
		// (e.g. _malloc_dbg and other helper functions)
		m_knownCallers.Add(pc);
		return INFO::CB_CONTINUE;
	}

	bool IsKnownCaller(uintptr_t pc) const
	{
		for(size_t i = 0; i < numModules; i++)
		{
			if(m_moduleIgnoreList[i].Contains(pc))
				return true;
		}

		return m_knownCallers.Find(pc);
	}

private:
	static const size_t numModules = 2;

	void AddRuntimeLibraryToIgnoreList()
	{
#if MSC_VERSION && _DLL	// DLL runtime library
#ifdef NDEBUG
		static const char* dllNameFormat = "msvc%c%d" ".dll";
#else
		static const char* dllNameFormat = "msvc%c%d" "d" ".dll";
#endif
		const int dllVersion = (MSC_VERSION-600)/10;	// VC2005: 1400 => 80
		wdbg_assert(0 < dllVersion && dllVersion <= 999);
		for(int i = 0; i < numModules; i++)
		{
			static const char modules[numModules] = { 'r', 'p' };	// C and C++ runtime libraries
			char dllName[20];
			sprintf_s(dllName, ARRAY_SIZE(dllName), dllNameFormat, modules[i], dllVersion);
			m_moduleIgnoreList[i] = ModuleExtents(dllName);
		}
#endif
	}

	static void CallHeapFunctions()
	{
		{
			void* p1 = malloc(1);
			void* p2 = realloc(p1, 111);
			if(p2)
				free(p2);
			else
				free(p1);
		}
		{
			char* p = new char;
			delete p;
		}
		{
			char* p = new char[2];
			delete[] p;
		}
	}

	ModuleExtents m_moduleIgnoreList[numModules];

	// note: this mechanism cannot hope to exclude every single STL helper
	// function, which is why we need the module ignore list.
	// however, it is still useful when compiling against the static CRT.
	bool m_isRecordingKnownCallers;
	ArraySet<uintptr_t, 500> m_knownCallers;
};


//-----------------------------------------------------------------------------
// stash (part of) a stack trace within _CrtMemBlockHeader

// this avoids the need for a mapping between allocation number and the
// caller information, which is slow, requires locking and consumes memory.
//
// callers := array of addresses inside functions that constitute the
// stack back-trace.

static const size_t numQuantizedPcBits = sizeof(uintptr_t)*CHAR_BIT - 2;

static uintptr_t Quantize(uintptr_t pc)
{
	// postcondition: the return value lies within the same function as
	// pc but can be stored in fewer bits. this is possible because:
	// - linkers typically align functions to at least four bytes
	// - pc is a return address and thus preceded by a call instruction and
	//   function prolog, which requires at least four bytes.
	return pc/4;
}

static uintptr_t Expand(uintptr_t pc)
{
	return pc*4;
}


static const uint numEncodedLengthBits = 2;
static const size_t maxCallers = (sizeof(char*)+sizeof(int))*CHAR_BIT / (2+14);

static uint NumBitsForEncodedLength(uint encodedLength)
{
	static const uint numBitsForEncodedLength[1u << numEncodedLengthBits] =
	{
		8,	// 1K
		14,	// 64K
		20,	// 4M
		numQuantizedPcBits	// a full pointer
	};
	return numBitsForEncodedLength[encodedLength];
}

static uint EncodedLength(uintptr_t quantizedOffset)
{
	for(uint encodedLength = 0; encodedLength < 1u << numEncodedLengthBits; encodedLength++)
	{
		const uint numBits = NumBitsForEncodedLength(encodedLength);
		const uintptr_t maxValue = (1u << numBits)-1;
		if(quantizedOffset <= maxValue)
			return encodedLength;
	}

	wdbg_assert(0);	// unreachable
}


static uintptr_t codeSegmentAddress;
static uintptr_t quantizedCodeSegmentAddress;
static uintptr_t quantizedCodeSegmentLength;

static void FindCodeSegment()
{
	const char* dllName = 0;	// current module
	ModuleExtents extents(dllName);
	codeSegmentAddress = extents.Address();
	quantizedCodeSegmentAddress = Quantize(codeSegmentAddress);
	quantizedCodeSegmentLength = Quantize(extents.Length());
}


class BitStream
{
public:
	BitStream(u8* storage, uint storageSize)
		: m_remainderBits(0), m_numRemainderBits(0)
		, m_pos(storage), m_bitsLeft((uint)storageSize*8)
	{
	}

	uint BitsLeft() const
	{
		return m_bitsLeft;
	}

	void Write(const uint numOutputBits, uintptr_t outputValue)
	{
		wdbg_assert(numOutputBits <= m_bitsLeft);
		wdbg_assert(outputValue < (1u << numOutputBits));

		uint outputBitsLeft = numOutputBits;
		while(outputBitsLeft > 0)
		{
			const uint numBits = std::min(outputBitsLeft, 8u);
			m_bitsLeft -= numBits;

			// (NB: there is no need to extract exactly numBits because
			// outputValue's MSBs were verified to be zero)
			const uintptr_t outputByte = outputValue & 0xFF;
			outputValue >>= 8;
			outputBitsLeft -= numBits;

			m_remainderBits |= outputByte << m_numRemainderBits;
			m_numRemainderBits += numBits;
			if(m_numRemainderBits >= 8)
			{
				const u8 remainderByte = (m_remainderBits & 0xFF);
				m_remainderBits >>= 8;
				m_numRemainderBits -= 8;

				*m_pos++ = remainderByte;
			}
		}
	}

	void Finish()
	{
		const uint partialBits = m_numRemainderBits % 8;
		if(partialBits)
		{
			m_bitsLeft -= 8-partialBits;
			m_numRemainderBits += 8-partialBits;
		}
		while(m_numRemainderBits)
		{
			const u8 remainderByte = (m_remainderBits & 0xFF);
			*m_pos++ = remainderByte;
			m_remainderBits >>= 8;
			m_numRemainderBits -= 8;
		}

		wdbg_assert(m_bitsLeft % 8 == 0);
		while(m_bitsLeft)
		{
			*m_pos++ = 0;
			m_bitsLeft -= 8;
		}
	}

	uintptr_t Read(const uint numInputBits)
	{
		wdbg_assert(numInputBits <= m_bitsLeft);

		uintptr_t inputValue = 0;
		uint inputBitsLeft = numInputBits;
		while(inputBitsLeft > 0)
		{
			const uint numBits = std::min(inputBitsLeft, 8u);
			m_bitsLeft -= numBits;

			if(m_numRemainderBits < numBits)
			{
				const uint inputByte = *m_pos++;
				m_remainderBits |= inputByte << m_numRemainderBits;
				m_numRemainderBits += 8;
			}

			const uintptr_t remainderByte = (m_remainderBits & ((1u << numBits)-1));
			m_remainderBits >>= numBits;
			m_numRemainderBits -= numBits;
			inputValue |= remainderByte << (numInputBits-inputBitsLeft);

			inputBitsLeft -= numBits;
		}

		return inputValue;
	}

private:
	uint m_remainderBits;
	uint m_numRemainderBits;
	u8* m_pos;
	uint m_bitsLeft;
};


static void StashCallers(_CrtMemBlockHeader* header, const uintptr_t* callers, size_t numCallers)
{
	// transform an array of callers into a (sorted and unique) set.
	uintptr_t quantizedPcSet[maxCallers];
	std::transform(callers, callers+numCallers, quantizedPcSet, Quantize);
	std::sort(quantizedPcSet, quantizedPcSet+numCallers);
	uintptr_t* const end = std::unique(quantizedPcSet, quantizedPcSet+numCallers);
	const size_t quantizedPcSetSize = end-quantizedPcSet;

	// transform the set into a sequence of quantized offsets.
	uintptr_t quantizedOffsets[maxCallers];
	if(quantizedPcSet[0] >= quantizedCodeSegmentAddress)
		quantizedOffsets[0] = quantizedPcSet[0] - quantizedCodeSegmentAddress;
	else
	{
		quantizedOffsets[0] = quantizedPcSet[0];

		// make sure RetrieveCallers can differentiate between pointers and code-segment-offsets
		wdbg_assert(quantizedOffsets[0] >= quantizedCodeSegmentLength);
	}
	for(size_t i = 1; i < numCallers; i++)
		quantizedOffsets[i] = quantizedPcSet[i] - quantizedPcSet[i-1];

	// write quantized offsets to stream
	BitStream bitStream((u8*)&header->file, sizeof(header->file)+sizeof(header->line));
	for(size_t i = 0; i < quantizedPcSetSize; i++)
	{
		const uintptr_t quantizedOffset = quantizedOffsets[i];
		const uint encodedLength = EncodedLength(quantizedOffset);
		const uint numBits = NumBitsForEncodedLength(encodedLength);
		if(bitStream.BitsLeft() < numEncodedLengthBits+numBits)
			break;
		bitStream.Write(numEncodedLengthBits, encodedLength);
		bitStream.Write(numBits, quantizedOffset);
	}

	bitStream.Finish();
}


static void RetrieveCallers(_CrtMemBlockHeader* header, uintptr_t* callers, size_t& numCallers)
{
	// read quantized offsets from stream
	uintptr_t quantizedOffsets[maxCallers];
	numCallers = 0;
	BitStream bitStream((u8*)&header->file, sizeof(header->file)+sizeof(header->line));
	for(;;)
	{
		if(bitStream.BitsLeft() < numEncodedLengthBits)
			break;
		const uint encodedLength = bitStream.Read(numEncodedLengthBits);
		const uint numBits = NumBitsForEncodedLength(encodedLength);
		if(bitStream.BitsLeft() < numBits)
			break;
		const uintptr_t quantizedOffset = bitStream.Read(numBits);
		if(!quantizedOffset)
			break;
		quantizedOffsets[numCallers++] = quantizedOffset;
	}

	if(!numCallers)
		return;

	// expand offsets into a set of callers
	if(quantizedOffsets[0] <= quantizedCodeSegmentLength)
		callers[0] = Expand(quantizedOffsets[0] + quantizedCodeSegmentAddress);
	else
		callers[0] = Expand(quantizedOffsets[0]);
	for(size_t i = 1; i < numCallers; i++)
		callers[i] = callers[i-1] + Expand(quantizedOffsets[i]);
}


//-----------------------------------------------------------------------------
// find out who called an allocation function

/**
 * gather and store a (filtered) list of callers.
 **/
class CallStack
{
public:
	void Gather()
	{
		m_numCallers = 0;
		(void)wdbg_sym_WalkStack(OnFrame_Trampoline, (uintptr_t)this);
		std::fill(m_callers+m_numCallers, m_callers+maxCallers, 0);
	}

	const uintptr_t* Callers() const
	{
		return m_callers;
	}

	size_t NumCallers() const
	{
		return m_numCallers;
	}

private:
	LibError OnFrame(const STACKFRAME64* frame)
	{
		const uintptr_t pc = frame->AddrPC.Offset;

		// skip invalid frames
		if(pc == 0)
			return INFO::CB_CONTINUE;

		LibError ret = m_filter.NotifyOfCaller(pc);
		// (CallerFilter provokes stack traces of heap functions; if that is
		// what happened, then we must not continue)
		if(ret != INFO::SKIPPED)
			return ret;

		// stop the stack walk if frame storage is full
		if(m_numCallers >= maxCallers)
			return INFO::OK;

		if(!m_filter.IsKnownCaller(pc))
			m_callers[m_numCallers++] = pc;
		return INFO::CB_CONTINUE;
	}

	static LibError OnFrame_Trampoline(const STACKFRAME64* frame, uintptr_t cbData)
	{
		CallStack* this_ = (CallStack*)cbData;
		return this_->OnFrame(frame);
	}

	CallerFilter m_filter;

	uintptr_t m_callers[maxCallers];
	size_t m_numCallers;
};


//-----------------------------------------------------------------------------
// RAII wrapper for installing a CRT allocation hook

class AllocationHook
{
public:
	AllocationHook()
	{
		wdbg_assert(s_instance == 0 && s_previousHook == 0);
		s_instance = this;
		s_previousHook = _CrtSetAllocHook(Hook);
	}

	~AllocationHook()
	{
		_CRT_ALLOC_HOOK removedHook = _CrtSetAllocHook(s_previousHook);
		wdbg_assert(removedHook == Hook);	// warn if we removed someone else's hook
		s_instance = 0;
		s_previousHook = 0;
	}

	/**
	* @param operation either _HOOK_ALLOC, _HOOK_REALLOC or _HOOK_FREE
	* @param userData is only valid (nonzero) for realloc and free because
	* we are called BEFORE the actual heap operation.
	**/
	virtual void OnHeapOperation(int operation, void* userData, size_t size, long allocationNumber) = 0;

private:
	static int __cdecl Hook(int operation, void* userData, size_t size, int blockType, long allocationNumber, const unsigned char* file, int line)
	{
		static bool busy = false;
		wdbg_assert(!busy);
		busy = true;
		s_instance->OnHeapOperation(operation, userData, size, allocationNumber);
		busy = false;

		if(s_previousHook)
			return s_previousHook(operation, userData, size, blockType, allocationNumber, file, line);
		return 1;	// continue as if the hook had never been called
	}

	// unfortunately static because we can't pass our `this' pointer through
	// the allocation hook.
	static AllocationHook* s_instance;
	static _CRT_ALLOC_HOOK s_previousHook;
};

AllocationHook* AllocationHook::s_instance;
_CRT_ALLOC_HOOK AllocationHook::s_previousHook;


//-----------------------------------------------------------------------------
// our allocation hook

// ideally we would just stash the callers in the newly created header.
// unfortunately we are called BEFORE it (and the allocation) are actually
// created, so we need to keep the information around until the next call to
// AllocHook; only then can it be stored.
//
// unfortunately the CRT does not provide an O(1) means of getting at the
// most recent block header. instead, we do so once and then keep it
// up-to-date in the allocation hook. this is safe because we run under
// the _HEAP_LOCK and ensure the allocation numbers match.

static intptr_t s_numAllocations;

intptr_t wdbg_heap_NumberOfAllocations()
{
	return s_numAllocations;
}

class AllocationTracker : public AllocationHook
{
public:
	AllocationTracker()
		: m_pendingAllocationNumber(0)
	{
	}

	virtual void OnHeapOperation(int operation, void* userData, size_t size, long allocationNumber)
	{
		UNUSED2(size);

		if(operation == _HOOK_ALLOC || operation == _HOOK_REALLOC)
			cpu_AtomicAdd(&s_numAllocations, 1);

		bool hasChanged;
		_CrtMemBlockHeader* head = GetHeapListHead(operation, userData, hasChanged);
		// if the head changed, the last operation was a (re)allocation and
		// we now have its header; stash the pending call stack there.
		if(hasChanged)
		{
			wdbg_assert(head->allocationNumber == m_pendingAllocationNumber);

			// note: overwrite existing file/line info (even if valid) to avoid
			// special cases in the report hook.
			StashCallers(head, m_pendingCallStack.Callers(), m_pendingCallStack.NumCallers());
		}

		// remember the current caller for next time
		m_pendingCallStack.Gather();	// NB: called for each operation, as required by the filter recording step
		m_pendingAllocationNumber = allocationNumber;
	}

private:
	long m_pendingAllocationNumber;
	CallStack m_pendingCallStack;
};


//-----------------------------------------------------------------------------

static void PrintCallStack(const uintptr_t* callers, size_t numCallers)
{
	if(!numCallers || callers[0] == 0)
	{
		wdbg_printf("\n  call stack not available.\n");
		return;
	}

	wdbg_printf("\n  partial call stack:\n");
	for(size_t i = 0; i < numCallers; i++)
	{
		char name[DBG_SYMBOL_LEN] = {'\0'}; char file[DBG_FILE_LEN] = {'\0'}; int line = -1;
		LibError err = debug_resolve_symbol((void*)callers[i], name, file, &line);
		wdbg_printf("    ");
		if(err != INFO::OK)
			wdbg_printf("(error %d resolving PC=%p) ", err, callers[i]);
		if(file[0] != '\0')
			wdbg_printf("%s(%d) : ", file, line);
		wdbg_printf("%s\n", name);
	}
}


static int __cdecl ReportHook(int reportType, char* message, int* out)
{
	UNUSED2(reportType);

	// set up return values to reduce the chance of mistakes below
	*out = 0;	// alternatives are failure (-1) and breakIntoDebugger (1)
	const int ret = 0;	// not "handled", continue calling other hooks

	// note: this hook is transparent in that it never affects the CRT.
	// we can't suppress parts of a leak report because that causes the
	// rest of it to be skipped.

	static enum
	{
		WaitingForDump,
		WaitingForLeakAddress,
		IsLeakAddress
	}
	state = WaitingForDump;
	switch(state)
	{
	case WaitingForDump:
		if(!strcmp(message, "Dumping objects ->\n"))
			state = WaitingForLeakAddress;
		return ret;

	case WaitingForLeakAddress:
		if(message[0] == '{')
			state = IsLeakAddress;
		else if(strchr(message, '('))
			message[0] = '\0';
		return ret;

	case IsLeakAddress:
		{
			const char* addressString = strstr(message, "0x");
			const uintptr_t address = strtoul(addressString, 0, 0);
			_CrtMemBlockHeader* header = HeaderFromData((void*)address);
			uintptr_t callers[maxCallers]; size_t numCallers;
			RetrieveCallers(header, callers, numCallers);
			PrintCallStack(callers, numCallers);
		}
		state = WaitingForLeakAddress;
		return ret;

	default:
		wdbg_assert(0);	// unreachable
	}

	wdbg_assert(0);	// unreachable
}

//-----------------------------------------------------------------------------

static AllocationTracker* s_tracker;

static LibError wdbg_heap_Init()
{
	FindCodeSegment();

#ifndef NDEBUG
	// load symbol information now (fails if it happens during shutdown)
	char name[DBG_SYMBOL_LEN]; char file[DBG_FILE_LEN]; int line;
	(void)debug_resolve_symbol(wdbg_heap_Init, name, file, &line);

	int ret = _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, ReportHook);
	if(ret == -1)
		abort();

	s_tracker = new AllocationTracker;
#endif

	wdbg_heap_Enable(true);

	return INFO::OK;
}
