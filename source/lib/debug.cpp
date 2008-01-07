/**
 * =========================================================================
 * File        : debug.cpp
 * Project     : 0 A.D.
 * Description : platform-independent debug support code.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "precompiled.h"
#include "debug.h"

#include <stdarg.h>
#include <string.h>

#include "app_hooks.h"
#include "os_path.h"
#include "path_util.h"
#include "debug_stl.h"
#include "lib/allocators/allocators.h"	// page_aligned_alloc
#include "fnv_hash.h"
#include "lib/posix/posix_pthread.h"
#include "lib/sysdep/cpu.h"	// cpu_CAS
#include "lib/sysdep/sysdep.h"
// some functions here are called from within mmgr; disable its hooks
// so that our allocations don't cause infinite recursion.
#ifdef REDEFINED_NEW
# include "lib/nommgr.h"
#endif


ERROR_ASSOCIATE(ERR::SYM_NO_STACK_FRAMES_FOUND, "No stack frames found", -1);
ERROR_ASSOCIATE(ERR::SYM_UNRETRIEVABLE_STATIC, "Value unretrievable (stored in external module)", -1);
ERROR_ASSOCIATE(ERR::SYM_UNRETRIEVABLE_REG, "Value unretrievable (stored in register)", -1);
ERROR_ASSOCIATE(ERR::SYM_TYPE_INFO_UNAVAILABLE, "Error getting type_info", -1);
ERROR_ASSOCIATE(ERR::SYM_INTERNAL_ERROR, "Exception raised while processing a symbol", -1);
ERROR_ASSOCIATE(ERR::SYM_UNSUPPORTED, "Symbol type not (fully) supported", -1);
ERROR_ASSOCIATE(ERR::SYM_CHILD_NOT_FOUND, "Symbol does not have the given child", -1);
ERROR_ASSOCIATE(ERR::SYM_NESTING_LIMIT, "Symbol nesting too deep or infinite recursion", -1);
ERROR_ASSOCIATE(ERR::SYM_SINGLE_SYMBOL_LIMIT, "Symbol has produced too much output", -1);
ERROR_ASSOCIATE(INFO::SYM_SUPPRESS_OUTPUT, "Symbol was suppressed", -1);


// needed when writing crashlog
static const size_t LOG_CHARS = 16384;
wchar_t debug_log[LOG_CHARS];
wchar_t* debug_log_pos = debug_log;

// write to memory buffer (fast)
void debug_wprintf_mem(const wchar_t* fmt, ...)
{
	const ssize_t chars_left = (ssize_t)LOG_CHARS - (debug_log_pos-debug_log);
	debug_assert(chars_left >= 0);

	// potentially not enough room for the new string; throw away the
	// older half of the log. we still protect against overflow below.
	if(chars_left < 512)
	{
		const size_t copy_size = sizeof(wchar_t) * LOG_CHARS/2;
		wchar_t* const middle = &debug_log[LOG_CHARS/2];
		cpu_memcpy(debug_log, middle, copy_size);
		memset(middle, 0, copy_size);
		debug_log_pos -= LOG_CHARS/2;	// don't assign middle (may leave gap)
	}

	// write into buffer (in-place)
	va_list args;
	va_start(args, fmt);
	int len = vswprintf(debug_log_pos, chars_left-2, fmt, args);
	va_end(args);
	if(len < 0)
	{
		debug_assert(0);	// vswprintf failed
		return;
	}
	debug_log_pos += len+2;
	wcscpy(debug_log_pos-2, L"\r\n");	// safe
}



// need to shoehorn printf-style variable params into
// the OutputDebugString call.
// - don't want to split into multiple calls - would add newlines to output.
// - fixing Win32 _vsnprintf to return # characters that would be written,
//   as required by C99, looks difficult and unnecessary. if any other code
//   needs that, implement GNU vasprintf.
// - fixed size buffers aren't nice, but much simpler than vasprintf-style
//   allocate+expand_until_it_fits. these calls are for quick debug output,
//   not loads of data, anyway.

// max # characters (including \0) output by debug_(w)printf in one call.
static const int MAX_CHARS = 512;


// rationale: static data instead of std::set to allow setting at any time.
// we store FNV hash of tag strings for fast comparison; collisions are
// extremely unlikely and can only result in displaying more/less text.
static const uint MAX_TAGS = 20;
static u32 tags[MAX_TAGS];
static uint num_tags;

void debug_filter_add(const char* tag)
{
	const u32 hash = fnv_hash(tag);

	// make sure it isn't already in the list
	for(uint i = 0; i < MAX_TAGS; i++)
		if(tags[i] == hash)
			return;

	// too many already?
	if(num_tags == MAX_TAGS)
	{
		debug_assert(0);	// increase MAX_TAGS
		return;
	}

	tags[num_tags++] = hash;
}

void debug_filter_remove(const char* tag)
{
	const u32 hash = fnv_hash(tag);

	for(uint i = 0; i < MAX_TAGS; i++)
		// found it
		if(tags[i] == hash)
		{
			// replace with last element (avoid holes)
			tags[i] = tags[MAX_TAGS-1];
			num_tags--;

			// can only happen once, so we're done.
			return;
		}
}

void debug_filter_clear()
{
	for(uint i = 0; i < MAX_TAGS; i++)
		tags[i] = 0;
}

bool debug_filter_allows(const char* text)
{
	uint i;
	for(i = 0; ; i++)
	{
		// no | found => no tag => should always be displayed
		if(text[i] == ' ' || text[i] == '\0')
			return true;
		if(text[i] == '|' && i != 0)
			break;
	}

	const u32 hash = fnv_hash(text, i);

	// check if entry allowing this tag is found
	for(i = 0; i < MAX_TAGS; i++)
		if(tags[i] == hash)
			return true;

	return false;
}


#undef debug_printf	// allowing #defining it out
void debug_printf(const char* fmt, ...)
{
	char buf[MAX_CHARS]; buf[ARRAY_SIZE(buf)-1] = '\0';

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, MAX_CHARS-1, fmt, ap);
	va_end(ap);

	if(debug_filter_allows(buf))
		debug_puts(buf);
}

#undef debug_wprintf	// allowing #defining it out
void debug_wprintf(const wchar_t* fmt, ...)
{
	wchar_t wcs_buf[MAX_CHARS]; wcs_buf[ARRAY_SIZE(wcs_buf)-1] = '\0';

	va_list ap;
	va_start(ap, fmt);
	vswprintf(wcs_buf, MAX_CHARS-1, fmt, ap);
	va_end(ap);

	// convert wchar_t to UTF-8.
	//
	// rationale: according to fwide(3) and assorted manpage, FILEs are in
	// single character or in wide character mode. When a FILE is in
	// single character mode, wide character writes will fail, and no
	// conversion is done automatically. Thus the manual conversion.
	//
	// it's done here (instead of in OS-specific debug_putws) because
	// filter_allow requires the conversion also.
	//
	// jw: MSDN wcstombs dox say 2 bytes per wchar is enough.
	// not sure about this; to be on the safe side, we check for overflow.
	const size_t MAX_BYTES = MAX_CHARS*2;
	char mbs_buf[MAX_BYTES]; mbs_buf[MAX_BYTES-1] = '\0';
	size_t bytes_written = wcstombs(mbs_buf, wcs_buf, MAX_BYTES);
	debug_assert(bytes_written != (size_t)-1);	// else: invalid wcs character encountered
	debug_assert(bytes_written <= MAX_BYTES);	// else: overflow (should be impossible)
	// exact fit, ensure it's 0-terminated
	if(bytes_written == MAX_BYTES)
		mbs_buf[MAX_BYTES-1] = '\0';

	if(debug_filter_allows(mbs_buf))
		debug_puts(mbs_buf);
}


//-----------------------------------------------------------------------------

LibError debug_write_crashlog(const wchar_t* text)
{
	// avoid potential infinite loop if an error occurs here.
	static uintptr_t in_progress;
	if(!cpu_CAS(&in_progress, 0, 1))
		return ERR::REENTERED;	// NOWARN

	OsPath path = OsPath(ah_get_log_dir())/"crashlog.txt";
	FILE* f = fopen(path.string().c_str(), "w");
	if(!f)
	{
		in_progress = 0;
		WARN_RETURN(ERR::FAIL);
	}

	fputwc(0xfeff, f);	// BOM
	fwprintf(f, L"%ls\n", text);
	fwprintf(f, L"\n\n====================================\n\n");

	// allow user to bundle whatever information they want
	ah_bundle_logs(f);

	fwprintf(f, L"Last known activity:\n\n %ls\n", debug_log);

	fclose(f);
	in_progress = 0;
	return INFO::OK;
}



//////////////////////////////////////////////////////////////////////////////
//
// storage for and construction of strings describing a symbol
//
//////////////////////////////////////////////////////////////////////////////

// tightly pack strings within one large buffer. we never need to free them,
// since the program structure / addresses can never change.
static const size_t STRING_BUF_SIZE = 64*KiB;
static char* string_buf;
static char* string_buf_pos;

static const char* symbol_string_build(void* symbol, const char* name, const char* file, int line)
{
	// maximum bytes allowed per string (arbitrary).
	// needed to prevent possible overflows.
	const size_t STRING_MAX = 1000;

	if(!string_buf)
	{
		string_buf = new char[STRING_BUF_SIZE];
		string_buf_pos = string_buf;
	}

	// make sure there's enough space for a new string
	char* string = string_buf_pos;
	if(string + STRING_MAX >= string_buf + STRING_BUF_SIZE)
	{
		WARN_ERR(ERR::LIMIT);
		return 0;
	}

	// user didn't know name/file/line. attempt to resolve from debug info.
	char name_buf[DBG_SYMBOL_LEN];
	char file_buf[DBG_FILE_LEN];
	if(!name || !file || !line)
	{
		int line_buf;
		(void)debug_resolve_symbol(symbol, name_buf, file_buf, &line_buf);

		// only override the original parameters if value is meaningful;
		// otherwise, stick with what we got, even if 0.
		// (obviates test of return value; correctly handles partial failure).
		if(name_buf[0])
			name = name_buf;
		if(file_buf[0])
			file = file_buf;
		if(line_buf)
			line = line_buf;
	}

	// file and line are available: write them
	int len;
	if(file && line)
	{
		// strip path from filename (long and irrelevant)
		const char* fn_only = path_name_only(file);

		len = snprintf(string, STRING_MAX-1, "%s:%05d ", fn_only, line);
	}
	// only address is known
	else
		len = snprintf(string, STRING_MAX-1, "%p ", symbol);

	// append symbol name
	if(name)
	{
		snprintf(string+len, STRING_MAX-1-len, "%s", name);
		debug_stl_simplify_name(string+len);
	}

	return string;
}


//////////////////////////////////////////////////////////////////////////////
//
// cache, mapping symbol address to its description string.
//
//////////////////////////////////////////////////////////////////////////////

// note: we don't want to allocate a new string for every symbol -
// that would waste lots of memory. instead, when a new address is first
// encountered, allocate a string describing it, and store for later use.

// hash table entry; valid iff symbol != 0. the string pointer must remain
// valid until the cache is shut down.
struct Symbol
{
	void* symbol;
	const char* string;
};

static const uint MAX_SYMBOLS = 2048;
static Symbol* symbols;
static uint total_symbols;


static uint hash_jumps;

// strip off lower 2 bits, since it's unlikely that 2 symbols are
// within 4 bytes of one another.
static uint hash(void* symbol)
{
	const uintptr_t address = (uintptr_t)symbol;
	return (uint)( (address >> 2) % MAX_SYMBOLS );
}


// algorithm: hash lookup with linear probing.
static const char* symbol_string_from_cache(void* symbol)
{
	// hash table not initialized yet, nothing to find
	if(!symbols)
		return 0;

	uint idx = hash(symbol);
	for(;;)
	{
		Symbol* c = &symbols[idx];

		// not in table
		if(!c->symbol)
			return 0;
		// found
		if(c->symbol == symbol)
			return c->string;

		idx = (idx+1) % MAX_SYMBOLS;
	}
}


// associate <string> (must remain valid) with <symbol>, for
// later calls to symbol_string_from_cache.
static void symbol_string_add_to_cache(const char* string, void* symbol)
{
	if(!symbols)
	{
		// note: must be zeroed to set each Symbol to "invalid"
		symbols = (Symbol*)calloc(MAX_SYMBOLS, sizeof(Symbol));
		debug_assert(symbols);
	}

	// hash table is completely full (guard against infinite loop below).
	// if this happens, the string won't be cached - nothing serious.
	if(total_symbols >= MAX_SYMBOLS)
		WARN_ERR_RETURN(ERR::LIMIT);
	total_symbols++;

	// find Symbol slot in hash table
	Symbol* c;
	uint idx = hash(symbol);
	for(;;)
	{
		c = &symbols[idx];

		// found an empty slot
		if(!c->symbol)
			break;

		idx = (idx+1) % MAX_SYMBOLS;
		hash_jumps++;
	}

	// commit Symbol information
	c->symbol  = symbol;
	c->string = string;

	string_buf_pos += strlen(string)+1;
}




const char* debug_get_symbol_string(void* symbol, const char* name, const char* file, int line)
{
	// return it if already in cache
	const char* string = symbol_string_from_cache(symbol);
	if(string)
		return string;

	// try to build a new string
	string = symbol_string_build(symbol, name, file, line);
	if(!string)
		return 0;

	symbol_string_add_to_cache(string, symbol);

	return string;
}


//-----------------------------------------------------------------------------
// output
//-----------------------------------------------------------------------------

// translates and displays the given strings in a dialog.
// this is typically only used when debug_display_error has failed or
// is unavailable because that function is much more capable.
// implemented via sys_display_msgw; see documentation there.
void debug_display_msgw(const wchar_t* caption, const wchar_t* msg)
{
	sys_display_msgw(ah_translate(caption), ah_translate(msg));
}


// when an error has come up and user clicks Exit, we don't want any further
// errors (e.g. caused by atexit handlers) to come up, possibly causing an
// infinite loop. it sucks to hide errors, but we assume that whoever clicked
// exit really doesn't want to see any more errors.
static bool exit_requested;

// this logic is applicable to any type of error. special cases such as
// suppressing certain expected WARN_ERRs are done there.
static bool should_suppress_error(u8* suppress)
{
	if(!suppress)
		return false;

	if(*suppress == DEBUG_SUPPRESS)
		return true;

	if(exit_requested)
		return true;

	return false;
}


static const size_t message_size_bytes = 256*KiB;	// enough

void debug_error_message_free(ErrorMessageMem* emm)
{
	page_aligned_free(emm->pa_mem, message_size_bytes);
}

// split out of debug_display_error because it's used by the self-test.
const wchar_t* debug_error_message_build(
	const wchar_t* description,
	const char* fn_only, int line, const char* func,
	uint skip, void* context,
	ErrorMessageMem* emm)
{
	// rationale: see ErrorMessageMem
	emm->pa_mem = page_aligned_alloc(message_size_bytes);
	if(!emm->pa_mem)
		return L"(insufficient memory to generate error message)";
	wchar_t* const buf = (wchar_t*)emm->pa_mem;
	const size_t max_chars = message_size_bytes / sizeof(wchar_t);
	wchar_t* pos = buf; size_t chars_left = max_chars; int len;

	// header
	len = swprintf(pos, chars_left,
		L"%ls\r\n"
		L"Location: %hs:%d (%hs)\r\n"
		L"\r\n"
		L"Call stack:\r\n"
		L"\r\n",
		description, fn_only, line, func);
	if(len < 0)
	{
fail:
		return L"(error while formatting error message)";
	}
	pos += len; chars_left -= len;

	// append stack trace
	if(!context)
		skip += 2;	// skip debug_error_message_build and debug_display_error
	LibError ret = debug_dump_stack(pos, chars_left, skip, context);
	if(ret == ERR::REENTERED)
	{
		len = swprintf(pos, chars_left,
			L"(cannot start a nested stack trace; what probably happened is that "
			L"an debug_assert/debug_warn/CHECK_ERR fired during the current trace.)"
		);
		if(len < 0) goto fail; pos += len; chars_left -= len;
	}
	else if(ret != INFO::OK)
	{
		char description_buf[100] = {'?'};
		len = swprintf(pos, chars_left,
			L"(error while dumping stack: %hs)",
			error_description_r(ret, description_buf, ARRAY_SIZE(description_buf))
		);
		if(len < 0) goto fail; pos += len; chars_left -= len;
	}
	else	// success
	{
		len = (int)wcslen(buf);
		pos = buf+len; chars_left = max_chars-len;
	}

	// append OS error (just in case it happens to be relevant -
	// it's usually still set from unrelated operations)
	char description_buf[100] = { '?' };
	LibError errno_equiv = LibError_from_errno(false);
	if(errno_equiv != ERR::FAIL)	// meaningful translation
		error_description_r(errno_equiv, description_buf, ARRAY_SIZE(description_buf));
	char os_error[100];
	if(sys_error_description_r(0, os_error, ARRAY_SIZE(os_error)) != INFO::OK)
		strcpy_s(os_error, ARRAY_SIZE(os_error), "?");
	len = swprintf(pos, chars_left,
		L"\r\n"
		L"errno = %d (%hs)\r\n"
		L"OS error = %hs\r\n",
		errno, description_buf, os_error
	);
	if(len < 0) goto fail; pos += len; chars_left -= len;

	return buf;
}

static ErrorReaction call_display_error(const wchar_t* text, uint flags)
{
	// first try app hook implementation
	ErrorReaction er = ah_display_error(text, flags);
	// .. it's only a stub: default to normal implementation
	if(er == ER_NOT_IMPLEMENTED)
		er = sys_display_error(text, flags);

	return er;
}

static ErrorReaction carry_out_ErrorReaction(ErrorReaction er, uint flags, u8* suppress)
{
	const bool manual_break = (flags & DE_MANUAL_BREAK) != 0;

	switch(er)
	{
	case ER_BREAK:
		// handle "break" request unless the caller wants to (doing so here
		// instead of within the dlgproc yields a correct call stack)
		if(!manual_break)
		{
			debug_break();
			er = ER_CONTINUE;
		}
		break;

	case ER_SUPPRESS:
		*suppress = DEBUG_SUPPRESS;
		er = ER_CONTINUE;
		break;

	case ER_EXIT:
		exit_requested = true;	// see declaration

		// disable memory-leak reporting to avoid a flood of warnings
		// (lots of stuff will leak since we exit abnormally).
		debug_heap_enable(DEBUG_HEAP_NONE);
#if CONFIG_USE_MMGR
		mmgr_set_options(0);
#endif

		exit(EXIT_FAILURE);
	}

	return er;
}

ErrorReaction debug_display_error(const wchar_t* description,
	uint flags, uint skip, void* context,
	const char* file, int line, const char* func,
	u8* suppress)
{
	// "suppressing" this error means doing nothing and returning ER_CONTINUE.
	if(should_suppress_error(suppress))
		return ER_CONTINUE;

	// fix up params
	// .. translate
	description = ah_translate(description);
	// .. caller supports a suppress flag; set the corresponding flag so that
	//    the error display implementation enables the Suppress option.
	if(suppress)
		flags |= DE_ALLOW_SUPPRESS;
	// .. deal with incomplete file/line info
	if(!file || file[0] == '\0')
		file = "unknown";
	if(line <= 0)
		line = 0;
	if(!func || func[0] == '\0')
		func = "?";
	// .. _FILE__ evaluates to the full path (albeit without drive letter)
	//    which is rather long. we only display the base name for clarity.
	const char* fn_only = path_name_only(file);

	// display in output window; double-click will navigate to error location.
	debug_wprintf(L"%hs(%d): %ls\n", fn_only, line, description);


	ErrorMessageMem emm;
	const wchar_t* text = debug_error_message_build(description,
		fn_only, line, func, skip, context, &emm);

	debug_write_crashlog(text);
	ErrorReaction er = call_display_error(text, flags);

	// note: debug_break-ing here to make sure the app doesn't continue
	// running is no longer necessary. debug_display_error now determines our
	// window handle and is modal.

	// must happen before carry_out_ErrorReaction because that may exit.
	debug_error_message_free(&emm);

	return carry_out_ErrorReaction(er, flags, suppress);
}




// strobe indicating expected_err is valid and the next error should be
// compared against that / skipped if equal to it.
// set/reset via cpu_CAS for thread-safety (hence uintptr_t).
static uintptr_t expected_err_valid;
static LibError expected_err;

void debug_skip_next_err(LibError err)
{
	if(cpu_CAS(&expected_err_valid, 0, 1))
		expected_err = err;
	else
		debug_assert(0);	// internal error: concurrent attempt to skip assert/error

}

static bool should_skip_this_error(LibError err)
{
	// (compare before resetting strobe - expected_err may change afterwards)
	bool was_expected_err = (expected_err == err);
	// (use cpu_CAS to ensure only one error is skipped)
	if(cpu_CAS(&expected_err_valid, 1, 0))
	{
		debug_assert(was_expected_err);
		return was_expected_err;
	}

	return false;
}

// to share code between assert and error skip mechanism, we treat the former as
// an error. choose the code such that no one would want to warn of it.
static const LibError assert_err = INFO::OK;

void debug_skip_next_assert()
{
	debug_skip_next_err(assert_err);
}

static bool should_skip_this_assert()
{
	return should_skip_this_error(assert_err);
}


ErrorReaction debug_assert_failed(const char* expr, u8* suppress,
	const char* file, int line, const char* func)
{
	if(should_skip_this_assert())
		return ER_CONTINUE;
	uint skip = 1; void* context = 0;
	wchar_t buf[400];
	swprintf(buf, ARRAY_SIZE(buf), L"Assertion failed: \"%hs\"", expr);
	return debug_display_error(buf, DE_MANUAL_BREAK, skip,context, file,line,func, suppress);
}


ErrorReaction debug_warn_err(LibError err, u8* suppress,
	const char* file, int line, const char* func)
{
	if(should_skip_this_error(err))
		return ER_CONTINUE;

	uint skip = 1; void* context = 0;
	wchar_t buf[400];
	char err_buf[200]; error_description_r(err, err_buf, ARRAY_SIZE(err_buf));
	swprintf(buf, ARRAY_SIZE(buf), L"Function call failed: return value was %d (%hs)", err, err_buf);
	return debug_display_error(buf, DE_MANUAL_BREAK, skip,context, file,line,func, suppress);
}


//-----------------------------------------------------------------------------
// thread naming
//-----------------------------------------------------------------------------

// when debugging multithreading problems, logging the currently running
// thread is helpful; a user-specified name is easier to remember than just
// the thread handle. to that end, we provide a robust TLS mechanism that is
// much safer than the previous method of hijacking TIB.pvArbitrary.
//
// note: on Win9x thread "IDs" are pointers to the TIB xor-ed with an
// obfuscation value calculated at boot-time.
//
// __declspec(thread) et al. are now available on VC and newer GCC but we
// implement TLS manually (via pthread_setspecific) to ensure compatibility.

static pthread_key_t tls_key;
static pthread_once_t tls_once = PTHREAD_ONCE_INIT;


// provided for completeness and to avoid displaying bogus resource leaks.
static void tls_shutdown()
{
	WARN_ERR(pthread_key_delete(tls_key));
	tls_key = 0;
}


// (called via pthread_once from debug_set_thread_name)
static void tls_init()
{
	WARN_ERR(pthread_key_create(&tls_key, 0));	// no dtor

	// note: do not use atexit; this may be called before _cinit.
}


// set the current thread's name; it will be returned by subsequent calls to
// debug_get_thread_name.
//
// the string pointed to by <name> MUST remain valid throughout the
// entire program; best to pass a string literal. allocating a copy
// would be quite a bit more work due to cleanup issues.
//
// if supported on this platform, the debugger is notified of the new name;
// it will be displayed there instead of just the handle.
void debug_set_thread_name(const char* name)
{
	WARN_ERR(pthread_once(&tls_once, tls_init));

	WARN_ERR(pthread_setspecific(tls_key, name));

#if OS_WIN
	wdbg_set_thread_name(name);
#endif
}


// return the pointer assigned by debug_set_thread_name or 0 if
// that hasn't been done yet for this thread.
const char* debug_get_thread_name()
{
	return (const char*)pthread_getspecific(tls_key);
}




void debug_shutdown()
{
	tls_shutdown();
}
