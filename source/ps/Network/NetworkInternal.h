#ifndef _NetworkInternal_H
#define _NetworkInternal_H

#include <map>

#ifndef _WIN32

#define Network_GetErrorString(_error, _buf, _buflen) strerror_r(_error, _buf, _buflen)

#define Network_LastError errno

#define closesocket(_fd) close(_fd)
#else

#define Network_GetErrorString(_error, _buf, _buflen) \
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, _error+WSABASEERR, 0, _buf, _buflen, NULL)
#define Network_LastError (WSAGetLastError() - WSABASEERR)
// These are defined so that WSAGLE - WSABASEERR = E*
// i.e. the same error name can be used in winsock and posix
#define WSABASEERR			10000

#define EWOULDBLOCK          (35)
#define ENETDOWN             (50)
#define ENETUNREACH          (51)
#define ENETRESET            (52)
#define ENOTCONN             (57)
#define ESHUTDOWN            (58)
#define ENOTCONN             (57)
#define ECONNABORTED         (53)
#define ECONNRESET           (54)
#define ETIMEDOUT            (60)
#define EADDRINUSE           (48)
#define EADDRNOTAVAIL        (49)
#define ECONNREFUSED         (61)
#define EHOSTUNREACH         (65)

#define MSG_SOCKET_READY WM_USER

#endif

typedef int socket_t;

class CSocketInternal
{
public:
	socket_t m_fd;
	CSocketAddress m_RemoteAddr;

	socket_t m_AcceptFd;
	CSocketAddress m_AcceptAddr;
	
	// Bitwise OR of all operations to listen for.
	// See READ and WRITE
	uint m_Ops;

	char *m_pConnectHost;
	int m_ConnectPort;
	
	u64 m_SentBytes;
	u64 m_RecvBytes;

	inline CSocketInternal():
		m_fd(-1),
		m_AcceptFd(-1),
		m_Ops(0),
		m_pConnectHost(NULL),
		m_ConnectPort(-1),
		m_SentBytes(0),
		m_RecvBytes(0)
	{
	}
};

struct CSocketSetInternal
{
	// Any access to the global variables should be protected using m_Mutex
	pthread_mutex_t m_Mutex;
	pthread_t m_Thread;
	
	uint m_NumSockets;

	std::map <socket_t, CSocketBase * > m_HandleMap;
#ifdef _WIN32
	HWND m_hWnd;
#else
	// [0] is for use by RunWaitLoop, [1] for SendWaitLoopAbort and SendWaitLoopUpdate
	int m_Pipe[2];
#endif

	u64 m_GlobalSentBytes;
	u64 m_GlobalRecvBytes;

public:
	inline CSocketSetInternal()
	{
#ifdef _WIN32
		m_hWnd=NULL;
#else
		m_Pipe[0]=-1;
		m_Pipe[1]=-1;
#endif
		pthread_mutex_init(&m_Mutex, NULL);
		m_Thread=0;
		m_NumSockets=0;
		m_GlobalSentBytes=0;
		m_GlobalRecvBytes=0;
	}
};

#endif
