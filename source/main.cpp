/* Copyright (C) 2025 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

/*

This module drives the game when running without Atlas (our integrated
map editor). It receives input and OS messages via SDL and feeds them
into the input dispatcher, where they are passed on to the game GUI and
simulation.
It also contains main(), which either runs the above controller or
that of Atlas depending on commandline parameters.

*/

// not for any PCH effort, but instead for the (common) definitions
// included there.
#define MINIMAL_PCH 2
#include "lib/precompiled.h"

#include "lib/debug.h"
#include "lib/status.h"
#include "lib/secure_crt.h"
#include "lib/frequency_filter.h"
#include "lib/input.h"
#include "lib/timer.h"
#include "lib/external_libraries/libsdl.h"

#include "ps/ArchiveBuilder.h"
#include "ps/CConsole.h"
#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/Filesystem.h"
#include "ps/Game.h"
#include "ps/Globals.h"
#include "ps/Hotkey.h"
#include "ps/Loader.h"
#include "ps/Mod.h"
#include "ps/ModInstaller.h"
#include "ps/Profile.h"
#include "ps/Profiler2.h"
#include "ps/Pyrogenesis.h"
#include "ps/Replay.h"
#include "ps/TouchInput.h"
#include "ps/UserReport.h"
#include "ps/Util.h"
#include "ps/VideoMode.h"
#include "ps/TaskManager.h"
#include "ps/World.h"
#include "ps/GameSetup/GameSetup.h"
#include "ps/GameSetup/Atlas.h"
#include "ps/GameSetup/Config.h"
#include "ps/GameSetup/CmdLineArgs.h"
#include "ps/GameSetup/Paths.h"
#include "ps/XML/Xeromyces.h"
#include "network/NetClient.h"
#include "network/NetServer.h"
#include "network/NetSession.h"
#include "lobby/IXmppClient.h"
#include "graphics/Camera.h"
#include "graphics/GameView.h"
#include "graphics/TextureManager.h"
#include "gui/GUIManager.h"
#include "renderer/backend/IDevice.h"
#include "renderer/Renderer.h"
#include "rlinterface/RLInterface.h"
#include "scriptinterface/ScriptContext.h"
#include "scriptinterface/ScriptEngine.h"
#include "scriptinterface/ScriptInterface.h"
#include "scriptinterface/JSON.h"
#include "simulation2/Simulation2.h"
#include "simulation2/system/TurnManager.h"
#include "soundmanager/ISoundManager.h"

#if OS_UNIX
#include <iostream>
#include <unistd.h> // geteuid
#endif // OS_UNIX

#if OS_MACOSX
#include "lib/sysdep/os/osx/osx_atlas.h"
#endif

#if MSC_VERSION
#include <process.h>
#define getpid _getpid // Use the non-deprecated function name
#endif

#if OS_WIN
// Forward declarations to avoid including Windows dependent headers.
Status waio_Shutdown();
Status wdir_watch_Init();
Status wdir_watch_Shutdown();
Status wutil_Init();
Status wutil_Shutdown();

// We don't want to include Windows.h as it might mess up the rest
// of the file so we just define DWORD as done in Windef.h.
#ifndef DWORD
typedef unsigned long DWORD;
#endif // !DWORD
// Request the high performance GPU on Windows by default if no system override is specified.
// See:
// - https://github.com/supertuxkart/stk-code/pull/4693/commits/0a99c667ef513b2ce0f5755729a6e05df8aac48a
// - https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
// - https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

#include <chrono>
#include <utility>

extern CStrW g_UniqueLogPostfix;

// Determines the lifetime of the mainloop
enum ShutdownType
{
	// The application shall continue the main loop.
	None,

	// The process shall terminate as soon as possible.
	Quit,

	// The engine should be restarted in the same process, for instance to activate different mods.
	Restart,

	// Atlas should be started in the same process.
	RestartAsAtlas
};

static ShutdownType g_Shutdown = ShutdownType::None;

