#include "precompiled.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <stdarg.h>


#include "sdl.h"
#include "ogl.h"
#include "detect.h"
#include "timer.h"
#include "input.h"
#include "lib.h"
#include "res/res.h"
#ifdef _M_IX86
#include "sysdep/ia32.h"	// _control87
#endif

#include "ps/CConsole.h"

#include "Config.h"
#include "MapReader.h"
#include "Terrain.h"
#include "TextureManager.h"
#include "ObjectManager.h"
#include "SkeletonAnimManager.h"
#include "Renderer.h"
#include "Model.h"
#include "UnitManager.h"


#include "BaseEntityCollection.h"
#include "Entity.h"
#include "EntityHandles.h"
#include "EntityManager.h"
#include "PathfindEngine.h"

#include "scripting/ScriptingHost.h"
#include "scripting/JSInterface_Entity.h"
#include "scripting/JSInterface_BaseEntity.h"
#include "scripting/JSInterface_Vector3D.h"
#include "gui/scripting/JSInterface_IGUIObject.h"
#include "gui/scripting/JSInterface_GUITypes.h"

#include "ConfigDB.h"
#include "CLogger.h"

#ifndef NO_GUI
#include "gui/GUI.h"
#endif

#include "NPFontManager.h"


CConsole* g_Console = 0;
extern int conInputHandler(const SDL_Event* ev);



u32 game_ticks;

bool keys[SDLK_LAST];
bool mouseButtons[5];

// Globals
int g_xres, g_yres;
int g_bpp;
int g_freq;
bool g_active = true;


// flag to disable extended GL extensions until fix found - specifically, crashes
// using VBOs on laptop Radeon cards
static bool g_NoGLVBO=false;
// flag to switch on shadows
static bool g_Shadows=false;
// flag to switch off pbuffers
static bool g_NoPBuffer=true;
// flag to switch on fixed frame timing (RC: I'm using this for profiling purposes)
static bool g_FixedFrameTiming=false;
static bool g_VSync = false;

static bool g_EntGraph = false;

static float g_Gamma = 1.0f;

// mapfile to load or null for no map (and to use default terrain)
static const char* g_MapFile=0;


static Handle g_Font_Console; // for the console
static Handle g_Font_Misc; // random font for miscellaneous things


extern CCamera g_Camera;

extern void terr_init();
extern void terr_update(float time);
extern int terr_handler(const SDL_Event* ev);

extern int allow_reload();
extern int dir_add_watch(const char* const dir, bool watch_subdirs);





extern void sle(int);



static size_t frameCount=0;
static bool quit = false;	// break out of main loop







const wchar_t* HardcodedErrorString(int err)
{
#define E(sym) case sym: return L ## #sym;

	switch(err)
	{
	E(ERR_NO_MEM)
	E(ERR_FILE_NOT_FOUND)
	E(ERR_INVALID_HANDLE)
	E(ERR_INVALID_PARAM)
	E(ERR_EOF)
	E(ERR_PATH_NOT_FOUND)
	E(ERR_VFS_PATH_LENGTH)
	default:
		return 0;
	}
}

const wchar_t* ErrorString(int err)
{
	// language file not available (yet)
	if(1)
		return HardcodedErrorString(err);

	// TODO: load from language file
}

static int write_sys_info();




// TODO: load from language file
// these will need to be variables; better to make an index into string table
// (as with errors)? if it's a string, what happens if lang file load failed?
#define STR_SDL_INIT_FAILED     L"SDL library initialization failed: %hs\n"
#define STR_SET_VMODE_FAILED    L"could not set %dx%d graphics mode: %hs\n"
#define STR_OGL_EXT_MISSING     L"required ARB_multitexture or ARB_texture_env_combine extension not available"
#define STR_MAP_LOAD_FAILED     L"Failed to load map %hs\n"


void Die(int err, const wchar_t* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wchar_t buf[1000];
	vswprintf(buf, 1000, fmt, args);
	va_end(args);

	const wchar_t* err_string = ErrorString(err);
	if(err_string)
	{
		wcscat(buf, L" (");	wcscat(buf, err_string); wcscat(buf, L")\n");
	}

	wdisplay_msg(L"0ad", buf);

	write_sys_info();

	debug_write_crashlog("crashlog.txt", NULL, NULL);

	exit(EXIT_FAILURE);
}



