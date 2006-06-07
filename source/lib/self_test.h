/**
 * =========================================================================
 * File        : self_test.h
 * Project     : 0 A.D.
 * Description : helpers for built-in self tests
 *
 * @author Jan.Wassenberg@stud.uni-karlsruhe.de
 * =========================================================================
 */

/*
 * Copyright (c) 2005 Jan Wassenberg
 *
 * Redistribution and/or modification are also permitted under the
 * terms of the GNU General Public License as published by the
 * Free Software Foundation (version 2 or later, at your option).
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/*

[KEEP IN SYNC WITH WIKI!]

Introduction
------------

Self-tests as advocated by eXtreme Programming have proven to be useful.
By embedding test code into modules, we can be confident that boundary
cases are handled correctly and everything still works after edits.
We give guidelines for their use and explain several helper mechanisms below.


Guidelines
----------

What makes a good self-test?
- They belong in the module being tested to ensure they are kept in
  sync with it.
- It is easiest to attach them to low-level functions, e.g. ilog2, rather
  than verifying the module's final result (e.g. checking renderer output by
  comparing pictures).
- You should cover all cases: expected failures ("does it fail as expected?"),
  bad inputs ("does it reject those?"), and successes ("did it have the
  expected result?").
- Tests should be non-intrusive (only bother user if something fails) and
  very quick. This is because they are executed every program run - which
  is a good thing because it solves the common problem of forgetting to
  run them after a change.

  If the test is unavoidably slow or annoying (example: wdbg_sym's
  stack trace), then best to disable it by default; see below for how.
  It can then be enabled manually after changes, and that is better than
  no test at all.


Example Usage
-------------

The following is a working example of a built-in self test using
our facilities. Further notes below are referenced with (1) etc.

>>>

#if SELF_TEST_ENABLED							// (1)
namespace test {								// (2)

static void test_log2()
{
	TEST(ilog2(0) == -1);						// (3)
	// further test cases..
}

static void self_test()
{
	test_log2();
	// further test groups..
}

SELF_TEST_RUN;									// (4)

}	// namespace test
#endif	// #if SELF_TEST_ENABLED

<<<

(1) when not enabled, self-tests are completely removed so as
    not to bloat the executable. for details on how to enable/disable them
	globally or override in single files, see below.

(2) wrapping in a namespace is optional and must be removed for C programs.
    it avoids possible name collisions with the module being tested.

(3) TEST *must* be used instead of debug_assert et al.! this is
    explained below.

(4) automatically calls your self_test function at non-local static object
    init time (i.e. before main is entered).

For further details, see below.

*/

#ifndef SELF_TEST_H__
#define SELF_TEST_H__

// a self test is enabled if at the point of its definition
// SELF_TEST_ENABLED evaluates to 1 (non-zero).
// the value #defined below is the global default. you can override it
// in individual files by defining to 0 or 1 before including this header.
#ifndef SELF_TEST_ENABLED
#define SELF_TEST_ENABLED 1
#endif

// each test case should use this (instead of assert et al.) to verify
// conditions.
// rationale: some code checks boundary conditions via assert. these are
// often triggered deliberately in self-tests to verify error behavior.
// we therefore squelch asserts while tests are active (see mechanism below),
// and this is the only error reporter guaranteed to work.
//
// note: could also stringize condition and display that, but it'd require
// macro magic (stringize+prepend L) and we already display file+line.
#define TEST(condition) STMT(\
	if(!(condition))\
		DISPLAY_ERROR(L"Self-test failed");\
)


// your source file should contain a function: void self_test(void) that
// performs all tests or calls out to individual test functions.
// this macro calls it at static init time and takes care of setting
// self_test_active (see above).
//
// rationale: since compiler optimizations may mess with the dummy variable,
// best to put this in a macro so we won't have to change each occurrence.
#define SELF_TEST_RUN\
	static int dummy = self_test_run(self_test)

// calling at static init time may not always be desirable - some
// self-tests may require initialization beforehand. this mechanism allows
// registering self tests automatically, which are then all run when you
// call self_test_run_all.
#define SELF_TEST_REGISTER\
	static SelfTestRecord self_test_record = { self_test, 0 };\
	static int dummy = self_test_register(&self_test_record)

struct SelfTestRecord
{
	void(*func)();
	const SelfTestRecord* next;
};

// call all self-tests registered thus far. rationale: see above.
// also displays a banner+elapsed time via debug_printf.
extern void self_test_run_all();


//
// internal use only:
//

// trampoline that sets self_test_active and returns a dummy value;
// used by SELF_TEST_RUN.
extern int self_test_run(void(*func)());

extern int self_test_register(SelfTestRecord* r);

// checked by debug_assert_failed; disables asserts if true (see above).
// set/cleared by run_self_test.
extern bool self_test_active;

#define TS_ASSERT_OK(expr) TS_ASSERT_EQUALS((expr), INFO_OK)
#define TS_ASSERT_STR_EQUAL(str1, str2) TS_ASSERT(!strcmp((str1), (str2)))

#endif	// #ifndef SELF_TEST_H__
