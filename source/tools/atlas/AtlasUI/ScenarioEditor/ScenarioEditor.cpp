#include "precompiled.h"

#include "ScenarioEditor.h"

#include "wx/busyinfo.h"
#include "wx/config.h"
#include "wx/evtloop.h"
#include "wx/ffile.h"
#include "wx/filename.h"
#include "wx/image.h"
#include "wx/tooltip.h"

#include "General/AtlasEventLoop.h"
#include "General/Datafile.h"

#include "HighResTimer/HighResTimer.h"
#include "Buttons/ToolButton.h"
#include "CustomControls/Canvas/Canvas.h"

#include "GameInterface/MessagePasser.h"
#include "GameInterface/Messages.h"

#include "AtlasScript/ScriptInterface.h"

#include "Tools/Common/Tools.h"
#include "Tools/Common/Brushes.h"
#include "Tools/Common/MiscState.h"

static HighResTimer g_Timer;

using namespace AtlasMessage;

//////////////////////////////////////////////////////////////////////////

// GL functions exported from DLL, and called by game (in a separate
// thread to the standard wx one)
ATLASDLLIMPEXP void Atlas_GLSetCurrent(void* canvas)
{
	static_cast<wxGLCanvas*>(canvas)->SetCurrent();
}

ATLASDLLIMPEXP void Atlas_GLSwapBuffers(void* canvas)
{
	static_cast<wxGLCanvas*>(canvas)->SwapBuffers();
}


//////////////////////////////////////////////////////////////////////////

class GameCanvas : public Canvas
{
public:
	GameCanvas(ToolManager& toolManager, wxWindow* parent, int* attribList)
		: Canvas(parent, attribList, wxWANTS_CHARS),
		m_ToolManager(toolManager), m_MouseState(NONE), m_LastMouseState(NONE)
	{
	}

private:

	bool KeyScroll(wxKeyEvent& evt, bool enable)
	{
		int dir;
		switch (evt.GetKeyCode())
		{
		case WXK_LEFT:  dir = eScrollConstantDir::LEFT; break;
		case WXK_RIGHT: dir = eScrollConstantDir::RIGHT; break;
		case WXK_UP:    dir = eScrollConstantDir::FORWARDS; break;
		case WXK_DOWN:  dir = eScrollConstantDir::BACKWARDS; break;
		case WXK_SHIFT: case WXK_CONTROL: dir = -1; break;
		default: return false;
		}

		float speed = 120.f * ScenarioEditor::GetSpeedModifier();

		if (dir == -1) // changed modifier keys - update all currently-scrolling directions
		{
			if (wxGetKeyState(WXK_LEFT))  POST_MESSAGE(ScrollConstant, (eRenderView::GAME, eScrollConstantDir::LEFT, speed));
			if (wxGetKeyState(WXK_RIGHT)) POST_MESSAGE(ScrollConstant, (eRenderView::GAME, eScrollConstantDir::RIGHT, speed));
			if (wxGetKeyState(WXK_UP))    POST_MESSAGE(ScrollConstant, (eRenderView::GAME, eScrollConstantDir::FORWARDS, speed));
			if (wxGetKeyState(WXK_DOWN))  POST_MESSAGE(ScrollConstant, (eRenderView::GAME, eScrollConstantDir::BACKWARDS, speed));
		}
		else
		{
			POST_MESSAGE(ScrollConstant, (eRenderView::GAME, dir, enable ? speed : 0.0f));
		}

		return true;
	}

	void OnKeyDown(wxKeyEvent& evt)
	{
		if (m_ToolManager.GetCurrentTool().OnKey(evt, ITool::KEY_DOWN))
		{
			// Key event has been handled by the tool, so don't try
			// to use it for camera motion too
			return;
		}

		if (KeyScroll(evt, true))
			return;

		evt.Skip();
	}

	void OnKeyUp(wxKeyEvent& evt)
	{
		if (m_ToolManager.GetCurrentTool().OnKey(evt, ITool::KEY_UP))
			return;

		if (KeyScroll(evt, false))
			return;

		evt.Skip();
	}

	void OnChar(wxKeyEvent& evt)
	{
		if (m_ToolManager.GetCurrentTool().OnKey(evt, ITool::KEY_CHAR))
			return;

		int dir = 0;
		if (evt.GetKeyCode() == '-' || evt.GetKeyCode() == '_')
			dir = -1;
		else if (evt.GetKeyCode() == '+' || evt.GetKeyCode() == '=')
			dir = +1;
		// TODO: internationalisation (-/_ and +/= don't always share a key)

		if (dir)
		{
			float speed = 16.f;
			if (wxGetKeyState(WXK_SHIFT))
				speed *= 4.f;
			POST_MESSAGE(SmoothZoom, (eRenderView::GAME, speed*dir));
		}
		else
			evt.Skip();
	}

