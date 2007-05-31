// Precompiled headers:

#ifndef INCLUDED_STDAFX
#define INCLUDED_STDAFX

#if defined(_MSC_VER) && _MSC_VER >= 1400
# pragma warning(disable: 6334) // TODO: what was this for?
#endif

#if HAVE_PCH

#define WX_PRECOMP

// Exclude rarely-used stuff from Windows headers
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
#endif

// Include useful wx headers
#include "wx/wxprec.h"

#include "wx/artprov.h"
#include "wx/cmdproc.h"
#include "wx/colordlg.h"
#include "wx/config.h"
#include "wx/dialog.h"
#include "wx/dir.h"
#include "wx/dnd.h"
#include "wx/docview.h"
#include "wx/file.h"
#include "wx/filename.h"
#include "wx/filesys.h"
#include "wx/glcanvas.h"
#include "wx/image.h"
#include "wx/listctrl.h"
#include "wx/mstream.h"
#include "wx/notebook.h"
#include "wx/progdlg.h"
#include "wx/regex.h"
#include "wx/sound.h"
#include "wx/spinctrl.h"
#include "wx/splitter.h"
#include "wx/tooltip.h"
#include "wx/treectrl.h"
#include "wx/wfstream.h"
#include "wx/zstream.h"

#include <vector>
#include <string>
#include <set>
#include <stack>
#include <map>
#include <limits>
#include <cassert>

#include <boost/signals.hpp>
#include <boost/bind.hpp>

// Nicer memory-leak detection:
#ifdef _WIN32
# ifdef _DEBUG
#  include <crtdbg.h>
#  define new new(_NORMAL_BLOCK ,__FILE__, __LINE__)
# endif
#endif

#endif // HAVE_PCH

#if !HAVE_PCH
// If no PCH, just include the most common headers anyway
# include "wx/wx.h"
#endif

#ifdef _WIN32
# define ATLASDLLIMPEXP extern "C" __declspec(dllexport)
#else
# if __GNUC__ >= 4
#  define ATLASDLLIMPEXP extern "C" __attribute__ ((visibility ("default")))
# else
#  define ATLASDLLIMPEXP extern "C"
# endif
#endif

// Abort with an obvious message if wx isn't Unicode, instead of complaining
// mysteriously when it first discovers wxChar != wchar_t
#ifndef UNICODE
# error This needs to be compiled with a Unicode version of wxWidgets.
#endif

#endif // INCLUDED_STDAFX
