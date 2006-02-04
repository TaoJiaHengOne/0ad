#include "precompiled.h"

#include "lib.h"
#include "timer.h"
#include "sysdep/sysdep.h"
#include "debug.h"

#include <stdarg.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "bfd.h"
#include <cxxabi.h>

#ifndef bfd_get_section_size
#define bfd_get_section_size bfd_get_section_size_before_reloc
#endif

#ifdef OS_LINUX
#define GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#endif

#ifdef OS_LINUX
# define DEBUGGER_WAIT 3
# define DEBUGGER_CMD "gdb"
# define DEBUGGER_ARG_FORMAT "--pid=%d"
# define DEBUGGER_BREAK_AFTER_WAIT 0
#else
# error "port"
#endif

#define PROFILE_RESOLVE_SYMBOL 0

// Hard-coded - yuck :P
#if defined(TESTING)
#define EXE_NAME "ps_test"
#elif defined(NDEBUG)
#define EXE_NAME "ps"
#else
#define EXE_NAME "ps_dbg"
#endif

struct symbol_file_context
{
	asymbol **syms;
	bfd *abfd;
};
symbol_file_context ps_dbg_context;
bool udbg_initialized=false;

struct symbol_lookup_context
{
	symbol_file_context *file_ctx;

	bfd_vma address;
	const char* symbol;
	const char* filename;
	uint line;
	
	bool found;
};

void unix_debug_break()
{
	kill(getpid(), SIGTRAP);
}

/*
Start the debugger and tell it to attach to the current process/thread
(called by display_error)
*/
void udbg_launch_debugger()
{
	pid_t orgpid=getpid();
	pid_t ret=fork();
	if (ret == 0)
	{
		// Child Process: exec() gdb (Debugger), set to attach to old fork
		char buf[16];
		snprintf(buf, 16, DEBUGGER_ARG_FORMAT, orgpid);
		
		int ret=execlp(DEBUGGER_CMD, DEBUGGER_CMD, buf, NULL);
		// In case of success, we should never get here anyway, though...
		if (ret != 0)
		{
			perror("Debugger launch failed");
		}
	}
	else if (ret > 0)
	{
		// Parent (original) fork:
		sleep(DEBUGGER_WAIT);
	}
	else // fork error, ret == -1
	{
		perror("Debugger launch: fork failed");
	}
}

void* debug_get_nth_caller(uint n, void *UNUSED(context))
{
	// bt[0] == debug_get_nth_caller
	// bt[1] == caller of get_nth_caller
	// bt[2] == 1:st caller (n==1)
	void *bt[n+2];
	int bt_size;
	
	bt_size=backtrace(bt, n+2);
	assert(bt_size >= (int)(n+2) && "Need at least n+2 frames in get_nth_caller");
	return bt[n+1]; // n==1 => bt[2], and so forth
}

const wchar_t* debug_dump_stack(wchar_t* buf, size_t max_chars, uint skip, void* UNUSED(context))
{
	++skip; // Skip ourselves too

	// bt[0..skip] == skipped
	// bt[skip..N_FRAMES+skip] == print
	static const uint N_FRAMES = 16;
	void *bt[skip+N_FRAMES];
	int bt_size;
	wchar_t *bufpos = buf;
	wchar_t *bufend = buf + max_chars;
	
	bt_size=backtrace(bt, ARRAY_SIZE(bt));
	// did we get enough backtraced frames?
	assert((bt_size >= (int)skip) && "Need at least <skip> frames in the backtrace");

	// Assumed max length of a single print-out
	static const uint MAX_OUT_CHARS=1024;

	for (uint i=skip;(int)i<bt_size && bufpos+MAX_OUT_CHARS < bufend;i++)
	{
		char file[DBG_FILE_LEN];
		char symbol[DBG_SYMBOL_LEN];
		int line;
		uint len;
		
		if (debug_resolve_symbol(bt[i], symbol, file, &line) == 0)
			len = swprintf(bufpos, MAX_OUT_CHARS, L"(0x%08x) %hs:%d %hs\n", bt[i], file, line, symbol);
		else
			len = swprintf(bufpos, MAX_OUT_CHARS, L"(0x%08x)\n", bt[i]);
		
		if (len < 0)
		{
			// MAX_OUT_CHARS exceeded, realistically this was caused by some
			// mindbogglingly long symbol name... replace the end with an
			// ellipsis and a newline
			memcpy(&bufpos[MAX_OUT_CHARS-6], L"...\n", 5*sizeof(wchar_t));
			len = MAX_OUT_CHARS;
		}
		
		bufpos += len;
	}

	return buf;
}