void Testing (void)
{
	g_Console->InsertMessage(L"Testing Function Registration");
}

void TestingUnicode (void)
{
	// This looks really broken in my IDE's font
	g_Console->InsertMessage(L"    Ai! lauri� lantar lassi s�rinen,");
	g_Console->InsertMessage(L"    y�ni �n�tim� ve r�mar aldaron!");
	g_Console->InsertMessage(L"    Y�ni ve lint� yuldar av�nier");
	g_Console->InsertMessage(L"    mi oromardi liss�-miruv�reva");
	g_Console->InsertMessage(L"    And�n� pella, Vardo tellumar");
	g_Console->InsertMessage(L"    nu luini yassen tintilar i eleni");
	g_Console->InsertMessage(L"    �maryo airet�ri-l�rinen.");
}




static int write_sys_info()
{
	get_gfx_info();

	struct utsname un;
	uname(&un);

	FILE* const f = fopen("../logs/system_info.txt", "w");
	if(!f)
		return -1;

	// .. OS
	fprintf(f, "%s %s (%s)\n", un.sysname, un.release, un.version);
	// .. CPU
	fprintf(f, "%s, %s", un.machine, cpu_type);
	if(cpus > 1)
		fprintf(f, " (x%d)", cpus);
	if(cpu_freq != 0.0f)
	{
		if(cpu_freq < 1e9)
			fprintf(f, ", %.2f MHz\n", cpu_freq*1e-6);
		else
			fprintf(f, ", %.2f GHz\n", cpu_freq*1e-9);
	}
	else
		fprintf(f, "\n");
	// .. memory
	fprintf(f, "%lu MB RAM; %lu MB free\n", tot_mem/MB, avl_mem/MB);
	// .. graphics card
	fprintf(f, "%s\n", gfx_card);
	fprintf(f, "%s\n", gfx_drv_ver);
	fprintf(f, "%dx%d:%d@%d\n", g_xres, g_yres, g_bpp, g_freq);
	// .. network name / ips
	//    note: can't use un.nodename because it is for an
	//    "implementation-defined communications network".
	char hostname[128];
	if (gethostname(hostname, sizeof(hostname)) == 0) // make sure it succeeded
	{
		fprintf(f, "%s\n", hostname);
		hostent* host = gethostbyname(hostname);
		if(host)
		{
			struct in_addr** ips = (struct in_addr**)host->h_addr_list;
			for(int i = 0; ips && ips[i]; i++)
				fprintf(f, "%s ", inet_ntoa(*ips[i]));
			fprintf(f, "\n");
		}
	}

	fclose(f);
	return 0;
}




static int set_vmode(int w, int h, int bpp, bool fullscreen)
{
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if(!SDL_SetVideoMode(w, h, bpp, SDL_OPENGL|(fullscreen?SDL_FULLSCREEN:0)))
		return -1;

	glViewport(0, 0, w, h);

#ifndef NO_GUI
	g_GUI.UpdateResolution();
#endif

	oglInit();	// required after each mode change

	if(SDL_SetGamma(g_Gamma, g_Gamma, g_Gamma) < 0)
		debug_warn("SDL_SetGamma failed");

	return 0;
}


static void WriteScreenshot()
{
	// determine next screenshot number.
	//
	// current approach: increment number until that file doesn't yet exist.
	// this is fairly slow, but it's typically only done once, since the last
	// number is cached. binary search shouldn't be necessary.
	//
	// known bug: after program restart, holes in the number series are
	// filled first. example: add 1st and 2nd; [exit] delete 1st; [restart]
	// add 3rd -> it gets number 1, not 3. 
	// could fix via enumerating all files, but it's not worth it ATM.
	char fn[VFS_MAX_PATH];
	static int next_num = 1;
	do
		sprintf(fn, "screenshots/screenshot%04d.png", next_num++);
	while(vfs_exists(fn));

	const int w = g_xres, h = g_yres;
	const int bpp = 24;
	const size_t size = w * h * bpp;
	void* img = mem_alloc(size);

	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, img);

	if(tex_write(fn, w, h, bpp, 0, img) < 0)
		debug_warn("WriteScreenshot: tex_write failed");

	mem_free(img);
}



// HACK: Let code from other files (i.e. the scripting system) quit
void kill_mainloop()
{
	quit = true;
}