	virtual void HandleMouseEvent(wxMouseEvent& evt)
	{

		// TODO or at least to think about: When using other controls in the
		// editor, it's annoying that keyboard/scrollwheel no longer navigate
		// around the world until you click on it.
		// Setting focus back whenever the mouse moves over the GL window
		// feels like a fairly natural solution to me, since I can use
		// e.g. brush-editing controls normally, and then move the mouse to
		// see the brush outline and magically get given back full control
		// of the camera.
		if (evt.Moving())
			SetFocus();

		if (m_ToolManager.GetCurrentTool().OnMouse(evt))
		{
			// Mouse event has been handled by the tool, so don't try
			// to use it for camera motion too
			return;
		}

		// Global mouse event handlers (for camera motion)

		if (evt.GetWheelRotation())
		{
			float speed = 16.f * ScenarioEditor::GetSpeedModifier();
			POST_MESSAGE(SmoothZoom, (eRenderView::GAME, evt.GetWheelRotation() * speed / evt.GetWheelDelta()));
		}
		else
		{
			if (evt.MiddleIsDown())
			{
				if (wxGetKeyState(WXK_CONTROL) || evt.RightIsDown())
					m_MouseState = ROTATEAROUND;
				else
					m_MouseState = SCROLL;
			}
			else
				m_MouseState = NONE;

			if (m_MouseState != m_LastMouseState)
			{
				switch (m_MouseState)
				{
				case NONE: break;
				case SCROLL: POST_MESSAGE(Scroll, (eRenderView::GAME, eScrollType::FROM, evt.GetPosition())); break;
				case ROTATEAROUND: POST_MESSAGE(RotateAround, (eRenderView::GAME, eRotateAroundType::FROM, evt.GetPosition())); break;
				default: wxFAIL;
				}
				m_LastMouseState = m_MouseState;
			}
			else if (evt.Dragging())
			{
				switch (m_MouseState)
				{
				case NONE: break;
				case SCROLL: POST_MESSAGE(Scroll, (eRenderView::GAME, eScrollType::TO, evt.GetPosition())); break;
				case ROTATEAROUND: POST_MESSAGE(RotateAround, (eRenderView::GAME, eRotateAroundType::TO, evt.GetPosition())); break;
				default: wxFAIL;
				}
			}
		}
	}

	enum { NONE, SCROLL, ROTATEAROUND };
	int m_MouseState, m_LastMouseState;

	ToolManager& m_ToolManager;

	DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(GameCanvas, Canvas)
	EVT_KEY_DOWN(GameCanvas::OnKeyDown)
	EVT_KEY_UP(GameCanvas::OnKeyUp)
	EVT_CHAR(GameCanvas::OnChar)
END_EVENT_TABLE()

//////////////////////////////////////////////////////////////////////////

volatile bool g_FrameHasEnded;
// Called from game thread
ATLASDLLIMPEXP void Atlas_NotifyEndOfFrame()
{
	g_FrameHasEnded = true;
}

enum
{
	ID_Quit = 1,

// 	ID_New,
	ID_Open,
	ID_Save,
	ID_SaveAs,

	ID_Wireframe,
	ID_MessageTrace,
	ID_Screenshot,
	ID_JavaScript,

	ID_Toolbar // must be last in the list
};

BEGIN_EVENT_TABLE(ScenarioEditor, wxFrame)
	EVT_CLOSE(ScenarioEditor::OnClose)
	EVT_TIMER(wxID_ANY, ScenarioEditor::OnTimer)

// 	EVT_MENU(ID_New, ScenarioEditor::OnNew)
	EVT_MENU(ID_Open, ScenarioEditor::OnOpen)
	EVT_MENU(ID_Save, ScenarioEditor::OnSave)
	EVT_MENU(ID_SaveAs, ScenarioEditor::OnSaveAs)
	EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, ScenarioEditor::OnMRUFile)

	EVT_MENU(ID_Quit, ScenarioEditor::OnQuit)
	EVT_MENU(wxID_UNDO, ScenarioEditor::OnUndo)
	EVT_MENU(wxID_REDO, ScenarioEditor::OnRedo)

	EVT_MENU(ID_Wireframe, ScenarioEditor::OnWireframe)
	EVT_MENU(ID_MessageTrace, ScenarioEditor::OnMessageTrace)
	EVT_MENU(ID_Screenshot, ScenarioEditor::OnScreenshot)
	EVT_MENU(ID_JavaScript, ScenarioEditor::OnJavaScript)

	EVT_IDLE(ScenarioEditor::OnIdle)