// to avoid redundant and/or recursive resizing, we save the new
// size after VIDEORESIZE messages and only update the video mode
// once per frame.
// these values are the latest resize message, and reset to 0 once we've
// updated the video mode
static int g_ResizedW;
static int g_ResizedH;

static std::chrono::high_resolution_clock::time_point lastFrameTime;

bool IsQuitRequested()
{
	return g_Shutdown == ShutdownType::Quit;
}

void QuitEngine()
{
	g_Shutdown = ShutdownType::Quit;
}

void RestartEngine()
{
	g_Shutdown = ShutdownType::Restart;
}

// main app message handler
static InReaction MainInputHandler(const SDL_Event_* ev)
{
	switch(ev->ev.type)
	{
	case SDL_WINDOWEVENT:
		switch(ev->ev.window.event)
		{
		case SDL_WINDOWEVENT_RESIZED:
			g_ResizedW = ev->ev.window.data1;
			g_ResizedH = ev->ev.window.data2;
			break;
		case SDL_WINDOWEVENT_MOVED:
			g_VideoMode.UpdatePosition(ev->ev.window.data1, ev->ev.window.data2);
		}
		break;

	case SDL_QUIT:
		QuitEngine();
		break;

	case SDL_DROPFILE:
	{
		char* dropped_filedir = ev->ev.drop.file;
		const Paths paths(g_CmdLineArgs);
		CModInstaller installer(paths.UserData() / "mods", paths.Cache());
		installer.Install(std::string(dropped_filedir), g_ScriptContext, true);
		SDL_free(dropped_filedir);
		if (installer.GetInstalledMods().empty())
			LOGERROR("Failed to install mod %s", dropped_filedir);
		else
		{
			LOGMESSAGE("Installed mod %s", installer.GetInstalledMods().front());
			ScriptInterface modInterface("Engine", "Mod", g_ScriptContext);
			g_Mods.UpdateAvailableMods(modInterface);
			RestartEngine();
		}
		break;
	}

	case SDL_HOTKEYPRESS:
		std::string hotkey = static_cast<const char*>(ev->ev.user.data1);
		if (hotkey == "exit")
		{
			QuitEngine();
			return IN_HANDLED;
		}
		else if (hotkey == "screenshot")
		{
			g_Renderer.MakeScreenShotOnNextFrame(CRenderer::ScreenShotType::DEFAULT);
			return IN_HANDLED;
		}
		else if (hotkey == "bigscreenshot")
		{
			g_Renderer.MakeScreenShotOnNextFrame(CRenderer::ScreenShotType::BIG);
			return IN_HANDLED;
		}
		else if (hotkey == "togglefullscreen")
		{
			g_VideoMode.ToggleFullscreen();
			return IN_HANDLED;
		}
		else if (hotkey == "profile2.toggle")
		{
			g_Profiler2.Toggle();
			return IN_HANDLED;
		}
		else if (hotkey == "mousegrabtoggle")
		{
			SDL_Window* const window{g_VideoMode.GetWindow()};
			const SDL_bool willGrabMouse{SDL_GetWindowGrab(window) ? SDL_FALSE : SDL_TRUE};
			SDL_SetWindowGrab(window, willGrabMouse);
		}
		break;
	}

	return IN_PASS;
}


// dispatch all pending events to the various receivers.
static void PumpEvents()
{
	ScriptRequest rq(g_GUI->GetScriptInterface());

	PROFILE3("dispatch events");

	SDL_Event_ ev;
	while (in_poll_event(&ev))
	{
		PROFILE2("event");
		if (g_GUI)
		{
			JS::RootedValue tmpVal(rq.cx);
			Script::ToJSVal(rq, &tmpVal, ev);
			std::string data = Script::StringifyJSON(rq, &tmpVal);
			PROFILE2_ATTR("%s", data.c_str());
		}
		in_dispatch_event(&ev);
	}

	g_TouchInput.Frame();
}

/**
 * Optionally throttle the render frequency in order to
 * prevent 100% workload of the currently used CPU core.
 */
