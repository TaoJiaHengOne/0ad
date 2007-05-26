/**
 * =========================================================================
 * File        : wtime.h
 * Project     : 0 A.D.
 * Description : emulate POSIX high resolution timer on Windows.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_WTIME
#define INCLUDED_WTIME

// advertise support for the timer routines we emulate; used by timer.cpp.
// #undef to avoid macro redefinition warning.
#undef HAVE_CLOCK_GETTIME
#define HAVE_CLOCK_GETTIME 1
#undef HAVE_GETTIMEOFDAY
#define HAVE_GETTIMEOFDAY 1


//
// <sys/types.h>
//

typedef unsigned long useconds_t;
typedef long suseconds_t;


//
// <unistd.h>
//

extern unsigned int sleep(unsigned int sec);
extern int usleep(useconds_t us);


//
// <time.h>
//

typedef enum
{
	// in our implementation, these actually do the same thing
	// (a timer that latches the system time at startup and uses the
	// monotonic HRT to add elapsed time since then)
	CLOCK_REALTIME,
	CLOCK_MONOTONIC
}
clockid_t;

// BSD gettimeofday
struct timeval
{
	time_t tv_sec;
	suseconds_t tv_usec;
};

// POSIX realtime clock_*
struct timespec
{
	time_t tv_sec;
	long   tv_nsec;
};

extern int gettimeofday(struct timeval* tv, void* tzp);

extern int nanosleep(const struct timespec* rqtp, struct timespec* rmtp);
extern int clock_gettime(clockid_t clock, struct timespec* ts);
extern int clock_getres(clockid_t clock, struct timespec* res);

#endif	// #ifndef INCLUDED_WTIME