END_EVENT_TABLE()

static AtlasWindowCommandProc g_CommandProc;
AtlasWindowCommandProc& ScenarioEditor::GetCommandProc() { return g_CommandProc; }

namespace
{
	// Wrapper function because SetCurrentTool takes an optional argument, which JS doesn't like
	void SetCurrentTool_script(void* cbdata, wxString name)
	{
		static_cast<ScenarioEditor*>(cbdata)->GetToolManager().SetCurrentTool(name);
	}
	wxString GetDataDirectory(void*)
	{
		return Datafile::GetDataDirectory();
	}
	
	// TODO: see comment in terrain.js, and remove this when/if it's no longer necessary
	void SetBrushStrength(void*, float strength)
	{
		g_Brush_Elevation.SetStrength(strength);
	}
	void SetSelectedTexture(void*, wxString name)
	{
		g_SelectedTexture = name;
	}
}

ScenarioEditor::ScenarioEditor(wxWindow* parent, ScriptInterface& scriptInterface)
: wxFrame(parent, wxID_ANY, _T(""), wxDefaultPosition, wxSize(1024, 768))
, m_FileHistory(_T("Scenario Editor")), m_ScriptInterface(scriptInterface)
, m_ObjectSettings(g_SelectedObjects, AtlasMessage::eRenderView::GAME)
, m_ToolManager(this)
{
	// Global application initialisation:

	// wxLog::SetTraceMask(wxTraceMessages);

	SetOpenFilename(_T(""));

	SetIcon(wxIcon(_T("ICON_ScenarioEditor")));

	wxToolTip::Enable(true);

	wxImage::AddHandler(new wxPNGHandler);

	//////////////////////////////////////////////////////////////////////////
	// Script interface functions
	GetScriptInterface().SetCallbackData(static_cast<void*>(this));
	GetScriptInterface().RegisterFunction<wxString, GetDataDirectory>("GetDataDirectory");
	GetScriptInterface().RegisterFunction<void, wxString, SetCurrentTool_script>("SetCurrentTool");
	GetScriptInterface().RegisterFunction<void, float, SetBrushStrength>("SetBrushStrength");
	GetScriptInterface().RegisterFunction<void, wxString, SetSelectedTexture>("SetSelectedTexture");

	{
		const wxString relativePath (_T("tools/atlas/scripts/main.js"));
		wxFileName filename (relativePath, wxPATH_UNIX);
		filename.MakeAbsolute(Datafile::GetDataDirectory());
		wxFFile file (filename.GetFullPath());
		wxString script;
		if (! file.ReadAll(&script))
			wxLogError(_("Failed to read script"));
		GetScriptInterface().LoadScript(filename.GetFullName(), script);
	}

	//////////////////////////////////////////////////////////////////////////
	// Menu

	wxMenuBar* menuBar = new wxMenuBar;
	SetMenuBar(menuBar);

	wxMenu *menuFile = new wxMenu;
	menuBar->Append(menuFile, _("&File"));
	{
// 		menuFile->Append(ID_New, _("&New"));
		menuFile->Append(ID_Open, _("&Open..."));
		menuFile->Append(ID_Save, _("&Save"));
		menuFile->Append(ID_SaveAs, _("Save &As..."));
		menuFile->AppendSeparator();//-----------
		menuFile->Append(ID_Quit,   _("E&xit"));
		m_FileHistory.UseMenu(menuFile);//-------
		m_FileHistory.AddFilesToMenu();
	}

// 	m_menuItem_Save = menuFile->FindItem(ID_Save); // remember this item, to let it be greyed out
// 	wxASSERT(m_menuItem_Save);

	wxMenu *menuEdit = new wxMenu;
	menuBar->Append(menuEdit, _("&Edit"));
	{
		menuEdit->Append(wxID_UNDO, _("&Undo"));
		menuEdit->Append(wxID_REDO, _("&Redo"));
	}

	GetCommandProc().SetEditMenu(menuEdit);
	GetCommandProc().Initialize();


	wxMenu *menuMisc = new wxMenu;
	menuBar->Append(menuMisc, _("&Misc hacks"));
	{
		menuMisc->AppendCheckItem(ID_Wireframe, _("&Wireframe"));
		menuMisc->AppendCheckItem(ID_MessageTrace, _("Message debug trace"));
		menuMisc->Append(ID_Screenshot, _("&Screenshot"));
		menuMisc->Append(ID_JavaScript, _("&JS console"));
	}

	m_FileHistory.Load(*wxConfigBase::Get());


	m_SectionLayout.SetWindow(this);

	// Toolbar:

	ToolButtonBar* toolbar = new ToolButtonBar(m_ToolManager, this, &m_SectionLayout, ID_Toolbar);
	// TODO: configurable small vs large icon images

	// (button label; tooltip text; image; internal tool name; section to switch to)
	toolbar->AddToolButton(_("Default"),       _("Default"),                   _T("default.png"),          _T(""),                 _T(""));
	toolbar->AddToolButton(_("Move"),          _("Move/rotate object"),        _T("moveobject.png"),       _T("TransformObject"),  _T("")/*_T("ObjectSidebar")*/);
	toolbar->AddToolButton(_("Elevation"),     _("Alter terrain elevation"),   _T("alterelevation.png"),   _T("AlterElevation"),   _T("")/*_T("TerrainSidebar")*/);
	toolbar->AddToolButton(_("Flatten"),       _("Flatten terrain elevation"), _T("flattenelevation.png"), _T("FlattenElevation"), _T("")/*_T("TerrainSidebar")*/);
	toolbar->AddToolButton(_("Paint Terrain"), _("Paint terrain texture"),     _T("paintterrain.png"),     _T("PaintTerrain"),     _T("")/*_T("TerrainSidebar")*/);

	toolbar->Realize();
	SetToolBar(toolbar);
	// Set the default tool to be selected
	m_ToolManager.SetCurrentTool(_T(""));


	// Set up GL canvas:

	int glAttribList[] = {
		WX_GL_RGBA,
		WX_GL_DOUBLEBUFFER,
		WX_GL_DEPTH_SIZE, 24, // TODO: wx documentation doesn't say 24 is valid
		WX_GL_BUFFER_SIZE, 24, // colour bits
		WX_GL_MIN_ALPHA, 8, // alpha bits
		0
	};
	Canvas* canvas = new GameCanvas(m_ToolManager, m_SectionLayout.GetCanvasParent(), glAttribList);
	m_SectionLayout.SetCanvas(canvas);

	// Set up sidebars:

	m_SectionLayout.Build(*this);

#if defined(__WXMSW__)
	// The canvas' context gets made current on creation; but it can only be
	// current for one thread at a time, and it needs to be current for the
	// thread that is doing the draw calls, so disable it for this one.
	wglMakeCurrent(NULL, NULL);
#elif defined(__WXGTK__)
	// Need to make sure the canvas is realized by GTK, so that its context is valid
	Show(true);
	wxSafeYield();
#endif

	// Send setup messages to game engine:

	POST_MESSAGE(SetCanvas, (static_cast<wxGLCanvas*>(canvas)));

	POST_MESSAGE(Init, (true));

	canvas->InitSize();

	// Start with a blank map (so that the editor can assume there's always
	// a valid map loaded)
	POST_MESSAGE(GenerateMap, (9));

	POST_MESSAGE(RenderEnable, (eRenderView::GAME));

	// Set up a timer to make sure tool-updates happen frequently (in addition
	// to the idle handler (which makes them happen more frequently if there's nothing
	// else to do))
	m_Timer.SetOwner(this);
	m_Timer.Start(20);

#ifdef __WXGTK__
	// HACK: because of how we fiddle with stuff earlier to make sure the canvas
	// is displayed, the layout gets messed up, and it only seems to be fixable
	// by changing the window's size
	SetSize(GetSize() + wxSize(1, 0));
#endif
}

