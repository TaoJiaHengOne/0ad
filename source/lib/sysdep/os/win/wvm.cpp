/* Copyright (C) 2025 Wildfire Games.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * virtual memory interface. supercedes POSIX mmap; provides support for
 * large pages, autocommit, and specifying protection flags during allocation.
 */

#include "precompiled.h"
#include "lib/sysdep/vm.h"

#include "lib/alignment.h"	// CACHE_ALIGNED
#include "lib/bits.h"	// round_down
#include "lib/module_init.h"
#include "lib/sysdep/numa.h"
#include "lib/sysdep/os/win/wutil.h"
#include "lib/timer.h"

#include <atomic>
#include <excpt.h>

namespace vm
{


//-----------------------------------------------------------------------------
// per-processor statistics

// (alignment avoids false sharing)
CACHE_ALIGNED(struct Statistics)	// POD
{
	// thread-safe (required due to concurrent commits)
	void NotifyLargePageCommit()
	{
		++largePageCommits;
	}

	void NotifySmallPageCommit()
	{
		++smallPageCommits;
	}

	std::atomic<size_t> largePageCommits{ 0 };
	std::atomic<size_t> smallPageCommits{ 0 };
};
static CACHE_ALIGNED(Statistics) statistics[os_cpu_MaxProcessors];

void DumpStatistics()
{
	ENSURE(IsAligned(&statistics[0], cacheLineSize));
	ENSURE(IsAligned(&statistics[1], cacheLineSize));

	size_t smallPageCommits = 0;
	size_t largePageCommits = 0;
	uintptr_t processorsWithNoCommits = 0;
	for(size_t processor = 0; processor < os_cpu_NumProcessors(); processor++)
	{
		const Statistics& s = statistics[processor];
		if(s.smallPageCommits == 0 && s.largePageCommits == 0)
			processorsWithNoCommits |= Bit<uintptr_t>(processor);
		smallPageCommits += s.smallPageCommits;
		largePageCommits += s.largePageCommits;
	}

	const size_t totalCommits = smallPageCommits+largePageCommits;
	if(totalCommits == 0)	// this module wasn't used => don't print debug output
		return;

	const size_t largePageRatio = totalCommits? largePageCommits*100/totalCommits : 0;
	debug_printf("%d commits (%d, i.e. %d%% of them via large pages)\n", totalCommits, largePageCommits, largePageRatio);
	if(processorsWithNoCommits != 0)
		debug_printf("  processors with no commits: %x\n", processorsWithNoCommits);

	if(numa_NumNodes() > 1)
		debug_printf("NUMA factor: %.2f\n", numa_Factor());
}


//-----------------------------------------------------------------------------
// allocator with large-page and NUMA support

static bool largePageAllocationTookTooLong = false;

static bool ShouldUseLargePages(size_t allocationSize, DWORD allocationType, PageType pageType)
{
	// don't even check for large page support.
	if(pageType == kSmall)
		return false;

	// can't use large pages when reserving - VirtualAlloc would fail with
	// ERROR_INVALID_PARAMETER.
	if((allocationType & MEM_COMMIT) == 0)
		return false;

	// OS lacks support for large pages.
	if(os_cpu_LargePageSize() == 0)
		return false;

	// large pages are available and application wants them used.
	if(pageType == kLarge)
		return true;

	// default: use a heuristic.
	{
		// internal fragmentation would be excessive.
		if(allocationSize <= g_LargePageSize / 2)
			return false;

		// a previous attempt already took too long.
		if(largePageAllocationTookTooLong)
			return false;
	}

	return true;
}


// used for reserving address space, committing pages, or both.
static void* AllocateLargeOrSmallPages(uintptr_t address, size_t size, DWORD allocationType, PageType pageType = kDefault, int prot = PROT_READ|PROT_WRITE)
{
	const HANDLE hProcess = GetCurrentProcess();
	const DWORD protect = MemoryProtectionFromPosix(prot);

	UCHAR node;
	const DWORD processor = GetCurrentProcessorNumber();
	WARN_IF_FALSE(GetNumaProcessorNode((UCHAR)processor, &node));

	if(ShouldUseLargePages(size, allocationType, pageType))
	{
		// MEM_LARGE_PAGES requires aligned addresses and sizes
		const size_t largePageSize = os_cpu_LargePageSize();
		const uintptr_t alignedAddress = round_down(address, largePageSize);
		const size_t alignedSize = round_up(size+largePageSize-1, largePageSize);
		// note: this call can take SECONDS, which is why several checks are
		// undertaken before we even try. these aren't authoritative, so we
		// at least prevent future attempts if it takes too long.
		const double startTime = timer_Time(); COMPILER_FENCE;
		void* largePages = VirtualAllocExNuma(hProcess, LPVOID(alignedAddress), alignedSize, allocationType|MEM_LARGE_PAGES, protect, node);
		const double elapsedTime = timer_Time() - startTime; COMPILER_FENCE;
		if(elapsedTime > 0.5)
			largePageAllocationTookTooLong = true;	// avoid large pages next time
		if(largePages)
		{
			if((allocationType & MEM_COMMIT) != 0)
				statistics[processor].NotifyLargePageCommit();
			return largePages;
		}
	}

	// try (again) with regular pages
	void* smallPages = VirtualAllocExNuma(hProcess, LPVOID(address), size, allocationType, protect, node);
	if(smallPages)
	{
		if((allocationType & MEM_COMMIT) != 0)
			statistics[processor].NotifySmallPageCommit();
		return smallPages;
	}
	else
	{
		MEMORY_BASIC_INFORMATION mbi = {0};
		(void)VirtualQuery(LPCVOID(address), &mbi, sizeof(mbi));	// return value is #bytes written in mbi
		debug_printf("Allocation failed: base=%p allocBase=%p allocProt=%d size=%d state=%d prot=%d type=%d\n", mbi.BaseAddress, mbi.AllocationBase, mbi.AllocationProtect, mbi.RegionSize, mbi.State, mbi.Protect, mbi.Type);
	}

	return 0;
}


//-----------------------------------------------------------------------------
// address space reservation

// indicates the extent of a range of address space,
// and the parameters for committing large/small pages in it.
//
// this bookkeeping information increases the safety of on-demand commits,
// enables different parameters for separate allocations, and allows
// variable alignment because it retains the original base address.
// (storing this information within the allocated memory would
// require mapping an additional page and may waste an entire
// large page if the base address happens to be aligned already.)
CACHE_ALIGNED(struct AddressRangeDescriptor)	// POD
{
	// attempt to activate this descriptor and reserve address space.
	// side effect: initializes all fields if successful.
	//
	// @param size, commitSize, pageType, prot - see ReserveAddressSpace.
	// @return INFO::SKIPPED if this descriptor is already in use,
	//   INFO::OK on success, otherwise ERR::NO_MEM (after showing an
	//   error message).
	Status Allocate(size_t size, size_t commitSize, PageType pageType, int prot)
	{
		// if this descriptor wasn't yet in use, mark it as busy
		// (double-checking is cheaper than compare_exchange)
		void* unused{ nullptr };
		if(base != nullptr || !base.compare_exchange_strong(unused, static_cast<void*>(this)))
			return INFO::SKIPPED;

		ENSURE(size != 0);		// probably indicates a bug in caller
		ENSURE((commitSize % g_LargePageSize) == 0 || pageType == kSmall);
		ASSERT(pageType == kLarge || pageType == kSmall || pageType == kDefault);
		ASSERT(prot == PROT_NONE || (prot & ~(PROT_READ|PROT_WRITE|PROT_EXEC)) == 0);
		m_CommitSize = commitSize;
		m_PageType = pageType;
		m_Prot = prot;
		m_Alignment = pageType == kSmall ? g_PageSize : g_LargePageSize;
		m_TotalSize = round_up(size + m_Alignment - 1, m_Alignment);

		// NB: it is meaningless to ask for large pages when reserving
		// (see ShouldUseLargePages). pageType only affects subsequent commits.
		base = AllocateLargeOrSmallPages(0, m_TotalSize, MEM_RESERVE);
		if(!base)
		{
			debug_printf("AllocateLargeOrSmallPages of %lld failed\n", (u64)m_TotalSize);
			DEBUG_DISPLAY_ERROR(ErrorString());
			return ERR::NO_MEM;	// NOWARN (error string is more helpful)
		}

		alignedBase = round_up(reinterpret_cast<uintptr_t>(base.load()), m_Alignment);
		alignedEnd = alignedBase + round_up(size, m_Alignment);
		return INFO::OK;
	}

	void Free()
	{
		vm::Free(base, m_TotalSize);
		m_Alignment = 0;
		m_TotalSize = 0;
		base = 0;	// release descriptor for subsequent reuse
		alignedBase = alignedEnd = 0;
	}

	bool Contains(uintptr_t address) const
	{
		// safety check: we should never see pointers in the no-man's-land
		// between the original and rounded up base addresses.
		ENSURE(!(reinterpret_cast<uintptr_t>(base.load()) <= address && address < alignedBase));

		return (alignedBase <= address && address < alignedEnd);
	}

	bool Commit(uintptr_t address)
	{
		// (safe because Allocate rounded up to alignment)
		const uintptr_t alignedAddress = round_down(address, m_Alignment);
		ENSURE(alignedBase <= alignedAddress && alignedAddress + m_CommitSize <= alignedEnd);
		return vm::Commit(alignedAddress, m_CommitSize, m_PageType, m_Prot);
	}

	// corresponds to the respective page size (Windows requires
	// naturally aligned addresses and sizes when committing large pages).
	// note that VirtualAlloc's alignment defaults to 64 KiB.
	uintptr_t m_Alignment;

	uintptr_t alignedBase;	// multiple of alignment
	uintptr_t alignedEnd;	// "

	// (actual requested size / allocated address is required by
	// ReleaseAddressSpace due to variable alignment.)
	std::atomic<void*> base{ nullptr };
	size_t m_TotalSize;

	// parameters to be relayed to vm::Commit
	size_t m_CommitSize;
	PageType m_PageType;
	int m_Prot;

//private:
	static const wchar_t* ErrorString()
	{
#if ARCH_IA32
		return L"Out of address space (64-bit OS may help)";
#elif OS_WIN
		// because early AMD64 lacked CMPXCHG16B, the Windows lock-free slist
		// must squeeze the address, ABA tag and list length (a questionable
		// design decision) into 64 bits. that leaves 39 bits for the
		// address, plus 4 implied zero bits due to 16-byte alignment.
		// [http://www.alex-ionescu.com/?p=50]
		return L"Out of address space (Windows only provides 8 TiB)";
#else
		return L"Out of address space";
#endif
	}
};

// (array size governs the max. number of extant allocations)
static AddressRangeDescriptor ranges[2*os_cpu_MaxProcessors];


static AddressRangeDescriptor* FindDescriptor(uintptr_t address)
{
	for(size_t idxRange = 0; idxRange < ARRAY_SIZE(ranges); idxRange++)
	{
		AddressRangeDescriptor& d = ranges[idxRange];
		if(d.Contains(address))
			return &d;
	}

	return 0;	// not contained in any allocated ranges
}


void* ReserveAddressSpace(size_t size, size_t commitSize, PageType pageType, int prot)
{
	for(size_t idxRange = 0; idxRange < ARRAY_SIZE(ranges); idxRange++)
	{
		Status ret = ranges[idxRange].Allocate(size, commitSize, pageType, prot);
		if(ret == INFO::OK)
			return (void*)ranges[idxRange].alignedBase;
		if(ret == ERR::NO_MEM)
			return 0;
		// else: descriptor already in use, try the next one
	}

	// all descriptors are in use; ranges[] was too small
	DEBUG_WARN_ERR(ERR::LIMIT);
	return 0;
}


void ReleaseAddressSpace(void* p, size_t)
{
	// it is customary to ignore null pointers
	if(!p)
		return;

	AddressRangeDescriptor* d = FindDescriptor(uintptr_t(p));
	if(d)
		d->Free();
	else
	{
		debug_printf("No AddressRangeDescriptor contains %P\n", p);
		ENSURE(0);
	}
}


//-----------------------------------------------------------------------------
// commit/decommit, allocate/free, protect

bool Commit(uintptr_t address, size_t size, PageType pageType, int prot)
{
	return AllocateLargeOrSmallPages(address, size, MEM_COMMIT, pageType, prot) != 0;
}


bool Decommit(uintptr_t address, size_t size)
{
	return VirtualFree(LPVOID(address), size, MEM_DECOMMIT) != FALSE;
}


bool Protect(uintptr_t address, size_t size, int prot)
{
	const DWORD protect = MemoryProtectionFromPosix(prot);
	DWORD oldProtect;	// required by VirtualProtect
	const BOOL ok = VirtualProtect(LPVOID(address), size, protect, &oldProtect);
	return ok != FALSE;
}


void* Allocate(size_t size, PageType pageType, int prot)
{
	return AllocateLargeOrSmallPages(0, size, MEM_RESERVE|MEM_COMMIT, pageType, prot);
}


void Free(void* p, size_t)
{
	if(p)	// otherwise, VirtualFree complains
	{
		const BOOL ok = VirtualFree(p, 0, MEM_RELEASE);
		WARN_IF_FALSE(ok);
	}
}


//-----------------------------------------------------------------------------
// on-demand commit

// NB: avoid using debug_printf here because OutputDebugString has been
// observed to generate vectored exceptions when running outside the IDE.
static LONG CALLBACK VectoredHandler(const PEXCEPTION_POINTERS ep)
{
	const PEXCEPTION_RECORD er = ep->ExceptionRecord;

	// we only want to handle access violations. (strictly speaking,
	// unmapped memory causes page faults, but Windows reports them
	// with EXCEPTION_ACCESS_VIOLATION.)
	if(er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	// NB: read exceptions are legitimate and occur when updating an
	// accumulator for the first time.

	// get the source/destination of the read/write operation that
	// failed. (NB: don't use er->ExceptionAddress - that's the
	// location of the code that encountered the fault)
	const uintptr_t address = (uintptr_t)er->ExceptionInformation[1];

	// if unknown (e.g. access violation in kernel address space or
	// violation of alignment requirements), we don't want to handle it.
	if(address == ~uintptr_t(0))
		return EXCEPTION_CONTINUE_SEARCH;

	// the address space must have been allocated by ReserveAddressSpace
	// (otherwise we wouldn't know the desired commitSize/pageType/prot).
	AddressRangeDescriptor* d = FindDescriptor(address);
	if(!d)
		return EXCEPTION_CONTINUE_SEARCH;

	// NB: the first access to a page isn't necessarily at offset 0
	// (memcpy isn't guaranteed to copy sequentially). rounding down
	// is safe and necessary - see AddressRangeDescriptor::alignment.
	const uintptr_t alignedAddress = round_down(address, d->m_Alignment);
	bool ok = d->Commit(alignedAddress);
	if(!ok)
	{
		debug_printf("VectoredHandler: Commit(0x%p) failed; address=0x%p\n", alignedAddress, address);
		ENSURE(0);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// continue at (i.e. retry) the same instruction.
	return EXCEPTION_CONTINUE_EXECUTION;
}


static PVOID handler;
static ModuleInitState initState{ 0 };
static std::atomic<intptr_t> references{ 0 };

static Status InitHandler()
{
	ENSURE(handler == 0);
	handler = AddVectoredExceptionHandler(TRUE, VectoredHandler);
	ENSURE(handler != 0);
	return INFO::OK;
}

static void ShutdownHandler()
{
	ENSURE(handler != 0);
	const ULONG ret = RemoveVectoredExceptionHandler(handler);
	ENSURE(ret != 0);
	handler = 0;
}

void BeginOnDemandCommits()
{
	ModuleInit(&initState, InitHandler);
	++references;
}

void EndOnDemandCommits()
{
	if(references.fetch_sub(1) == 1)
		ModuleShutdown(&initState, ShutdownHandler);
}

}	// namespace vm