inline static void LimitFPS()
{
	if (g_VideoMode.IsVSyncEnabled())
		return;

	const double fpsLimit{
		g_ConfigDB.Get(g_Game && g_Game->IsGameStarted() ? "adaptivefps.session" : "adaptivefps.menu", 0.0)};

	// Keep in sync with options.json
	if (fpsLimit < 20.0 || fpsLimit >= 360.0)
		return;

	double wait = 1000.0 / fpsLimit -
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now() - lastFrameTime).count() / 1000.0;

	if (wait > 0.0)
		SDL_Delay(wait);

	lastFrameTime = std::chrono::high_resolution_clock::now();
}

static int ProgressiveLoad()
{
	PROFILE3("progressive load");

	wchar_t description[100];
	int progress_percent;
	try
	{
		Status ret = LDR_ProgressiveLoad(10e-3, description, ARRAY_SIZE(description), &progress_percent);
		switch(ret)
		{
			// no load active => no-op (skip code below)
		case INFO::OK:
			return 0;
			// current task didn't complete. we only care about this insofar as the
			// load process is therefore not yet finished.
		case ERR::TIMED_OUT:
			break;
			// just finished loading
		case INFO::ALL_COMPLETE:
			g_Game->ReallyStartGame();
			wcscpy_s(description, ARRAY_SIZE(description), L"Game is starting..");
			// LDR_ProgressiveLoad returns L""; set to valid text to
			// avoid problems in converting to JSString
			break;
			// error!
		default:
			WARN_RETURN_STATUS_IF_ERR(ret);
			// can't do this above due to legit ERR::TIMED_OUT
			break;
		}
	}
	catch (std::exception& e)
	{
		// Map loading failed

		// Call script function to do the actual work
		//	(delete game data, switch GUI page, show error, etc.)
		CancelLoad(CStr(e.what()).FromUTF8());
	}

	g_GUI->DisplayLoadProgress(progress_percent, description);
	return 0;
}


static void RendererIncrementalLoad()
{
	PROFILE3("renderer incremental load");

	const double maxTime = 0.1f;

	double startTime = timer_Time();
	bool more;
	do {
		more = g_Renderer.GetTextureManager().MakeProgress();
	}
	while (more && timer_Time() - startTime < maxTime);
}

static void Frame(RL::Interface* rlInterface, const int fixedFrameFrequency)
{
	g_Profiler2.RecordFrameStart();
	PROFILE2("frame");
	g_Profiler2.IncrementFrameNumber();
	PROFILE2_ATTR("%d", g_Profiler2.GetFrameNumber());

	// get elapsed time
	const double time = timer_Time();
	g_frequencyFilter->Update(time);
	// .. old method - "exact" but contains jumps
#if 0
	static double last_time;
	const double time = timer_Time();
	const float TimeSinceLastFrame = (float)(time-last_time);
	last_time = time;
	ONCE(return);	// first call: set last_time and return

	// .. new method - filtered and more smooth, but errors may accumulate
#else
	const float realTimeSinceLastFrame{static_cast<float>(
		1.0 / (fixedFrameFrequency > 0 ? fixedFrameFrequency : g_frequencyFilter->SmoothedFrequency()))};
#endif
	ENSURE(realTimeSinceLastFrame > 0.0f);

	// Decide if update is necessary
	const bool needUpdate{g_app_has_focus || g_NetClient || !g_PauseOnFocusLoss};

	// If we are not running a multiplayer game, disable updates when the game is
	// minimized or out of focus and relinquish the CPU a bit, in order to make
	// debugging easier.
	if (!needUpdate)
	{
		PROFILE3("non-focus delay");
		// don't use SDL_WaitEvent: don't want the main loop to freeze until app focus is restored
		SDL_Delay(10);
	}

	// this scans for changed files/directories and reloads them, thus
	// allowing hotloading (changes are immediately assimilated in-game).
	ReloadChangedFiles();

	ProgressiveLoad();

	RendererIncrementalLoad();

	PumpEvents();

	// if the user quit by closing the window, the GL context will be broken and
	// may crash when we call Render() on some drivers, so leave this loop
	// before rendering
	if (g_Shutdown != ShutdownType::None)
		return;

	// respond to pumped resize events
	if (g_ResizedW || g_ResizedH)
	{
		g_VideoMode.ResizeWindow(g_ResizedW, g_ResizedH);
		g_ResizedW = g_ResizedH = 0;
	}

	if (g_NetClient)
		g_NetClient->Poll();

	std::optional<bool> completionCommand{g_GUI->TickObjects()};
	if (completionCommand.has_value())
		g_Shutdown = completionCommand.value() ? ShutdownType::RestartAsAtlas : ShutdownType::Quit;

	if (rlInterface)
		rlInterface->TryApplyMessage();

	if (g_Game && g_Game->IsGameStarted() && needUpdate)
	{
		if (!rlInterface)
			g_Game->Update(realTimeSinceLastFrame);

		g_Game->GetView()->Update(float(realTimeSinceLastFrame));
	}

	// Keep us connected to any XMPP servers
	if (g_XmppClient)
		g_XmppClient->recv();

	g_UserReporter.Update();

	g_Console->Update(realTimeSinceLastFrame);

	if (g_SoundManager)
		g_SoundManager->IdleTask();

	g_Renderer.RenderFrame(true);

	g_Profiler.Frame();

	LimitFPS();
}