float ScenarioEditor::GetSpeedModifier()
{
	if (wxGetKeyState(WXK_SHIFT) && wxGetKeyState(WXK_CONTROL))
		return 1.f/64.f;
	else if (wxGetKeyState(WXK_CONTROL))
		return 1.f/4.f;
	else if (wxGetKeyState(WXK_SHIFT))
		return 4.f;
	else
		return 1.f;
}


void ScenarioEditor::OnClose(wxCloseEvent&)
{
	m_ToolManager.SetCurrentTool(_T(""));

	m_FileHistory.Save(*wxConfigBase::Get());

	POST_MESSAGE(Shutdown, ());

	qExit().Post();
		// blocks until engine has noticed the message, so we won't be
		// destroying the GLCanvas while it's still rendering

	Destroy();
}


static void UpdateTool(ToolManager& toolManager)
{
	// Don't keep posting events if the game can't keep up
	if (g_FrameHasEnded)
	{
		g_FrameHasEnded = false; // (thread safety doesn't matter here)
		// TODO: Smoother timing stuff?
		static double last = g_Timer.GetTime();
		double time = g_Timer.GetTime();
		toolManager.GetCurrentTool().OnTick(time-last);
		last = time;
	}
}
void ScenarioEditor::OnTimer(wxTimerEvent&)
{
	UpdateTool(m_ToolManager);
}
void ScenarioEditor::OnIdle(wxIdleEvent&)
{
	UpdateTool(m_ToolManager);
}

