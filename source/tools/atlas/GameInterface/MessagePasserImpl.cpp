#include "precompiled.h"

#include "MessagePasserImpl.h"
#include "Messages.h"

#include "lib/timer.h"
#include "lib/rand.h"

using namespace AtlasMessage;


MessagePasserImpl::MessagePasserImpl()
: m_Trace(false), m_Semaphore(NULL)
{
	int tries = 0;
	while (tries++ < 16) // some arbitrary cut-off point to avoid infinite loops
	{
		CStr name = "/wfg-atlas-msgpass-" + CStr(rand(100000, 1000000));
		sem_t* sem = sem_open(name, O_CREAT | O_EXCL, 0700, 0);
		// This cast should not be necessary, but apparently SEM_FAILED is not
		// a value of a pointer type
		if (sem == (sem_t*)SEM_FAILED)
		{
			int err = errno;
			if (err == EEXIST)
			{
				// Semaphore already exists - try another one
				continue;
			}
			// Otherwise, it's a probably-fatal error
			debug_printf("errno: %d (%s)\n", err, strerror(err));
			debug_warn("sem_open failed");
			break;
		}
		// Succeeded - use this semaphore
		m_Semaphore = sem;
		m_SemaphoreName = name;
		break;
	}
	
	if (! m_Semaphore)
	{
		debug_warn("Failed to create semaphore for Atlas - giving up");
		// We will probably crash later - maybe we could fall back on sem_init, if this
		// ever fails in practice
	}
}

MessagePasserImpl::~MessagePasserImpl()
{
	if (m_Semaphore)
	{
		// Clean up
		sem_close(m_Semaphore);
		sem_unlink(m_SemaphoreName);
	}
}

void MessagePasserImpl::Add(IMessage* msg)
{
	debug_assert(msg);
	debug_assert(msg->GetType() == IMessage::Message);

	if (m_Trace)
		debug_printf("%8.3f add message: %s\n", get_time(), msg->GetName());

	m_Mutex.Lock();
	m_Queue.push(msg);
	m_Mutex.Unlock();
}

IMessage* MessagePasserImpl::Retrieve()
{
	// (It should be fairly easy to use a more efficient thread-safe queue,
	// since there's only one thread adding items and one thread consuming;
	// but it's not worthwhile yet.)

	m_Mutex.Lock();

	IMessage* msg = NULL;
	if (! m_Queue.empty())
	{
		msg = m_Queue.front();
		m_Queue.pop();
	}

	m_Mutex.Unlock();

	if (m_Trace && msg)
		debug_printf("%8.3f retrieved message: %s\n", get_time(), msg->GetName());

	return msg;
}

void MessagePasserImpl::Query(QueryMessage* qry, void(* UNUSED(timeoutCallback) )())
{
	debug_assert(qry);
	debug_assert(qry->GetType() == IMessage::Query);

	if (m_Trace)
		debug_printf("%8.3f add query: %s\n", get_time(), qry->GetName());

	// Set the semaphore, so we can block until the query has been handled
	qry->m_Semaphore = static_cast<void*>(m_Semaphore);

	m_Mutex.Lock();
	m_Queue.push(qry);
	m_Mutex.Unlock();

	// Wait until the query handler has handled the query and called sem_post:


	// The following code was necessary to avoid deadlock, but it still breaks
	// in some cases (e.g. when Atlas issues a query before its event loop starts
	// running) and doesn't seem to be the simplest possible solution.
	// So currently we're trying to not do anything like that at all, and
	// just stop the game making windows (which is what seems (from experience) to
	// deadlock things) by overriding ah_display_error. Hopefully it'll work like
	// that, and the redundant code below/elsewhere can be removed, but it's
	// left in here in case it needs to be reinserted in the future to make it
	// work.
	// (See http://www.wildfiregames.com/forum/index.php?s=&showtopic=10236&view=findpost&p=174617)

// 	// At least on Win32, it is necessary for the UI thread to run its event
// 	// loop to avoid deadlocking the system (particularly when the game
// 	// tries to show a dialog box); so timeoutCallback is called whenever we
// 	// think it's necessary for that to happen.
// 
// #if OS_WIN
// 	// On Win32, use MsgWaitForMultipleObjects, which waits on the semaphore
// 	// but is also interrupted by incoming Windows-messages.
//	// while (0 != (err = sem_msgwait_np(psem)))
// 	
// 	while (0 != (err = sem_wait(psem)))
// #else
// 	// TODO: On non-Win32, I have no idea whether the same problem exists; but
// 	// it might do, so call the callback every few seconds just in case it helps.
// 	struct timespec abs_timeout;
// 	clock_gettime(CLOCK_REALTIME, &abs_timeout);
// 	abs_timeout.tv_sec += 2;
// 	while (0 != (err = sem_timedwait(psem, &abs_timeout)))
// #endif

	int err;
	while (0 != (err = sem_wait(m_Semaphore)))
	{
		// If timed out, call callback and try again
// 		if (errno == ETIMEDOUT)
// 			timeoutCallback();
// 		else
		// Keep retrying while EINTR, but other errors are probably fatal
		if (errno != EINTR)
		{
			debug_warn("Semaphore wait failed");
			return; // (leaks the semaphore)
		}
	}

	// Clean up
	qry->m_Semaphore = NULL;
}

bool MessagePasserImpl::IsEmpty()
{
	m_Mutex.Lock();
	bool empty = m_Queue.empty();
	m_Mutex.Unlock();
	return empty;
}

void MessagePasserImpl::SetTrace(bool t)
{
	m_Trace = t;
}
