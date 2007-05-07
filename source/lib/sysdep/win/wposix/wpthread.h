/**
 * =========================================================================
 * File        : wpthread.h
 * Project     : 0 A.D.
 * Description : emulate pthreads on Windows.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_WPTHREAD
#define INCLUDED_WPTHREAD


//
// <sched.h>
//

struct sched_param
{
	int sched_priority;
};

enum
{
	SCHED_RR,
	SCHED_FIFO,
	SCHED_OTHER
};

// changing will break pthread_setschedparam:
#define sched_get_priority_max(policy) +2
#define sched_get_priority_min(policy) -2


//
// <pthread.h>
//

// one-time init
typedef uintptr_t pthread_once_t;
#define PTHREAD_ONCE_INIT 0	// static pthread_once_t x = PTHREAD_ONCE_INIT;

extern int pthread_once(pthread_once_t*, void (*init_routine)(void));

// thread
typedef unsigned int pthread_t;

extern pthread_t pthread_self(void);
extern int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param);
extern int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param);
extern int pthread_create(pthread_t* thread, const void* attr, void* (*func)(void*), void* arg);
extern int pthread_cancel(pthread_t thread);
extern int pthread_join(pthread_t thread, void** value_ptr);

// mutex
typedef void* pthread_mutex_t;	// pointer to critical section
typedef void pthread_mutexattr_t;
#define PTHREAD_MUTEX_INITIALIZER pthread_mutex_initializer()

extern pthread_mutex_t pthread_mutex_initializer(void);
extern int pthread_mutex_init(pthread_mutex_t*, const pthread_mutexattr_t*);
extern int pthread_mutex_destroy(pthread_mutex_t*);
extern int pthread_mutex_lock(pthread_mutex_t*);
extern int pthread_mutex_trylock(pthread_mutex_t*);
extern int pthread_mutex_unlock(pthread_mutex_t*);
extern int pthread_mutex_timedlock(pthread_mutex_t*, const struct timespec*);

// thread-local storage
typedef unsigned int pthread_key_t;

extern int pthread_key_create(pthread_key_t*, void (*dtor)(void*));
extern int pthread_key_delete(pthread_key_t);
extern void* pthread_getspecific(pthread_key_t);
extern int   pthread_setspecific(pthread_key_t, const void* value);


//
// <semaphore.h>
//

typedef uintptr_t sem_t;

extern int sem_init(sem_t*, int pshared, unsigned value);
extern int sem_post(sem_t*);
extern int sem_wait(sem_t*);
extern int sem_timedwait(sem_t*, const struct timespec*);
extern int sem_destroy(sem_t*);

// wait until semaphore is locked or a message arrives. non-portable.
//
// background: on Win32, UI threads must periodically pump messages, or
// else deadlock may result (see WaitForSingleObject docs). that entails
// avoiding any blocking functions. when event waiting is needed,
// one cheap workaround would be to time out periodically and pump messages.
// that would work, but either wastes CPU time waiting, or introduces
// message latency. to avoid this, we provide an API similar to sem_wait and
// sem_timedwait that gives MsgWaitForMultipleObjects functionality.
//
// return value: 0 if the semaphore has been locked (SUS terminology),
// -1 otherwise. errno differentiates what happened: ETIMEDOUT if a
// message arrived (this is to ease switching between message waiting and
// periodic timeout), or an error indication.
extern int sem_msgwait_np(sem_t* sem);

#endif	// #ifndef INCLUDED_WPTHREAD