void ScenarioEditor::OnQuit(wxCommandEvent&)
{
	Close();
}

void ScenarioEditor::OnUndo(wxCommandEvent&)
{
	GetCommandProc().Undo();
}

void ScenarioEditor::OnRedo(wxCommandEvent&)
{
	GetCommandProc().Redo();
}

//////////////////////////////////////////////////////////////////////////

void ScenarioEditor::OpenFile(const wxString& name)
{
	wxBusyInfo busy(_("Loading map"));
	wxBusyCursor busyc;

	// TODO: Work when the map is not in .../maps/scenarios/
	std::wstring map = name.c_str();

	// Deactivate tools, so they don't carry forwards into the new CWorld
	// and crash.
	m_ToolManager.SetCurrentTool(_T(""));
	// TODO: clear the undo buffer, etc

	POST_MESSAGE(LoadMap, (map));

	SetOpenFilename(name);

	// Wait for it to load, while the wxBusyInfo is telling the user that we're doing that
	qPing qry;
	qry.Post();

	// TODO: Make this a non-undoable command
}

// TODO (eventually): replace all this file-handling stuff with the Workspace Editor

void ScenarioEditor::OnOpen(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog dlg (NULL, wxFileSelectorPromptStr,
		Datafile::GetDataDirectory() + _T("/mods/official/maps/scenarios"), m_OpenFilename,
		_T("PMP files (*.pmp)|*.pmp|All files (*.*)|*.*"),
		wxOPEN);

	wxString cwd = wxFileName::GetCwd();
	
	if (dlg.ShowModal() == wxID_OK)
		OpenFile(dlg.GetFilename());
	
	wxCHECK_RET(cwd == wxFileName::GetCwd(), _T("cwd changed"));
		// paranoia - MSDN says OFN_NOCHANGEDIR (used when we don't give wxCHANGE_DIR)
		// "is ineffective for GetOpenFileName", but it seems to work anyway

	// TODO: Make this a non-undoable command
}

void ScenarioEditor::OnMRUFile(wxCommandEvent& event)
{
	wxString file (m_FileHistory.GetHistoryFile(event.GetId() - wxID_FILE1));
	if (file.Len())
		OpenFile(file);
}

void ScenarioEditor::OnSave(wxCommandEvent& event)
{
	if (m_OpenFilename.IsEmpty())
		OnSaveAs(event);
	else
	{
		wxBusyInfo busy(_("Saving map"));

		// Deactivate tools, so things like unit previews don't get saved.
		// (TODO: Would be nicer to leave the tools active, and just not save
		// the preview units.)
		m_ToolManager.SetCurrentTool(_T(""));

		std::wstring map = m_OpenFilename.c_str();
		POST_MESSAGE(SaveMap, (map));

		// Wait for it to finish saving
		qPing qry;
		qry.Post();
	}
}

void ScenarioEditor::OnSaveAs(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog dlg (NULL, wxFileSelectorPromptStr,
		Datafile::GetDataDirectory() + _T("/mods/official/maps/scenarios"), m_OpenFilename,
		_T("PMP files (*.pmp)|*.pmp|All files (*.*)|*.*"),
		wxSAVE | wxOVERWRITE_PROMPT);

	if (dlg.ShowModal() == wxID_OK)
	{
		wxBusyInfo busy(_("Saving map"));

		m_ToolManager.SetCurrentTool(_T(""));

		// TODO: Work when the map is not in .../maps/scenarios/
		std::wstring map = dlg.GetFilename().c_str();
		POST_MESSAGE(SaveMap, (map));

		SetOpenFilename(dlg.GetFilename());

		// Wait for it to finish saving
		qPing qry;
		qry.Post();
	}
}