static void NonVisualFrame()
{
	g_Profiler2.RecordFrameStart();
	PROFILE2("frame");
	g_Profiler2.IncrementFrameNumber();
	PROFILE2_ATTR("%d", g_Profiler2.GetFrameNumber());

	if (g_NetClient)
		g_NetClient->Poll();

	static u32 turn = 0;
	if (g_Game && g_Game->IsGameStarted() && g_Game->GetTurnManager())
		if (g_Game->GetTurnManager()->Update(DEFAULT_TURN_LENGTH, 1))
			debug_printf("Turn %u (%u)...\n", turn++, DEFAULT_TURN_LENGTH);

	g_Profiler.Frame();

	if (g_Game->IsGameFinished())
		QuitEngine();
}

static void MainControllerInit()
{
	// add additional input handlers only needed by this controller:

	// must be registered after gui_handler. Should mayhap even be last.
	in_add_handler(MainInputHandler);
}

static void MainControllerShutdown()
{
	in_reset_handlers();
}

static std::optional<RL::Interface> CreateRLInterface(const CmdLineArgs& args)
{
	if (!args.Has("rl-interface"))
		return std::nullopt;

	const std::string server_address{args.Get("rl-interface").empty() ?
		g_ConfigDB.Get("rlinterface.address", std::string{}) : args.Get("rl-interface")};

	debug_printf("RL interface listening on %s\n", server_address.c_str());
	return std::make_optional<RL::Interface>(server_address.c_str());
}

