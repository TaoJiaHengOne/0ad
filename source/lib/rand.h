/**
 * =========================================================================
 * File        : rand.h
 * Project     : 0 A.D.
 * Description : pseudorandom number generator
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_RAND
#define INCLUDED_RAND

/**
 * return random integer in [min, max).
 * avoids several common pitfalls; see discussion at
 * http://www.azillionmonkeys.com/qed/random.html
 **/
extern uint rand(uint min_inclusive, uint max_exclusive);

#endif	// #ifndef INCLUDED_RAND