void ScenarioEditor::SetOpenFilename(const wxString& filename)
{
	SetTitle(wxString::Format(_("Atlas - Scenario Editor - %s"),
		(filename.IsEmpty() ? wxString(_("(untitled)")) : filename).c_str()));

	m_OpenFilename = filename;

	if (! filename.IsEmpty())
		m_FileHistory.AddFileToHistory(filename);
}

//////////////////////////////////////////////////////////////////////////

void ScenarioEditor::OnWireframe(wxCommandEvent& event)
{
	POST_MESSAGE(RenderStyle, (event.IsChecked()));
}

void ScenarioEditor::OnMessageTrace(wxCommandEvent& event)
{
	POST_MESSAGE(MessageTrace, (event.IsChecked()));
}

void ScenarioEditor::OnScreenshot(wxCommandEvent& WXUNUSED(event))
{
	POST_MESSAGE(Screenshot, (10));
}

void ScenarioEditor::OnJavaScript(wxCommandEvent& WXUNUSED(event))
{
	wxString cmd = ::wxGetTextFromUser(_T(""), _("JS command"), _T(""), this);
	if (cmd.IsEmpty())
		return;
	POST_MESSAGE(JavaScript, (cmd.c_str()));
}

//////////////////////////////////////////////////////////////////////////

Position::Position(const wxPoint& pt)
: type(1)
{
	type1.x = pt.x;
	type1.y = pt.y;
}

//////////////////////////////////////////////////////////////////////////

/* Disabled (and should be removed if it turns out to be unnecessary)
   - see MessagePasserImpl.cpp for information

static void QueryCallback()
{
	// If this thread completely blocked on the semaphore inside Query, it would
	// never respond to window messages, and the system deadlocks if the
	// game tries to display an assertion failure dialog. (See
	// WaitForSingleObject on MSDN.)
	// So, this callback is called occasionally, and gives wx a change to
	// handle messages.

	// This is kind of like wxYield, but without the ProcessPendingEvents -
	// it's enough to make Windows happy and stop deadlocking, without actually
	// calling the event handlers (which could lead to nasty recursion)

// 	while (wxEventLoop::GetActive()->Pending())
// 		wxEventLoop::GetActive()->Dispatch();

	// Oh dear, we can't use that either - it (at least in wx 2.6.3) still
	// processes messages, which causes reentry into various things that we
	// don't want to be reentrant. So do it all manually, accepting Windows
	// messages and sticking them on a list for later processing (in a custom
	// event loop class):

	// (TODO: Rethink this entire process on Linux)
	// (Alt TODO: Could we make the game never pop up windows (or use the Win32
	// GUI in any other way) when it's running under Atlas, so we wouldn't need
	// to do any message processing here at all?)

#ifdef _WIN32
	AtlasEventLoop* evtLoop = (AtlasEventLoop*)wxEventLoop::GetActive();

	// evtLoop might be NULL, particularly if we're still initialising windows
	// and haven't got into the normal event loop yet. But we'd have to process
	// messages anyway, to avoid the deadlocks that this is for. So, don't bother
	// with that and just crash instead.
	// (Maybe it could be solved better by constructing/finding an event loop
	// object here and setting it as the global one, assuming it's not overwritten
	// later by wx.)

	while (evtLoop->Pending())
	{
		// Based on src/msw/evtloop.cpp's wxEventLoop::Dispatch()

		MSG msg;
		BOOL rc = ::GetMessage(&msg, (HWND) NULL, 0, 0);

		if (rc == 0)
		{
			// got WM_QUIT
			return;
		}

		if (rc == -1)
		{
			wxLogLastError(wxT("GetMessage"));
			return;
		}

		// Our special bits:

		if (msg.message == WM_PAINT)
		{
			// "GetMessage does not remove WM_PAINT messages from the queue.
			// The messages remain in the queue until processed."
			// So let's process them, to avoid infinite loops...
			PAINTSTRUCT paint;
			::BeginPaint(msg.hwnd, &paint);
			::EndPaint(msg.hwnd, &paint);
			// Remember that some painting was needed - we'll just repaint
			// the whole screen when this is finished.
			evtLoop->NeedsPaint();
		}
		else
		{
			// Add this message to a queue for later processing. (That's
			// probably kind of valid, at least in most cases.)
			MSG* pMsg = new MSG(msg);
			evtLoop->AddMessage(pMsg);
		}
	}
#endif
}
*/
void QueryMessage::Post()
{
//	g_MessagePasser->Query(this, &QueryCallback);
	g_MessagePasser->Query(this, NULL);
}