// moved into a helper function to ensure args is destroyed before
// exit(), which may result in a memory leak.
static void RunGameOrAtlas(const PS::span<const char* const> argv)
{
	const CmdLineArgs args(argv);

	g_CmdLineArgs = args;

	if (args.Has("version"))
	{
		debug_printf("Pyrogenesis %s\n", engine_version);
		return;
	}

	if (args.Has("autostart-nonvisual") && args.Get("autostart").empty() && !args.Has("rl-interface") && !args.Has("autostart-client"))
	{
		LOGERROR("-autostart-nonvisual can't be used alone. A map with -autostart=\"TYPEDIR/MAPNAME\" is needed.");
		return;
	}

	if (args.Has("unique-logs"))
		g_UniqueLogPostfix = L"_" + std::to_wstring(std::time(nullptr)) + L"_" + std::to_wstring(getpid());

	const bool isVisualReplay = args.Has("replay-visual");
	const bool isNonVisualReplay = args.Has("replay");
	const bool isVisual = !args.Has("autostart-nonvisual");

	const int fixedFrameFrequency{args.Has("fixed-frame-frequency")
		? args.Get("fixed-frame-frequency").ToInt() : 0};

	const OsPath replayFile(
		isVisualReplay ? args.Get("replay-visual") :
		isNonVisualReplay ? args.Get("replay") : "");

	if (isVisualReplay || isNonVisualReplay)
	{
		if (!FileExists(replayFile))
		{
			debug_printf("ERROR: The requested replay file '%s' does not exist!\n", replayFile.string8().c_str());
			return;
		}
		if (DirectoryExists(replayFile))
		{
			debug_printf("ERROR: The requested replay file '%s' is a directory!\n", replayFile.string8().c_str());
			return;
		}
	}

	std::vector<OsPath> modsToInstall;
	for (const CStr& arg : args.GetArgsWithoutName())
	{
		const OsPath modPath(arg);
		if (!CModInstaller::IsDefaultModExtension(modPath.Extension()))
		{
			debug_printf("Skipping file '%s' which does not have a mod file extension.\n", modPath.string8().c_str());
			continue;
		}
		if (!FileExists(modPath))
		{
			debug_printf("ERROR: The mod file '%s' does not exist!\n", modPath.string8().c_str());
			continue;
		}
		if (DirectoryExists(modPath))
		{
			debug_printf("ERROR: The mod file '%s' is a directory!\n", modPath.string8().c_str());
			continue;
		}
		modsToInstall.emplace_back(std::move(modPath));
	}

	// We need to initialize SpiderMonkey and libxml2 in the main thread before
	// any thread uses them. So initialize them here before we might run Atlas.
	ScriptEngine scriptEngine;
	CXeromycesEngine xeromycesEngine;

	// Initialise the global task manager at this point (JS & Profiler2 are set up).
	Threading::TaskManager taskManager;

	if (ATLAS_RunIfOnCmdLine(args, false))
		return;

	if (isNonVisualReplay)
	{
		Paths paths(args);
		g_VFS = CreateVfs();
		// Mount with highest priority, we don't want mods overwriting this.
		g_VFS->Mount(L"cache/", paths.Cache(), VFS_MOUNT_ARCHIVABLE, VFS_MAX_PRIORITY);

		{
			CReplayPlayer replay;
			replay.Load(replayFile);
			replay.Replay(
				args.Has("serializationtest"),
				args.Has("rejointest") ? args.Get("rejointest").ToInt() : -1,
				args.Has("ooslog"),
				!args.Has("hashtest-full") || args.Get("hashtest-full") == "true",
				args.Has("hashtest-quick") && args.Get("hashtest-quick") == "true");
		}

		g_VFS.reset();
		return;
	}

	// run in archive-building mode if requested
	if (args.Has("archivebuild"))
	{
		Paths paths(args);

		OsPath mod(args.Get("archivebuild"));
		OsPath zip;
		if (args.Has("archivebuild-output"))
			zip = args.Get("archivebuild-output");
		else
			zip = mod.Filename().ChangeExtension(L".zip");

		CArchiveBuilder builder(mod, paths.Cache());

		// Add mods provided on the command line
		// NOTE: We do not handle mods in the user mod path here
		std::vector<CStr> mods = args.GetMultiple("mod");
		for (size_t i = 0; i < mods.size(); ++i)
			builder.AddBaseMod(paths.RData()/"mods"/mods[i]);

		builder.Build(zip, args.Has("archivebuild-compress"));
		return;
	}

	const double res = timer_Resolution();
	g_frequencyFilter = CreateFrequencyFilter(res, 30.0);

	// run the game
	bool firstIteration{true};
	do
	{
		g_Shutdown = ShutdownType::None;

		// Do this as soon as possible, because it chdirs and will mess up the error reporting if
		// anything crashes before the working directory is set.
		InitVfs(args);

		// This must come after VFS init, which sets the current directory (required for finding our
		// output log files).
		FileLogger logger;

		if (!Init(args, std::exchange(firstIteration, false) ? INIT_MODS : 0))
		{
			ShutdownConfigAndSubsequent();
			continue;
		}

		std::vector<CStr> installedMods;
		if (!modsToInstall.empty())
		{
			Paths paths(args);
			CModInstaller installer(paths.UserData() / "mods", paths.Cache());

			// Install the mods without deleting the pyromod files
			// `modsToInstall` is cleared so we don't intstall the mods again on restart.
			for (const OsPath& modPath : std::exchange(modsToInstall, {}))
			{
				CModInstaller::ModInstallationResult result = installer.Install(modPath, g_ScriptContext, true);
				if (result != CModInstaller::ModInstallationResult::SUCCESS)
					LOGERROR("Failed to install '%s'", modPath.string8().c_str());
			}

			installedMods = installer.GetInstalledMods();

			ScriptInterface modInterface("Engine", "Mod", g_ScriptContext);
			g_Mods.UpdateAvailableMods(modInterface);
		}

		std::optional<ScriptInterface> guiScriptInterface;

		if (isVisual)
		{
			guiScriptInterface.emplace("Engine", "gui", *g_ScriptContext);
			InitGraphics(args, 0, installedMods, *g_ScriptContext, *guiScriptInterface);
			MainControllerInit();
		}
		else if (!InitNonVisual(args))
			g_Shutdown = ShutdownType::Quit;

		// MSVC doesn't support copy elision in ternary expressions. So we use a lambda instead.
		std::optional<RL::Interface> rlInterface{[&]() -> std::optional<RL::Interface>
			{
				if (g_Shutdown == ShutdownType::None)
					return CreateRLInterface(args);
				else
					return std::nullopt;
			}()};

		while (g_Shutdown == ShutdownType::None)
		{
			if (isVisual)
				Frame(rlInterface ? &*rlInterface : nullptr, fixedFrameFrequency);
			else if(rlInterface)
				rlInterface->TryApplyMessage();
			else
				NonVisualFrame();
		}

		ShutdownNetworkAndUI();
		guiScriptInterface.reset();
		ShutdownConfigAndSubsequent();
		MainControllerShutdown();

	} while (g_Shutdown == ShutdownType::Restart);

#if OS_MACOSX
	if (g_Shutdown == ShutdownType::RestartAsAtlas)
		startNewAtlasProcess(g_Mods.GetEnabledMods());
#else
	if (g_Shutdown == ShutdownType::RestartAsAtlas)
		ATLAS_RunIfOnCmdLine(args, true);
#endif
}