static int handler(const SDL_Event* ev)
{
	int c;

	switch(ev->type)
	{
	case SDL_ACTIVEEVENT:
		g_active = ev->active.gain != 0;
		break;

	case SDL_KEYDOWN:
		c = ev->key.keysym.sym;
		keys[c] = true;

		switch(c)
		{
		case SDLK_ESCAPE:
			quit = true;
			break;

		case SDLK_PRINT:
			WriteScreenshot();
			break;
		}
		break;

	case SDL_KEYUP:
		c = ev->key.keysym.sym;
		keys[c] = false;
		break;
	case SDL_MOUSEBUTTONDOWN:
		c = ev->button.button;
		if( c < 5 )
			mouseButtons[c] = true;
		else
			debug_warn("SDL mouse button defs changed; fix mouseButton array def");
		break;
	case SDL_MOUSEBUTTONUP:
		c = ev->button.button;
		if( c < 5 )
			mouseButtons[c] = false;
		else
			debug_warn("SDL mouse button defs changed; fix mouseButton array def");
		break;
	}

	return EV_PASS;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderTerrain: iterate through all terrain patches and submit all patches in viewing frustum to
// the renderer
void RenderTerrain()
{
	CFrustum frustum=g_Camera.GetFustum();
	u32 patchesPerSide=g_Terrain.GetPatchesPerSide();
	for (uint j=0; j<patchesPerSide; j++) {
		for (uint i=0; i<patchesPerSide; i++) {
			CPatch* patch=g_Terrain.GetPatch(i,j);
			if (frustum.IsBoxVisible (CVector3D(0,0,0),patch->GetBounds())) {
				g_Renderer.Submit(patch);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SubmitModelRecursive: recurse down given model, submitting it and all its descendents to the
// renderer
void SubmitModelRecursive(CModel* model)
{
	g_Renderer.Submit(model);

	const std::vector<CModel::Prop>& props=model->GetProps();
	for (uint i=0;i<props.size();i++) {
		SubmitModelRecursive(props[i].m_Model);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderModels: iterate through model list and submit all models in viewing frustum to the
// Renderer
void RenderModels()
{
	CFrustum frustum=g_Camera.GetFustum();

	const std::vector<CUnit*>& units=g_UnitMan.GetUnits();
	for (uint i=0;i<units.size();++i) {
		if (frustum.IsBoxVisible(CVector3D(0,0,0),units[i]->GetModel()->GetBounds())) {
			SubmitModelRecursive(units[i]->GetModel());
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// RenderNoCull: render absolutely everything to a blank frame to force renderer
// to load required assets
void RenderNoCull()
{
	g_Renderer.BeginFrame();
	g_Renderer.SetCamera(g_Camera);

	uint i,j;
	const std::vector<CUnit*>& units=g_UnitMan.GetUnits();
	for (i=0;i<units.size();++i) {
		SubmitModelRecursive(units[i]->GetModel());
	}

	u32 patchesPerSide=g_Terrain.GetPatchesPerSide();
	for (j=0; j<patchesPerSide; j++) {
		for (i=0; i<patchesPerSide; i++) {
			CPatch* patch=g_Terrain.GetPatch(i,j);
			g_Renderer.Submit(patch);
		}
	}

	g_Renderer.FlushFrame();
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	g_Renderer.EndFrame();
}


static void Render()
{
	MICROLOG(L"begin frame");

	// start new frame
	g_Renderer.BeginFrame();
	g_Renderer.SetCamera(g_Camera);

	// switch on wireframe for terrain if we want it
	//g_Renderer.SetTerrainRenderMode( SOLID ); // (PT: If this is done here, the W key doesn't work)
	MICROLOG(L"render terrain");
	RenderTerrain();
	MICROLOG(L"render models");
	RenderModels();
	MICROLOG(L"flush frame");
	g_Renderer.FlushFrame();

	if( g_EntGraph )
	{
		glPushAttrib( GL_ENABLE_BIT );
		glDisable( GL_LIGHTING );
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_DEPTH_TEST );
		glColor3f( 1.0f, 0.0f, 1.0f );

		MICROLOG(L"render entities");
		g_EntityManager.renderAll(); // <-- collision outlines, pathing routes

		glPopAttrib();
	}

	MICROLOG(L"render fonts");
	// overlay mode
	glPushAttrib(GL_ENABLE_BIT);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.f, (float)g_xres, 0.f, (float)g_yres, -1.f, 1000.f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();


	// Use the GL_ALPHA texture as the alpha channel with a flat colouring
	glDisable(GL_ALPHA_TEST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	unifont_bind(g_Font_Misc);

	glColor4f(1.0f, 0.8f, 0.0f, 1.0f);

	glLoadIdentity();
	glTranslatef(10.0f, 10.0f, 0.0f);

	glScalef(1.0, -1.0, 1.0);
	glwprintf( L"%d FPS", fps);

#ifndef NO_GUI
	// Temp GUI message GeeTODO
	glLoadIdentity();
	glTranslatef(10, 60, 0);
	glwprintf( L"%hs", g_GUI.TEMPmessage.c_str() );

	glLoadIdentity();
	MICROLOG(L"render GUI");
	g_GUI.Draw();
#endif

	unifont_bind(g_Font_Console);
	glLoadIdentity();
	MICROLOG(L"render console");
	g_Console->Render();

	// restore
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glPopAttrib();

	MICROLOG(L"end frame");
	g_Renderer.EndFrame();
}


static void do_tick()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////
// UpdateWorld: update time dependent data in the world to account for changes over
// the given time (in s)
void UpdateWorld(float time)
{
	const std::vector<CUnit*>& units=g_UnitMan.GetUnits();
	for (uint i=0;i<units.size();++i) {
		units[i]->GetModel()->Update(time);
	}

	g_EntityManager.updateAll( time );

}


void ParseArgs(int argc, char* argv[])
{
	for (int i=1;i<argc;i++) {
		if (argv[i][0]=='-') {
			switch (argv[i][1]) {
				case 'm':
					if (argv[i][2]=='=') {
						g_MapFile=argv[i]+3;
					}
					break;
				case 'g':
					if( argv[i][2] == '=' )
					{
						g_Gamma = (float)atof( argv[i] + 3 );
						if( g_Gamma == 0.0f ) g_Gamma = 1.0f;
					}
					break;
				case 'e':
					g_EntGraph = true; break;
				case 'v':
					g_ConfigDB.CreateValue(CFG_SYSTEM, "vsync")
									->m_String="true";
					break;
				case 'n':
					if (strncmp(argv[i]+1,"novbo",5)==0) {
						g_ConfigDB.CreateValue(CFG_SYSTEM, "novbo")
									->m_String="true";
					}
					else if (strncmp(argv[i]+1,"nopbuffer",9)==0) {
						g_NoPBuffer=true;
					}
					break;

				case 'f':
					if (strncmp(argv[i]+1,"fixedframe",10)==0) {
						g_FixedFrameTiming=true;
					}
					break;

				case 's':
					if (strncmp(argv[i]+1,"shadows",7)==0) {
						g_ConfigDB.CreateValue(CFG_SYSTEM, "shadows")
									->m_String="true";
					}
					break;
				case 'x':
					if (strncmp(argv[i], "-xres=", 6)==0) {
						g_ConfigDB.CreateValue(CFG_SYSTEM, "xres")
									->m_String=argv[i]+6;
					}
					break;
				case 'y':
					if (strncmp(argv[i], "-yres=", 6)==0) {
						g_ConfigDB.CreateValue(CFG_SYSTEM, "yres")
									->m_String=argv[i]+6;
					}
					break;
				case 'c':
					if (strcmp(argv[i], "-conf") == 0) {
						if (argc-i >= 1) // At least one arg left
						{
							i++;
							char *arg=argv[i];
							char *equ=strchr(arg, '=');
							if (equ)
							{
								*equ=0;
								g_ConfigDB.CreateValue(CFG_SYSTEM, arg)
									->m_String=(equ+1);
							}
						}
					}
					break;
			}
		}
	}
	
	CConfigValue *val;
	if (val=g_ConfigDB.GetValue(CFG_SYSTEM, "xres"))
		val->GetInt(g_xres);
	if (val=g_ConfigDB.GetValue(CFG_SYSTEM, "yres"))
		val->GetInt(g_yres);

	if (val=g_ConfigDB.GetValue(CFG_SYSTEM, "vsync"))
		val->GetBool(g_VSync);
	if (val=g_ConfigDB.GetValue(CFG_SYSTEM, "novbo"))
		val->GetBool(g_NoGLVBO);
	if (val=g_ConfigDB.GetValue(CFG_SYSTEM, "shadows"))
		val->GetBool(g_Shadows);
		
	LOG(NORMAL, "g_x/yres is %dx%d", g_xres, g_yres);
}



static void InitScripting()
{
	// Create the scripting host.  This needs to be done before the GUI is created.
	new ScriptingHost;

	// Register the JavaScript interfaces with the runtime
	JSI_Entity::init();
	JSI_BaseEntity::init();
	JSI_IGUIObject::init();
	JSI_GUITypes::init();
	JSI_Vector3D::init();
}

static void InitVfs(char* argv0)
{
	// set current directory to "$game_dir/data".
	// this is necessary because it is otherwise unknown,
	// especially if run from a shortcut / symlink.
	//
	// "../data" is relative to the executable (in "$game_dir/system").
	//
	// rationale for data/ being root: untrusted scripts must not be
	// allowed to overwrite critical game (or worse, OS) files.
	// the VFS prevents any accesses to files above this directory.
	int err = file_rel_chdir(argv0, "../data");
	if(err < 0)
		throw err;
//		display_startup_error(L"error setting current directory.\n"\
//			L"argv[0] is probably incorrect. please start the game via command-line.");

	vfs_mount("", "mods/official", 0);
	vfs_mount("screenshots", "screenshots", 0);
}

static void psInit()
{
	g_Font_Console = unifont_load("console");
	g_Font_Misc = unifont_load("verdana16");
	// HACK: Cache some other fonts, because the resource manager doesn't
	unifont_load("palatino12");

	g_Console->SetSize(0, g_yres-600.f, (float)g_xres, 600.f);
	g_Console->m_iFontHeight = unifont_linespacing(g_Font_Console);
	g_Console->m_iFontOffset = 9;

#ifndef NO_GUI
	// GUI uses VFS, so this must come after VFS init.
	g_GUI.Initialize();

	g_GUI.LoadXMLFile("gui/test/styles.xml");
	g_GUI.LoadXMLFile("gui/test/hello.xml");
	g_GUI.LoadXMLFile("gui/test/sprite1.xml");

#endif
}

static void psShutdown()
{
#ifndef NO_GUI
	g_GUI.Destroy();
	delete &g_GUI;
#endif

	delete g_Console;

	// close down Xerces if it was loaded
	CXeromyces::Terminate();
}


extern u64 PREVTSC;


static void Shutdown()
{
	psShutdown(); // Must delete g_GUI before g_ScriptingHost

	delete &g_ScriptingHost;
	delete &g_Pathfinder;
	delete &g_EntityManager;
	delete &g_EntityTemplateCollection;

	// destroy actor related stuff
	delete &g_UnitMan;
	delete &g_ObjMan;
	delete &g_SkelAnimMan;

	// destroy terrain related stuff
	delete &g_TexMan;

	// destroy renderer
	delete &g_Renderer;

	delete &g_ConfigDB;

	// Not particularly consistent with all the other singletons,
	// but the GUI currently uses it and so it needs to be unloaded
	NPFontManager::release();
}


static void Init(int argc, char* argv[])
{
#ifdef _WIN32
sle(1134);
#endif

	MICROLOG(L"In init");

	// If you ever want to catch a particular allocation:
	//_CrtSetBreakAlloc(4128);

#ifdef _MSC_VER
u64 TSC=rdtsc();
debug_out(
"----------------------------------------\n"\
"MAIN (elapsed = %f ms)\n"\
"----------------------------------------\n", (TSC-PREVTSC)/2e9*1e3);
PREVTSC=TSC;
#endif


	MICROLOG(L"init lib");
	lib_init();

	// set 24 bit (float) FPU precision for faster divides / sqrts
#ifdef _M_IX86
	_control87(_PC_24, _MCW_PC);
#endif

	// Set up the console early, so that debugging
	// messages can be logged to it. (The console's size
	// and fonts are set later in psInit())
	g_Console = new CConsole();

	MICROLOG(L"detect");
	detect();

	MICROLOG(L"init vfs");
	InitVfs(argv[0]);

	MICROLOG(L"init sdl");
	// init SDL
	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE) < 0)
		Die(0, STR_SDL_INIT_FAILED, SDL_GetError());
	atexit(SDL_Quit);
	SDL_EnableUNICODE(1);

	// preferred video mode = current desktop settings
	// (command line params may override these)
	get_cur_vmode(&g_xres, &g_yres, &g_bpp, &g_freq);

	MICROLOG(L"init scripting");
	InitScripting();	// before GUI
	
	MICROLOG(L"init config");
	new CConfigDB;
	g_ConfigDB.SetConfigFile(CFG_SYSTEM, false, "config/system.cfg");
	g_ConfigDB.Reload(CFG_SYSTEM);

	g_ConfigDB.SetConfigFile(CFG_MOD, true, "config/mod.cfg");
	// No point in reloading mod.cfg here - we haven't mounted mods yet
	
	g_ConfigDB.SetConfigFile(CFG_USER, true, "config/user.cfg");
	// Same thing here; we haven't even started up yet - this will wait until
	// the profile dir is VFS mounted (or we will do a new SetConfigFile with
	// a generated profile path)
	
	ParseArgs(argc, argv);

//g_xres = 800;
//g_yres = 600;

	// GUI is notified in set_vmode, so this must come before that.
#ifndef NO_GUI
	new CGUI;
#endif

	CConfigValue *val=g_ConfigDB.GetValue(CFG_SYSTEM, "windowed");
	bool windowed=false;
	if (val) val->GetBool(windowed);

	write_sys_info();

#ifdef _WIN32
sle(11340106);
#endif

	MICROLOG(L"set vmode");

	if(set_vmode(g_xres, g_yres, 32, !windowed) < 0)
		Die(0, STR_SET_VMODE_FAILED, g_xres, g_yres, SDL_GetError());

	if(!oglExtAvail("GL_ARB_multitexture") || !oglExtAvail("GL_ARB_texture_env_combine"))
		Die(0, STR_OGL_EXT_MISSING);

	// enable/disable VSync
	// note: "GL_EXT_SWAP_CONTROL" is "historical" according to dox.
	if(oglExtAvail("WGL_EXT_swap_control"))
		wglSwapIntervalEXT(g_VSync? 1 : 0);




#ifdef _MSC_VER
u64 CURTSC=rdtsc();
debug_out(
"----------------------------------------\n"\
"low-level ready (elapsed = %f ms)\n"\
"----------------------------------------\n", (CURTSC-PREVTSC)/2e9*1e3);
PREVTSC=CURTSC;
#endif

	MICROLOG(L"init ps");
	psInit();

	// create renderer
	new CRenderer;

	// set renderer options from command line options - NOVBO must be set before opening the renderer
	g_Renderer.SetOptionBool(CRenderer::OPT_NOVBO,g_NoGLVBO);
	g_Renderer.SetOptionBool(CRenderer::OPT_SHADOWS,g_Shadows);
	g_Renderer.SetOptionBool(CRenderer::OPT_NOPBUFFER,g_NoPBuffer);

	// create terrain related stuff
	new CTextureManager;

	// create actor related stuff
	new CSkeletonAnimManager;
	new CObjectManager;
	new CUnitManager;

	MICROLOG(L"init renderer");
	g_Renderer.Open(g_xres,g_yres,g_bpp);

	// terr_init loads a bunch of resources as well as setting up the terrain
	terr_init();


	// This needs to be done after the renderer has loaded all its actors...
	new CBaseEntityCollection;
	new CEntityManager;
	new CPathfindEngine;

	g_EntityTemplateCollection.loadTemplates();

// if no map name specified, load test01.pmp (for convenience during
// development. that means loading no map at all is currently impossible.
// is that a problem?
if(!g_MapFile)
	g_MapFile = "test01.pmp";

	MICROLOG(L"init map");

	// load a map if we were given one
	if (g_MapFile) {
		CStr mapfilename("mods/official/maps/scenarios/");
		mapfilename+=g_MapFile;
		try {
			CMapReader reader;
			reader.LoadMap(mapfilename);
		} catch (...) {
			Die(0, STR_MAP_LOAD_FAILED, mapfilename.c_str());
		}
	}

	// Initialize entities

	CMessage init_msg (CMessage::EMSG_INIT);
	g_EntityManager.dispatchAll(&init_msg);

#ifndef NO_GUI
	in_add_handler(gui_handler);
#endif
	in_add_handler(handler);
	in_add_handler(terr_handler);

	in_add_handler(conInputHandler);

	MICROLOG(L"render blank");
	// render everything to a blank frame to force renderer to load everything
	RenderNoCull();

	if (g_FixedFrameTiming) {
#if 0		// TOPDOWN
		g_Camera.SetProjection(1.0f,10000.0f,DEGTORAD(90));
		g_Camera.m_Orientation.SetIdentity();
		g_Camera.m_Orientation.RotateX(DEGTORAD(90));
		g_Camera.m_Orientation.Translate(CELL_SIZE*250*0.5, 250, CELL_SIZE*250*0.5);
#else		// std view
		g_Camera.SetProjection(1.0f,10000.0f,DEGTORAD(20));
		g_Camera.m_Orientation.SetXRotation(DEGTORAD(30));
		g_Camera.m_Orientation.RotateY(DEGTORAD(-45));
		g_Camera.m_Orientation.Translate(350, 350, -275);
#endif
		g_Camera.UpdateFrustum();
	}

	g_Console->RegisterFunc(Testing, L"Testing");

	{
		wchar_t t[] = { 'T',0xE9,'s','t','i','n','g' , 0 };
		g_Console->RegisterFunc(TestingUnicode, t);
	}

#ifdef _MSC_VER
{
u64 CURTSC=rdtsc();
debug_out(
"----------------------------------------\n"\
"READY (elapsed = %f ms)\n"\
"----------------------------------------\n", (CURTSC-PREVTSC)/2e9*1e3);
PREVTSC=CURTSC;
}
#endif

}


static void Frame()
{
	MICROLOG(L"In frame");

	static double last_time;
	const double time = get_time();
	const float TimeSinceLastFrame = (float)(time-last_time);
	last_time = time;
	ONCE(return);
		// first call: set last_time and return
	assert(TimeSinceLastFrame >= 0.0f);

	MICROLOG(L"reload files");

	res_reload_changed_files();

// TODO: limiter in case simulation can't keep up?
	const double TICK_TIME = 30e-3;	// [s]
#if 0
	double time1 = get_time();
	while((time1-time0) > TICK_TIME)
	{
		game_ticks++;

		in_get_events();
		do_tick();
		time0 += TICK_TIME;
	}
#endif

	MICROLOG(L"input");

	// ugly, but necessary. these are one-shot events, have to be reset.
	mouseButtons[SDL_BUTTON_WHEELUP] = false;
	mouseButtons[SDL_BUTTON_WHEELDOWN] = false;
	in_get_events();

	if(TimeSinceLastFrame > 0.0f)
	{
		MICROLOG(L"update world");

		UpdateWorld(TimeSinceLastFrame);
		if (!g_FixedFrameTiming)
			terr_update(float(TimeSinceLastFrame));
		g_Console->Update(TimeSinceLastFrame);
	}

	if(g_active)
	{
		MICROLOG(L"render");
		Render();
		MICROLOG(L"swap buffers");
		SDL_GL_SwapBuffers();
		MICROLOG(L"finished render");
	}
	// inactive; relinquish CPU for a little while
	// don't use SDL_WaitEvent: don't want the main loop to freeze until app focus is restored
	else
		SDL_Delay(10);

	calc_fps();
	frameCount++;
	if (g_FixedFrameTiming && frameCount==100) quit=true;
}


#ifdef _WIN32
// Define/undefine this as desired:
#  define CUSTOM_EXCEPTION_HANDLER
#endif

#ifdef CUSTOM_EXCEPTION_HANDLER
#include <excpt.h>
#endif

int main(int argc, char* argv[])
{
	MICROLOG(L"In main");

#ifdef CUSTOM_EXCEPTION_HANDLER
	__try
	{
#endif
		MICROLOG(L"Init");
		Init(argc, argv);

		while(!quit)
		{
			MICROLOG(L"Frame");
			Frame();
		}

		MICROLOG(L"Shutdown");
		Shutdown();
#ifdef CUSTOM_EXCEPTION_HANDLER
	}
	__except(debug_main_exception_filter(GetExceptionCode(), GetExceptionInformation()))
	{
	}
#endif

	exit(0);
	return 0;
}