static int slurp_symtab(symbol_file_context *ctx)
{
	bfd *abfd=ctx->abfd;
	asymbol ***syms=&ctx->syms;
	long symcount;
	unsigned int size=0;

	if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
	{
		printf("slurp_symtab(): Huh? Has no symbols...\n");
		return -1;
	}

	size = bfd_get_symtab_upper_bound(abfd);
	if (size < 0)
	{
		bfd_perror("symtab_upper_bound");
		return -1;
	}
	*syms = (asymbol **)malloc(size);
	if (!syms)
		return -1;
	symcount = bfd_canonicalize_symtab(abfd, *syms);

	if (symcount == 0)
		symcount = bfd_read_minisymbols (abfd, FALSE, (void **)syms, &size);
	if (symcount == 0)
		symcount = bfd_read_minisymbols (abfd, TRUE /* dynamic */, (void **)syms, &size);

	if (symcount < 0)
	{
		bfd_perror("slurp_symtab");
		return -1;
	}
	
	return 0;
}

static int read_symbols(const char *file_name, symbol_file_context *ctx)
{
	char **matching=NULL;
	
	ONCE(bfd_init());

	ctx->abfd = bfd_openr (file_name, NULL);
	if (ctx->abfd == NULL)
	{
		bfd_perror("udbg.cpp: bfd_openr");
		return -1;
	}

	if (! bfd_check_format_matches (ctx->abfd, bfd_object, &matching))
	{
		if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
		{
			printf("Error reading symbols from %s: ambiguous format\n", file_name);
			while (*matching)
				printf("\tPotential matching format: %s\n", *matching++);
			free(matching);
		}
		else
		{
			bfd_perror("bfd_check_format_matches");
		}
		return -1;
	}

	int res=slurp_symtab(ctx);
	if (res == 0)
		return res;
	else
	{
		bfd_perror("udbg.cpp: slurp_symtab");
		bfd_close(ctx->abfd);
		return -1;
	}
}

void udbg_init(void)
{
	if (read_symbols(EXE_NAME, &ps_dbg_context)==0)
		udbg_initialized=true;

#if PROFILE_RESOLVE_SYMBOL
	{
		TIMER(udbg_init_benchmark)
		char symbol[DBG_SYMBOL_LEN];
		char file[DBG_FILE_LEN];
		int line;
		debug_resolve_symbol(debug_get_nth_caller(3), symbol, file, &line);
		printf("%s (%s:%d)\n", symbol, file, line);
		for (int i=0;i<1000000;i++)
		{
			debug_resolve_symbol(debug_get_nth_caller(1), symbol, file, &line);
		}
	}
#endif
}

static void find_address_in_section (bfd *abfd, asection *section, void *data)
{
	symbol_lookup_context *ctx=(symbol_lookup_context *)data;
	asymbol **syms=ctx->file_ctx->syms;

	bfd_vma pc=ctx->address;
	bfd_vma vma;
	bfd_size_type size;
	
	if (ctx->found) return;

	if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_get_section_vma (abfd, section);
	if (pc < vma)
		return;

	size = bfd_get_section_size (section);
	if (pc >= vma + size)
		return;

	ctx->found = bfd_find_nearest_line (abfd, section, syms,
				pc - vma, &ctx->filename, &ctx->symbol, &ctx->line);
}