#if OS_ANDROID
// In Android we compile the engine as a shared library, not an executable,
// so rename main() to a different symbol that the wrapper library can load
#undef main
#define main pyrogenesis_main
extern "C" __attribute__((visibility ("default"))) int main(int argc, char* argv[]);
#endif

extern "C" int main(int argc, char* argv[])
{
#if OS_UNIX
	// Don't allow people to run the game with root permissions,
	//	because bad things can happen, check before we do anything
	if (geteuid() == 0)
	{
		std::cerr << "********************************************************\n"
				  << "WARNING: Attempted to run the game with root permission!\n"
				  << "This is not allowed because it can alter home directory \n"
				  << "permissions and opens your system to vulnerabilities.   \n"
				  << "(You received this message because you were either      \n"
				  <<"  logged in as root or used e.g. the 'sudo' command.)    \n"
				  << "********************************************************\n\n";
		return EXIT_FAILURE;
	}
#endif // OS_UNIX

#if OS_WIN
	wutil_Init();
	wdir_watch_Init();
#endif

	EarlyInit();	// must come at beginning of main

	// static_cast is ok, argc is never negative.
	RunGameOrAtlas({argv, static_cast<std::size_t>(argc)});

	// Shut down profiler initialised by EarlyInit
	g_Profiler2.Shutdown();

#if OS_WIN
	// All calls to Windows specific functions have to happen before the following
	// shutdowns.
	wdir_watch_Shutdown();
	waio_Shutdown();
	wutil_Shutdown();
#endif

	return EXIT_SUCCESS;
}
