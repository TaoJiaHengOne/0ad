/**
 * =========================================================================
 * File        : qpc.cpp
 * Project     : 0 A.D.
 * Description : Timer implementation using QueryPerformanceCounter
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "precompiled.h"
#include "qpc.h"

#include "lib/sysdep/win/win.h"
#include "lib/sysdep/win/wcpu.h"
#include "lib/sysdep/win/wutil.h"	// wutil_argv
#include "pit.h"	// PIT_FREQ
#include "pmt.h"	// PMT_FREQ


LibError CounterQPC::Activate()
{
	// note: QPC is observed to be universally supported, but the API
	// provides for failure, so play it safe.

	LARGE_INTEGER qpcFreq, qpcValue;
	const BOOL ok1 = QueryPerformanceFrequency(&qpcFreq);
	const BOOL ok2 = QueryPerformanceCounter(&qpcValue);
	WARN_RETURN_IF_FALSE(ok1 && ok2);
	if(!qpcFreq.QuadPart || !qpcValue.QuadPart)
		WARN_RETURN(ERR::FAIL);

	m_frequency = (i64)qpcFreq.QuadPart;
	return INFO::OK;
}

void CounterQPC::Shutdown()
{
}

bool CounterQPC::IsSafe() const
{
	// note: we have separate modules that directly access some of the
	// counters potentially used by QPC. disabling the redundant counters
	// would be ugly (increased coupling). instead, we'll make sure our
	// implementations could (if necessary) coexist with QPC, but it
	// shouldn't come to that since only one counter is needed/used.

	// the PIT is entirely safe (even if annoyingly slow to read)
	if(m_frequency == PIT_FREQ)
		return true;

	// the PMT is generally safe (see discussion in CounterPmt::IsSafe),
	// but older QPC implementations had problems with 24-bit rollover.
	// "System clock problem can inflate benchmark scores"
	// (http://www.lionbridge.com/bi/cont2000/200012/perfcnt.asp ; no longer
	// online, nor findable in Google Cache / archive.org) tells of
	// incorrect values every 4.6 seconds (i.e. 24 bits @ 3.57 MHz) unless
	// the timer is polled in the meantime. fortunately, this is guaranteed
	// by our periodic updates (which come at least that often).
	if(m_frequency == PMT_FREQ)
		return true;

	// the TSC has been known to be buggy (even mentioned in MSDN). it is
	// used on MP HAL systems and can be detected by comparing QPF with the
	// CPU clock. we consider it unsafe unless the user promises (via
	// command line) that it's patched and thus reliable on their system.
	bool usesTsc = IsSimilarMagnitude(m_frequency, wcpu_ClockFrequency());
	// unconfirmed reports indicate QPC sometimes uses 1/3 of the
	// CPU clock frequency, so check that as well.
	usesTsc |= IsSimilarMagnitude(m_frequency, wcpu_ClockFrequency()/3);
	if(usesTsc)
	{
		const bool isTscSafe = wutil_HasCommandLineArgument("-wQpcTscSafe");
		return isTscSafe;
	}

	// the HPET is reliable and used on Vista. it can't easily be recognized
	// since its frequency is variable (the spec says > 10 MHz; the master
	// 14.318 MHz oscillator is often used). considering frequencies in
	// [10, 100 MHz) to be a HPET would be dangerous because it may actually
	// be faster or RDTSC slower. we have to exclude all other cases and
	// assume it's a HPET - and thus safe - if we get here.
	return true;
}

u64 CounterQPC::Counter() const
{
	// fairly time-critical here, don't check the return value
	// (IsSupported made sure it succeeded initially)
	LARGE_INTEGER qpc_value;
	(void)QueryPerformanceCounter(&qpc_value);
	return qpc_value.QuadPart;
}

/**
 * WHRT uses this to ensure the counter (running at nominal frequency)
 * doesn't overflow more than once during CALIBRATION_INTERVAL_MS.
 **/
uint CounterQPC::CounterBits() const
{
	// there are reports of incorrect rollover handling in the PMT
	// implementation of QPC (see CounterPMT::IsSafe). however, other
	// counters would be used on those systems, so it's irrelevant.
	// we'll report the full 64 bits.
	return 64;
}

/**
 * initial measurement of the tick rate. not necessarily correct
 * (e.g. when using TSC: wcpu_ClockFrequency isn't exact).
 **/
double CounterQPC::NominalFrequency() const
{
	return (double)m_frequency;
}
