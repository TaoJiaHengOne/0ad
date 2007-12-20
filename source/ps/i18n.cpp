#include "precompiled.h"

#include "ps/i18n.h"
#include "scripting/ScriptingHost.h"

#include "ps/Filesystem.h"

#include "ps/CLogger.h"
#define LOG_CATEGORY "i18n"

// Yay, global variables. (The user only ever wants to be using one language
// at a time, so this is sufficient)
I18n::CLocale_interface* g_CurrentLocale = NULL;
std::string g_CurrentLocaleName;

bool I18n::LoadLanguage(const char* name)
{
	// Special case: If name==NULL, use an 'empty' locale which should have
	// no external dependencies other than CLogger, so it can be called
	// before SpiderMonkey/etc has been initialised (useful for localised
	// error messages that should fall back to English if the language hasn't
	// been loaded yet)
	if (name == NULL)
	{
		CLocale_interface* locale_ptr = I18n::NewLocale(NULL, NULL);
		debug_assert(locale_ptr);
		delete g_CurrentLocale;
		g_CurrentLocale = locale_ptr;
		g_CurrentLocaleName = "";
		return true;
	}

	CLocale_interface* locale_ptr = I18n::NewLocale(g_ScriptingHost.getContext(), JS_GetGlobalObject(g_ScriptingHost.getContext()));

	if (! locale_ptr)
	{
		debug_warn("Failed to create locale");
		return false;
	}

	// Automatically delete the pointer when returning early
	std::auto_ptr<CLocale_interface> locale (locale_ptr);

	CStr dirname = CStr("language/")+name+"/";

	// Open *.lng with LoadStrings
	VfsPaths pathnames;
	if(fs_GetPathnames(g_VFS, dirname, "*.lng", pathnames) < 0)
		return false;
	for (size_t i = 0; i < pathnames.size(); i++)
	{
		const char* pathname = pathnames[i].string().c_str();
		CVFSFile strings;
		if (! (strings.Load(pathname) == PSRETURN_OK && locale->LoadStrings((const char*)strings.GetBuffer())))
		{
			LOG(ERROR, LOG_CATEGORY, "Error opening language string file '%s'", pathname);
			return false;
		}
	}

	// Open *.wrd with LoadDictionary
	if(fs_GetPathnames(g_VFS, dirname, "*.wrd", pathnames) < 0)
		return false;
	for (size_t i = 0; i < pathnames.size(); i++)
	{
		const char* pathname = pathnames[i].string().c_str();
		CVFSFile strings;
		if (! (strings.Load(pathname) == PSRETURN_OK && locale->LoadDictionary((const char*)strings.GetBuffer())))
		{
			LOG(ERROR, LOG_CATEGORY, "Error opening language string file '%s'", pathname);
			return false;
		}
	}

	// Open *.js with LoadFunctions
	if(fs_GetPathnames(g_VFS, dirname, "*.js", pathnames) < 0)
		return false;
	for (size_t i = 0; i < pathnames.size(); i++)
	{
		const char* pathname = pathnames[i].string().c_str();
		CVFSFile strings;
		if (! (strings.Load(pathname) == PSRETURN_OK
			&& 
			locale->LoadFunctions(
				(const char*)strings.GetBuffer(),
				strings.GetBufferSize(),
				pathname
			)))
		{
			LOG(ERROR, LOG_CATEGORY, "Error opening language function file '%s'", pathname);
			return false;
		}
	}

	// Free any previously loaded data
	delete g_CurrentLocale;

	// Store the new CLocale*, and stop the auto_ptr from deleting it
	g_CurrentLocale = locale.release();

	// Remember the name
	g_CurrentLocaleName = name;

	return true;
}

const char* I18n::CurrentLanguageName()
{
	return g_CurrentLocaleName.c_str();
}

void I18n::Shutdown()
{
	delete g_CurrentLocale;
	g_CurrentLocale = NULL;
}