// BFD functions perform allocs with real malloc - we need to free that data
#include "nommgr.h"
void demangle_buf(char *buf, const char *symbol, size_t n)
{
	int status=0;
	char *alloc=NULL;
	if (symbol == NULL || *symbol == '\0')
	{
		symbol = "??";
		status = -1;
	}
	else
	{
		alloc=abi::__cxa_demangle(symbol, NULL, NULL, &status);
	}
	// status is 0 on success and a negative value on failure
	if (status == 0)
		symbol=alloc;
	strncpy(buf, symbol, n);
	buf[n-1]=0;
	if (alloc)
		free(alloc);
}

static LibError debug_resolve_symbol_dladdr(void *ptr, char* sym_name, char* file, int* line)
{
	Dl_info syminfo;
	
	int res=dladdr(ptr, &syminfo);
	if (res == 0) return ERR_FAIL;
	
	if (sym_name)
	{
		if (syminfo.dli_sname)
			demangle_buf(sym_name, syminfo.dli_sname, DBG_SYMBOL_LEN);
		else
		{
			snprintf(sym_name, DBG_SYMBOL_LEN, "0x%08x", (uint)ptr);
			sym_name[DBG_SYMBOL_LEN-1]=0;
		}
	}
	
	if (file)
	{
		strncpy(file, syminfo.dli_fname, DBG_FILE_LEN);
		file[DBG_FILE_LEN-1]=0;
	}
	
	if (line)
	{
		*line=0;
	}
	
	return ERR_OK;
}

LibError debug_resolve_symbol(void* ptr_of_interest, char* sym_name, char* file, int* line)
{
	ONCE(udbg_init());

	// We use our default context - in the future we could do something like
	// mapping library -> file context to support more detailed reporting on
	// external libraries
	symbol_file_context *file_ctx=&ps_dbg_context;
	bfd *abfd=file_ctx->abfd;

	// Reset here if we fail later on
	if (sym_name)
		*sym_name=0;
	if (file)
		*file=0;
	if (line)
		*line=0;

	if (!udbg_initialized)
		return debug_resolve_symbol_dladdr(ptr_of_interest, sym_name, file, line);
	
	symbol_lookup_context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.address=reinterpret_cast<bfd_vma>(ptr_of_interest);
	ctx.file_ctx=file_ctx;

	bfd_map_over_sections (abfd, find_address_in_section, &ctx);
	
	// This will happen for addresses in external files. What one *could* do
	// here is to figure out the originating library file and load that through
	// BFD... but how often will external libraries have debugging info? really?
	// At least attempt to find out the symbol name through dladdr.
	if (!ctx.found)
		return debug_resolve_symbol_dladdr(ptr_of_interest, sym_name, file, line);

	if (sym_name)
	{
		demangle_buf(sym_name, ctx.symbol, DBG_SYMBOL_LEN);
	}

	if (file)
	{
		if (ctx.filename != NULL)
		{
			const char *h;

			h = strrchr (ctx.filename, '/');
			if (h != NULL)
				ctx.filename = h + 1;
	
			strncpy(file, ctx.filename, DBG_FILE_LEN);
			file[DBG_FILE_LEN]=0;
		}
		else
			strcpy(file, "none");
	}
	
	if (line)
	{
		*line = ctx.line;
	}
	
	return ERR_OK;
}

void debug_puts(const char* text)
{
	fputs(text, stdout);
	fflush(stdout);
}


// TODO: Do these properly. (I don't know what I'm doing; I just
// know that these functions are required in order to compile...)

int debug_write_crashlog(const char* UNUSED(file), wchar_t* UNUSED(header),
	void* UNUSED(context))
{
	abort();
}

int debug_is_pointer_bogus(const void* UNUSED(p))
{
	return false;
}

void debug_heap_check()
{
}

// if <full_monty> is true or PARANOIA #defined, all possible checks are
// performed as often as possible. this is really slow (we are talking x100),
// but reports errors closer to where they occurred.
void debug_heap_enable(DebugHeapChecks UNUSED(what))
{
	// No-op until we find out if glibc has heap debugging
}

// disable all automatic checks until the next debug_heap_enable.
void debug_heap_disable()
{
	// No-op until we find out if glibc has heap debugging
}
