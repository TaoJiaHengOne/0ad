#ifndef INCLUDED_CLOGGER
#define INCLUDED_CLOGGER

#include <fstream>
#include <string>
#include <set>
#include <sstream>

class CLogger;
extern CLogger* g_Logger;

#define LOG (g_Logger->Log)
#define LOG_ONCE (g_Logger->LogOnce)

class CLogger : boost::noncopyable
{
public:
	enum ELogMethod
	{
		Normal,
		Error,
		Warning
	};

	// Default constructor - outputs to normal log files
	CLogger();

	// Special constructor (mostly for testing) - outputs to provided streams.
	// Can take ownership of streams and delete them in the destructor.
	CLogger(std::ostream* mainLog, std::ostream* interestingLog, bool takeOwnership);

	~CLogger();

	// Functions to write different message types
	void WriteMessage(const char *message, int interestedness);
	void WriteError  (const char *message, int interestedness);
	void WriteWarning(const char *message, int interestedness);
	
	// Function to log stuff to file
	void Log(ELogMethod method, const char* category, const char *fmt, ...);
	// Similar to Log, but only outputs each message once no matter how many times it's called
	void LogOnce(ELogMethod method, const char* category, const char *fmt, ...);
	
private:
	void Init();

	void LogUsingMethod(ELogMethod method, const char* category, const char* message);

	// the output streams
	std::ostream* m_MainLog;
	std::ostream* m_InterestingLog;
	bool m_OwnsStreams;

	// vars to hold message counts
	int m_NumberOfMessages;
	int m_NumberOfErrors;
	int m_NumberOfWarnings;

	// Returns how interesting this category is to the user
	// (0 = no messages, 1(default) = warnings/errors, 2 = all)
	int Interestedness(const char* category);

	// Used to remember LogOnce messages
	std::set<std::string> m_LoggedOnce;
};

/**
 * Helper class for unit tests - captures all log output while it is in scope,
 * and returns it as a single string.
 */
class TestLogger : boost::noncopyable
{
public:
	TestLogger();
	~TestLogger();
	std::string GetOutput();
private:
	CLogger* m_OldLogger;
	std::stringstream m_Stream;
};

#endif
