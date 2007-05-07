/**
 * =========================================================================
 * File        : self_test.cpp
 * Project     : 0 A.D.
 * Description : helpers for built-in self tests
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "precompiled.h"

#if 0

#include "self_test.h"
#include "timer.h"

// checked by debug_assert_failed; disables asserts if true (see above).
// set/cleared by self_test_run.
bool self_test_active = false;

// trampoline that sets self_test_active and returns a dummy value;
// used by SELF_TEST_RUN.
int self_test_run(void (*func)())
{
	self_test_active = true;
	func();
	self_test_active = false;
	return 0;	// assigned to dummy at file scope
}


static const SelfTestRecord* registered_tests;

int self_test_register(SelfTestRecord* r)
{
	// SELF_TEST_REGISTER has already initialized r->func.
	r->next = registered_tests;
	registered_tests = r;
	return 0;	// assigned to dummy at file scope
}


void self_test_run_all()
{
	debug_printf("SELF TESTS:\n");
	const double t0 = get_time();

	// someone somewhere may want to run self-tests twice (e.g. to help
	// track down memory corruption), so don't destroy the list while
	// iterating over it.
	const SelfTestRecord* r = registered_tests;
	while(r)
	{
		self_test_run(r->func);
		r = r->next;
	}

	const double dt = get_time() - t0;
	debug_printf("-- done (elapsed time %.0f ms)\n", dt*1e3);
}

#endif
