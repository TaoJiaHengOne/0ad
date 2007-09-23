/**
 * =========================================================================
 * File        : tsc.cpp
 * Project     : 0 A.D.
 * Description : Timer implementation using RDTSC
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "precompiled.h"
#include "tsc.h"

#include "lib/sysdep/cpu.h"
#include "lib/sysdep/win/win.h"
#include "lib/sysdep/ia32/ia32.h"	// ia32_rdtsc
#include "lib/bits.h"


//-----------------------------------------------------------------------------
// detect throttling

enum AmdPowerNowFlags
{
	PN_FREQ_ID_CTRL    = BIT(1),
	PN_SW_THERMAL_CTRL = BIT(5),
	PN_INVARIANT_TSC   = BIT(8)
};

static bool IsThrottlingPossible()
{
	u32 regs[4];

	switch(ia32_Vendor())
	{
	case IA32_VENDOR_INTEL:
		if(ia32_cap(IA32_CAP_TM_SCC) || ia32_cap(IA32_CAP_EST))
			return true;
		break;

	case IA32_VENDOR_AMD:
		if(ia32_asm_cpuid(0x80000007, regs))
		{
			if(regs[EDX] & (PN_FREQ_ID_CTRL|PN_SW_THERMAL_CTRL))
				return true;
		}
		break;
	}

	return false;
}


//-----------------------------------------------------------------------------

LibError CounterTSC::Activate()
{
	if(!ia32_cap(IA32_CAP_TSC))
		return ERR::NO_SYS;		// NOWARN (CPU doesn't support RDTSC)

	return INFO::OK;
}

void CounterTSC::Shutdown()
{
}

bool CounterTSC::IsSafe() const
{
	// use of the TSC for timing is subject to a litany of potential problems:
	// - separate, unsynchronized counters with offset and drift;
	// - frequency changes (P-state transitions and STPCLK throttling);
	// - failure to increment in C3 and C4 deep-sleep states.
	// we will discuss the specifics below.

	// SMP or multi-core => counters are unsynchronized. this could be
	// solved by maintaining separate per-core counter states, but that
	// requires atomic reads of the TSC and the current processor number.
	//
	// (otherwise, we have a subtle race condition: if preempted while
	// reading the time and rescheduled on a different core, incorrect
	// results may be returned, which would be unacceptable.)
	//
	// unfortunately this isn't possible without OS support or the
	// as yet unavailable RDTSCP instruction => unsafe.
	//
	// (note: if the TSC is invariant, drift is no longer a concern.
	// we could synchronize the TSC MSRs during initialization and avoid
	// per-core counter state and the abovementioned race condition.
	// however, we won't bother, since such platforms aren't yet widespread
	// and would surely support the nice and safe HPET, anyway)
	if(cpu_NumPackages() != 1 || cpu_CoresPerPackage() != 1)
		return false;

	// recent CPU:
	if(ia32_Generation() >= 7)
	{
		// note: 8th generation CPUs support C1-clock ramping, which causes
		// drift on multi-core systems, but those were excluded above.

		u32 regs[4];
		if(ia32_asm_cpuid(0x80000007, regs))
		{
			// TSC is invariant WRT P-state, C-state and STPCLK => safe.
			if(regs[EDX] & PN_INVARIANT_TSC)
				return true;
		}

		// in addition to P-state transitions, we're also subject to
		// STPCLK throttling. this happens when the chipset thinks the
		// system is dangerously overheated; the OS isn't even notified.
		// it may be rare, but could cause incorrect results => unsafe.
		return false;

		// newer systems also support the C3 Deep Sleep state, in which
		// the TSC isn't incremented. that's not nice, but irrelevant
		// since STPCLK dooms the TSC on those systems anyway.
	}

	// we're dealing with a single older CPU; the only problem there is
	// throttling, i.e. changes to the TSC frequency. we don't want to
	// disable this because it may be important for cooling. the OS
	// initiates changes but doesn't notify us; jumps are too frequent
	// and drastic to detect and account for => unsafe.
	if(IsThrottlingPossible())
		return false;

	return true;
}

u64 CounterTSC::Counter() const
{
	return ia32_rdtsc();
}

/**
 * WHRT uses this to ensure the counter (running at nominal frequency)
 * doesn't overflow more than once during CALIBRATION_INTERVAL_MS.
 **/
uint CounterTSC::CounterBits() const
{
	return 64;
}

/**
 * initial measurement of the tick rate. not necessarily correct
 * (e.g. when using TSC: cpu_ClockFrequency isn't exact).
 **/
double CounterTSC::NominalFrequency() const
{
	return cpu_ClockFrequency();
}
