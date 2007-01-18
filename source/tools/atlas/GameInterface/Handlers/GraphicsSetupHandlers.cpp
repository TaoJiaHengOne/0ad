#include "precompiled.h"

#include "MessageHandler.h"
#include "../GameLoop.h"
#include "../CommandProc.h"
#include "../ActorViewer.h"
#include "../View.h"

#include "graphics/GameView.h"
#include "graphics/ObjectManager.h"
#include "gui/CGUI.h"
#include "gui/GUIbase.h"
#include "lib/res/file/vfs.h"
#include "lib/sdl.h"
#include "maths/MathUtil.h"
#include "ps/CConsole.h"
#include "ps/Game.h"
#include "ps/GameSetup/Config.h"
#include "ps/GameSetup/GameSetup.h"
#include "renderer/Renderer.h"

namespace AtlasMessage {

static bool g_DidInitSim;

MESSAGEHANDLER(Init)
{
	UNUSED2(msg);

#if OS_LINUX
	// When using GLX (Linux), SDL has to load the GL library to find
	// glXGetProcAddressARB before it can load any extensions.
	// When running in Atlas, we skip the SDL video initialisation code
	// which loads the library, and so SDL_GL_GetProcAddress fails (in
	// ogl.cpp importExtensionFunctions).
	// (TODO: I think this is meant to be context-independent, i.e. it
	// doesn't matter that we're getting extensions from SDL-initialised
	// GL stuff instead of from the wxWidgets-initialised GL stuff, but that
	// should be checked.)
	// So, make sure it's loaded:
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_GL_LoadLibrary(NULL); // NULL = use default
	// (it shouldn't hurt if this is called multiple times, I think)
#endif

	oglInit();

	g_Quickstart = true;

	uint flags = INIT_HAVE_VMODE|INIT_NO_GUI;
	if (! msg->initsimulation)
		flags |= INIT_NO_SIM;

	Init(g_GameLoop->args, flags);

	g_DidInitSim = msg->initsimulation; // so we can shut down the right things later

#if OS_WIN
	// HACK (to stop things looking very ugly when scrolling) - should
	// use proper config system.
	if(oglHaveExtension("WGL_EXT_swap_control"))
		pwglSwapIntervalEXT(1);
#endif
}


MESSAGEHANDLER(Shutdown)
{
	UNUSED2(msg);
	
	// Empty the CommandProc, to get rid of its references to entities before
	// we kill the EntityManager
	GetCommandProc().Destroy();

	View::DestroyViews();
	g_GameLoop->view = View::GetView_None();

	uint flags = 0;
	if (! g_DidInitSim)
		flags |= INIT_NO_SIM;
	Shutdown(flags);
}


QUERYHANDLER(Exit)
{
	UNUSED2(msg);
	g_GameLoop->running = false;
}


MESSAGEHANDLER(RenderEnable)
{
	g_GameLoop->view = View::GetView(msg->view);
}

MESSAGEHANDLER(SetViewParamB)
{
	View* view = View::GetView(msg->view);
	view->SetParam(*msg->name, msg->value);
}

MESSAGEHANDLER(SetViewParamC)
{
	View* view = View::GetView(msg->view);
	view->SetParam(*msg->name, msg->value);
}

MESSAGEHANDLER(SetActorViewer)
{
	if (msg->flushcache)
	{
		// TODO EXTREME DANGER: this'll break horribly if any units remain
		// in existence and use their actors after we've deleted all the actors.
		// (The actor viewer currently only has one unit at a time, so it's
		// alright.)
		// Should replace this with proper actor hot-loading system, or something.

		View::GetView_Actor()->GetActorViewer().SetActor(L"", L"");
		View::GetView_Actor()->GetActorViewer().UnloadObjects();
		vfs_reload_changed_files();
	}
	View::GetView_Actor()->SetSpeedMultiplier(msg->speed);
	View::GetView_Actor()->GetActorViewer().SetActor(*msg->id, *msg->animation);
}

//////////////////////////////////////////////////////////////////////////

MESSAGEHANDLER(SetCanvas)
{
	g_GameLoop->glCanvas = msg->canvas;
	printf("Setting gl canvas to %p\n", g_GameLoop->glCanvas);
	Atlas_GLSetCurrent(const_cast<void*>(g_GameLoop->glCanvas));
}


MESSAGEHANDLER(ResizeScreen)
{
	g_xres = msg->width;
	g_yres = msg->height;
	if (g_xres <= 2) g_xres = 2; // avoid GL errors caused by invalid sizes
	if (g_yres <= 2) g_yres = 2;

	SViewPort vp = { 0, 0, g_xres, g_yres };

	g_Renderer.SetViewport(vp);
	g_Renderer.Resize(g_xres, g_yres);

	g_GUI.UpdateResolution();

	g_Console->UpdateScreenSize(g_xres, g_yres);
}

//////////////////////////////////////////////////////////////////////////

MESSAGEHANDLER(RenderStyle)
{
	g_Renderer.SetTerrainRenderMode(msg->wireframe ? EDGED_FACES : SOLID);
	g_Renderer.SetModelRenderMode(msg->wireframe ? EDGED_FACES : SOLID);
}

}
