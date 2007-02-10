
///////////////////////////////////////////////////////////////////////////////
//
// Name:		Renderer.cpp
// Author:		Rich Cross
// Contact:		rich@wildfiregames.com
//
// Description: OpenGL renderer class; a higher level interface
//	on top of OpenGL to handle rendering the basic visual games
//	types - terrain, models, sprites, particles etc
//
///////////////////////////////////////////////////////////////////////////////


#include "precompiled.h"

#include <map>
#include <set>
#include <algorithm>
#include "Renderer.h"
#include "graphics/Terrain.h"
#include "maths/Matrix3D.h"
#include "maths/MathUtil.h"
#include "graphics/Camera.h"
#include "graphics/Texture.h"
#include "graphics/LightEnv.h"
#include "graphics/Terrain.h"
#include "ps/Pyrogenesis.h"	// MICROLOG
#include "ps/CLogger.h"
#include "ps/Game.h"
#include "ps/Profile.h"
#include "ps/Game.h"
#include "ps/World.h"
#include "ps/Player.h"
#include "simulation/LOSManager.h"
#include "simulation/TerritoryManager.h"

#include "graphics/Model.h"
#include "graphics/ModelDef.h"

#include "lib/ogl.h"
#include "lib/path_util.h"
#include "lib/res/res.h"
#include "lib/res/file/file.h"
#include "lib/res/graphics/tex.h"
#include "lib/res/graphics/ogl_tex.h"
#include "ps/Loader.h"
#include "ps/ProfileViewer.h"

#include "graphics/GameView.h"
#include "graphics/ParticleEngine.h"
#include "graphics/DefaultEmitter.h"
#include "renderer/FixedFunctionModelRenderer.h"
#include "renderer/HWLightingModelRenderer.h"
#include "renderer/InstancingModelRenderer.h"
#include "renderer/ModelRenderer.h"
#include "renderer/PlayerRenderer.h"
#include "renderer/RenderModifiers.h"
#include "renderer/RenderPathVertexShader.h"
#include "renderer/ShadowMap.h"
#include "renderer/TerrainOverlay.h"
#include "renderer/TerrainRenderer.h"
#include "renderer/TransparencyRenderer.h"
#include "renderer/WaterManager.h"
#include "renderer/SkyManager.h"

#define LOG_CATEGORY "graphics"


///////////////////////////////////////////////////////////////////////////////////
// CRendererStatsTable - Profile display of rendering stats

/**
 * Class CRendererStatsTable: Implementation of AbstractProfileTable to
 * display the renderer stats in-game.
 *
 * Accesses CRenderer::m_Stats by keeping the reference passed to the
 * constructor.
 */
class CRendererStatsTable : public AbstractProfileTable, boost::noncopyable
{
public:
	CRendererStatsTable(const CRenderer::Stats& st);

	// Implementation of AbstractProfileTable interface
	CStr GetName();
	CStr GetTitle();
	size_t GetNumberRows();
	const std::vector<ProfileColumn>& GetColumns();
	CStr GetCellText(size_t row, size_t col);
	AbstractProfileTable* GetChild(size_t row);

private:
	/// Reference to the renderer singleton's stats
	const CRenderer::Stats& Stats;

	/// Column descriptions
	std::vector<ProfileColumn> columnDescriptions;

	enum {
		Row_Counter = 0,
		Row_DrawCalls,
		Row_TerrainTris,
		Row_ModelTris,
		Row_BlendSplats,

		// Must be last to count number of rows
		NumberRows
	};
};

// Construction
CRendererStatsTable::CRendererStatsTable(const CRenderer::Stats& st)
	: Stats(st)
{
	columnDescriptions.push_back(ProfileColumn("Name", 230));
	columnDescriptions.push_back(ProfileColumn("Value", 100));
}

// Implementation of AbstractProfileTable interface
CStr CRendererStatsTable::GetName()
{
	return "renderer";
}

CStr CRendererStatsTable::GetTitle()
{
	return "Renderer statistics";
}

size_t CRendererStatsTable::GetNumberRows()
{
	return NumberRows;
}

const std::vector<ProfileColumn>& CRendererStatsTable::GetColumns()
{
	return columnDescriptions;
}

CStr CRendererStatsTable::GetCellText(size_t row, size_t col)
{
	char buf[256];

	switch(row)
	{
	case Row_Counter:
		if (col == 0)
			return "counter";
		snprintf(buf, sizeof(buf), "%d", Stats.m_Counter);
		return buf;

	case Row_DrawCalls:
		if (col == 0)
			return "# draw calls";
		snprintf(buf, sizeof(buf), "%d", Stats.m_DrawCalls);
		return buf;

	case Row_TerrainTris:
		if (col == 0)
			return "# terrain tris";
		snprintf(buf, sizeof(buf), "%d", Stats.m_TerrainTris);
		return buf;

	case Row_ModelTris:
		if (col == 0)
			return "# model tris";
		snprintf(buf, sizeof(buf), "%d", Stats.m_ModelTris);
		return buf;

	case Row_BlendSplats:
		if (col == 0)
			return "# blend splats";
		snprintf(buf, sizeof(buf), "%d", Stats.m_BlendSplats);
		return buf;

	default:
		return "???";
	}
}

AbstractProfileTable* CRendererStatsTable::GetChild(size_t UNUSED(row))
{
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////
// CRenderer implementation

enum {
	AmbientDiffuse = 0,
	OnlyDiffuse,

	NumVertexTypes
};

/**
 * Struct CRendererInternals: Truly hide data that is supposed to be hidden
 * in this structure so it won't even appear in header files.
 */
struct CRendererInternals : public boost::noncopyable
{
	/// true if CRenderer::Open has been called
	bool IsOpen;

	/// Table to display renderer stats in-game via profile system
	CRendererStatsTable profileTable;

	/// Water manager
	WaterManager waterManager;

	/// Sky manager
	SkyManager skyManager;

	/// Terrain renderer
	TerrainRenderer* terrainRenderer;

	/// Shadow map
	ShadowMap* shadow;

	/// Various model renderers
	struct Models {
		// The following model renderers are aliases for the appropriate real_*
		// model renderers (depending on hardware availability and current settings)
		// and must be used for actual model submission and rendering
		ModelRenderer* Normal;
		ModelRenderer* NormalInstancing;
		ModelRenderer* Player;
		ModelRenderer* PlayerInstancing;
		ModelRenderer* Transp;

		// "Palette" of available ModelRenderers. Do not use these directly for
		// rendering and submission; use the aliases above instead.
		ModelRenderer* pal_NormalFF[NumVertexTypes];
		ModelRenderer* pal_PlayerFF[NumVertexTypes];
		ModelRenderer* pal_NormalHWLit[NumVertexTypes];
		ModelRenderer* pal_PlayerHWLit[NumVertexTypes];
		ModelRenderer* pal_NormalInstancing[NumVertexTypes];
		ModelRenderer* pal_PlayerInstancing[NumVertexTypes];
		ModelRenderer* pal_TranspFF[NumVertexTypes];
		ModelRenderer* pal_TranspHWLit[NumVertexTypes];
		ModelRenderer* pal_TranspSortAll;

		ModelVertexRendererPtr VertexFF[NumVertexTypes];
		ModelVertexRendererPtr VertexHWLit[NumVertexTypes];
		ModelVertexRendererPtr VertexInstancing[NumVertexTypes];
		ModelVertexRendererPtr VertexPolygonSort;

		// generic RenderModifiers that are supposed to be used directly
		RenderModifierPtr ModWireframe;
		RenderModifierPtr ModSolidColor;
		RenderModifierPtr ModTransparentShadow;
		RenderModifierPtr ModTransparentDepthShadow;

		// RenderModifiers that are selected from the palette below
		RenderModifierPtr ModNormal;
		RenderModifierPtr ModPlayer;
		RenderModifierPtr ModTransparent;

		// Palette of available RenderModifiers
		RenderModifierPtr ModPlain;
		LitRenderModifierPtr ModPlainLit;
		RenderModifierPtr ModPlayerUnlit;
		LitRenderModifierPtr ModPlayerLit;
		RenderModifierPtr ModTransparentUnlit;
		LitRenderModifierPtr ModTransparentLit;
	} Model;


	CRendererInternals()
	: IsOpen(false), profileTable(g_Renderer.m_Stats)
	{
		terrainRenderer = new TerrainRenderer();
		shadow = new ShadowMap();

		for(int vertexType = 0; vertexType < NumVertexTypes; ++vertexType)
		{
			Model.pal_NormalFF[vertexType] = 0;
			Model.pal_PlayerFF[vertexType] = 0;
			Model.pal_TranspFF[vertexType] = 0;
			Model.pal_NormalHWLit[vertexType] = 0;
			Model.pal_PlayerHWLit[vertexType] = 0;
			Model.pal_TranspHWLit[vertexType] = 0;
			Model.pal_NormalInstancing[vertexType] = 0;
			Model.pal_PlayerInstancing[vertexType] = 0;
		}
		Model.pal_TranspSortAll = 0;

		Model.Normal = 0;
		Model.NormalInstancing = 0;
		Model.Player = 0;
		Model.PlayerInstancing = 0;
		Model.Transp = 0;
	}

	~CRendererInternals()
	{
		delete shadow;
		delete terrainRenderer;
	}

	bool CanUseRenderPathVertexShader()
	{
		for(int vertexType = 0; vertexType < NumVertexTypes; ++vertexType)
		{
			if (!Model.pal_NormalHWLit[vertexType] ||
			    !Model.pal_PlayerHWLit[vertexType])
				return false;
		}

		return true;
	}

	/**
	* Load the OpenGL projection and modelview matrices and the viewport according
	* to the given camera.
	*/
	void SetOpenGLCamera(const CCamera& camera)
	{
		CMatrix3D view;
		camera.m_Orientation.GetInverse(view);
		const CMatrix3D& proj = camera.GetProjection();

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(&proj._11);

		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(&view._11);

		const SViewPort &vp = camera.GetViewPort();
		glViewport(vp.m_X,vp.m_Y,vp.m_Width,vp.m_Height);
	}
};

///////////////////////////////////////////////////////////////////////////////////
// CRenderer destructor
CRenderer::CRenderer()
{
	m = new CRendererInternals;
	m_WaterManager = &m->waterManager;
	m_SkyManager = &m->skyManager;

	g_ProfileViewer.AddRootTable(&m->profileTable);

	m_Width=0;
	m_Height=0;
	m_Depth=0;
	m_FrameCounter=0;
	m_TerrainRenderMode=SOLID;
	m_ModelRenderMode=SOLID;
	m_ClearColor[0]=m_ClearColor[1]=m_ClearColor[2]=m_ClearColor[3]=0;

	m_SortAllTransparent = false;
	m_DisplayFrustum = false;
	m_DisableCopyShadow = false;
	m_FastPlayerColor = true;
	m_SkipSubmit = false;
	m_RenderTerritories = true;

	m_VertexShader = 0;

	m_Options.m_NoVBO=false;
	m_Options.m_NoFramebufferObject = false;
	m_Options.m_Shadows=true;
	m_Options.m_RenderPath = RP_DEFAULT;
	m_Options.m_FancyWater = false;

	m_ShadowZBias = 0.02f;
	m_ShadowMapSize = 0;

	for (uint i=0;i<MaxTextureUnits;i++) {
		m_ActiveTextures[i]=0;
	}

	ONCE( ScriptingInit(); );

	AddLocalProperty(L"fancyWater", &m_Options.m_FancyWater, false);
	AddLocalProperty(L"horizonHeight", &m->skyManager.m_HorizonHeight, false);
	AddLocalProperty(L"waterMurkiness", &m->waterManager.m_Murkiness, false);
	AddLocalProperty(L"waterReflTintStrength", &m->waterManager.m_ReflectionTintStrength, false);
	AddLocalProperty(L"waterRepeatPeriod", &m->waterManager.m_RepeatPeriod, false);
	AddLocalProperty(L"waterShininess", &m->waterManager.m_Shininess, false);
	AddLocalProperty(L"waterSpecularStrength", &m->waterManager.m_SpecularStrength, false);
	AddLocalProperty(L"waterWaviness", &m->waterManager.m_Waviness, false);
}

///////////////////////////////////////////////////////////////////////////////////
// CRenderer destructor
CRenderer::~CRenderer()
{
	// model rendering
	for(int vertexType = 0; vertexType < NumVertexTypes; ++vertexType)
	{
		delete m->Model.pal_NormalFF[vertexType];
		delete m->Model.pal_PlayerFF[vertexType];
		delete m->Model.pal_TranspFF[vertexType];
		delete m->Model.pal_NormalHWLit[vertexType];
		delete m->Model.pal_PlayerHWLit[vertexType];
		delete m->Model.pal_TranspHWLit[vertexType];
		delete m->Model.pal_NormalInstancing[vertexType];
		delete m->Model.pal_PlayerInstancing[vertexType];
	}
	delete m->Model.pal_TranspSortAll;

	// general
	delete m_VertexShader;
	m_VertexShader = 0;

	CParticleEngine::GetInstance()->cleanup();

	// we no longer UnloadAlphaMaps / UnloadWaterTextures here -
	// that is the responsibility of the module that asked for
	// them to be loaded (i.e. CGameView).
	delete m;
}


///////////////////////////////////////////////////////////////////////////////////
// EnumCaps: build card cap bits
void CRenderer::EnumCaps()
{
	// assume support for nothing
	m_Caps.m_VBO=false;
	m_Caps.m_TextureBorderClamp=false;
	m_Caps.m_GenerateMipmaps=false;
	m_Caps.m_VertexShader=false;
	m_Caps.m_DepthTextureShadows = false;
	m_Caps.m_FramebufferObject = false;

	// now start querying extensions
	if (!m_Options.m_NoVBO) {
		if (oglHaveExtension("GL_ARB_vertex_buffer_object")) {
			m_Caps.m_VBO=true;
		}
	}
	if (oglHaveExtension("GL_ARB_texture_border_clamp")) {
		m_Caps.m_TextureBorderClamp=true;
	}
	if (oglHaveExtension("GL_SGIS_generate_mipmap")) {
		m_Caps.m_GenerateMipmaps=true;
	}
	if (0 == oglHaveExtensions(0, "GL_ARB_shader_objects", "GL_ARB_shading_language_100", 0))
	{
		if (oglHaveExtension("GL_ARB_vertex_shader"))
			m_Caps.m_VertexShader=true;
	}

	if (0 == oglHaveExtensions(0, "GL_ARB_shadow", "GL_ARB_depth_texture", 0)) {
		// According to Delphi3d.net, all relevant graphics chips that support depth textures
		// (i.e. Geforce3+, Radeon9500+, even i915) also have >= 4 TMUs, so this restriction
		// isn't actually a restriction, and it helps with integrating depth texture
		// shadows into rendering paths.
		if (ogl_max_tex_units >= 4)
			m_Caps.m_DepthTextureShadows = true;
	}
	if (!m_Options.m_NoFramebufferObject)
	{
		if (oglHaveExtension("GL_EXT_framebuffer_object"))
			m_Caps.m_FramebufferObject = true;
	}
}


bool CRenderer::Open(int width, int height, int depth)
{
	m->IsOpen = true;

	// Must query card capabilities before creating renderers that depend
	// on card capabilities.
	EnumCaps();
	m->shadow->SetUseDepthTexture(true);

	m_VertexShader = new RenderPathVertexShader;
	if (!m_VertexShader->Init())
	{
		delete m_VertexShader;
		m_VertexShader = 0;
	}

	// model rendering
	m->Model.VertexFF[AmbientDiffuse] = ModelVertexRendererPtr(new FixedFunctionModelRenderer(false));
	m->Model.VertexFF[OnlyDiffuse] = ModelVertexRendererPtr(new FixedFunctionModelRenderer(true));
	if (HWLightingModelRenderer::IsAvailable())
	{
		m->Model.VertexHWLit[AmbientDiffuse] = ModelVertexRendererPtr(new HWLightingModelRenderer(false));
		m->Model.VertexHWLit[OnlyDiffuse] = ModelVertexRendererPtr(new HWLightingModelRenderer(true));
	}
	if (InstancingModelRenderer::IsAvailable())
	{
		m->Model.VertexInstancing[AmbientDiffuse] = ModelVertexRendererPtr(new InstancingModelRenderer(false));
		m->Model.VertexInstancing[OnlyDiffuse] = ModelVertexRendererPtr(new InstancingModelRenderer(true));
	}
	m->Model.VertexPolygonSort = ModelVertexRendererPtr(new PolygonSortModelRenderer);

	for(int vertexType = 0; vertexType < NumVertexTypes; ++vertexType)
	{
		m->Model.pal_NormalFF[vertexType] = new BatchModelRenderer(m->Model.VertexFF[vertexType]);
		m->Model.pal_PlayerFF[vertexType] = new BatchModelRenderer(m->Model.VertexFF[vertexType]);
		m->Model.pal_TranspFF[vertexType] = new SortModelRenderer(m->Model.VertexFF[vertexType]);
		if (m->Model.VertexHWLit[vertexType])
		{
			m->Model.pal_NormalHWLit[vertexType] = new BatchModelRenderer(m->Model.VertexHWLit[vertexType]);
			m->Model.pal_PlayerHWLit[vertexType] = new BatchModelRenderer(m->Model.VertexHWLit[vertexType]);
			m->Model.pal_TranspHWLit[vertexType] = new SortModelRenderer(m->Model.VertexHWLit[vertexType]);
		}
		if (m->Model.VertexInstancing[vertexType])
		{
			m->Model.pal_NormalInstancing[vertexType] = new BatchModelRenderer(m->Model.VertexInstancing[vertexType]);
			m->Model.pal_PlayerInstancing[vertexType] = new BatchModelRenderer(m->Model.VertexInstancing[vertexType]);
		}
	}

	m->Model.pal_TranspSortAll = new SortModelRenderer(m->Model.VertexPolygonSort);

	m->Model.ModWireframe = RenderModifierPtr(new WireframeRenderModifier);
	m->Model.ModPlain = RenderModifierPtr(new PlainRenderModifier);
	m->Model.ModPlainLit = LitRenderModifierPtr(new PlainLitRenderModifier);
	SetFastPlayerColor(true);
	m->Model.ModPlayerLit = LitRenderModifierPtr(new LitPlayerColorRender);
	m->Model.ModSolidColor = RenderModifierPtr(new SolidColorRenderModifier);
	m->Model.ModTransparentUnlit = RenderModifierPtr(new TransparentRenderModifier);
	m->Model.ModTransparentLit = LitRenderModifierPtr(new LitTransparentRenderModifier);
	m->Model.ModTransparentShadow = RenderModifierPtr(new TransparentShadowRenderModifier);
	m->Model.ModTransparentDepthShadow = RenderModifierPtr(new TransparentDepthShadowModifier);

	// Particle engine
	CParticleEngine::GetInstance()->initParticleSystem();
// 	CEmitter *pEmitter = new CDefaultEmitter(1000, -1);
// 	CParticleEngine::GetInstance()->addEmitter(pEmitter);

	// Dimensions
	m_Width = width;
	m_Height = height;
	m_Depth = depth;

	// set packing parameters
	glPixelStorei(GL_PACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);

	// setup default state
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);

	GLint bits;
	glGetIntegerv(GL_DEPTH_BITS,&bits);
	LOG(NORMAL, LOG_CATEGORY, "CRenderer::Open: depth bits %d",bits);
	glGetIntegerv(GL_STENCIL_BITS,&bits);
	LOG(NORMAL, LOG_CATEGORY, "CRenderer::Open: stencil bits %d",bits);
	glGetIntegerv(GL_ALPHA_BITS,&bits);
	LOG(NORMAL, LOG_CATEGORY, "CRenderer::Open: alpha bits %d",bits);

	// Validate the currently selected render path
	SetRenderPath(m_Options.m_RenderPath);

	return true;
}

// resize renderer view
void CRenderer::Resize(int width,int height)
{
	// need to recreate the shadow map object to resize the shadow texture
	m->shadow->RecreateTexture();

	m_Width = width;
	m_Height = height;
}

//////////////////////////////////////////////////////////////////////////////////////////
// SetOptionBool: set boolean renderer option
void CRenderer::SetOptionBool(enum Option opt,bool value)
{
	switch (opt) {
		case OPT_NOVBO:
			m_Options.m_NoVBO=value;
			break;
		case OPT_NOFRAMEBUFFEROBJECT:
			m_Options.m_NoFramebufferObject=value;
			break;
		case OPT_SHADOWS:
			m_Options.m_Shadows=value;
			break;
		case OPT_FANCYWATER:
			m_Options.m_FancyWater=value;
			break;
		default:
			debug_warn("CRenderer::SetOptionBool: unknown option");
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// GetOptionBool: get boolean renderer option
bool CRenderer::GetOptionBool(enum Option opt) const
{
	switch (opt) {
		case OPT_NOVBO:
			return m_Options.m_NoVBO;
		case OPT_NOFRAMEBUFFEROBJECT:
			return m_Options.m_NoFramebufferObject;
		case OPT_SHADOWS:
			return m_Options.m_Shadows;
		case OPT_FANCYWATER:
			return m_Options.m_FancyWater;
		default:
			debug_warn("CRenderer::GetOptionBool: unknown option");
			break;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// SetOptionColor: set color renderer option
// void CRenderer::SetOptionColor(Option UNUSED(opt),const RGBAColor& UNUSED(value))
// {
// //	switch (opt) {
// //		default:
// 			debug_warn("CRenderer::SetOptionColor: unknown option");
// //			break;
// //	}
// }

void CRenderer::SetOptionFloat(enum Option opt, float val)
{
	switch(opt)
	{
	case OPT_LODBIAS:
		m_Options.m_LodBias = val;
		break;
	default:
		debug_warn("CRenderer::SetOptionFloat: unknown option");
		break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// GetOptionColor: get color renderer option
// const RGBAColor& CRenderer::GetOptionColor(Option UNUSED(opt)) const
// {
// 	static const RGBAColor defaultColor(1.0f,1.0f,1.0f,1.0f);
// 
// //	switch (opt) {
// //		default:
// 			debug_warn("CRenderer::GetOptionColor: unknown option");
// //			break;
// //	}
// 
// 	return defaultColor;
// }


//////////////////////////////////////////////////////////////////////////////////////////
// SetRenderPath: Select the preferred render path.
// This may only be called before Open(), because the layout of vertex arrays and other
// data may depend on the chosen render path.
void CRenderer::SetRenderPath(RenderPath rp)
{
	if (!m->IsOpen)
	{
		// Delay until Open() is called.
		m_Options.m_RenderPath = rp;
		return;
	}

	// Renderer has been opened, so validate the selected renderpath
	if (rp == RP_DEFAULT)
	{
		if (m->CanUseRenderPathVertexShader())
			rp = RP_VERTEXSHADER;
		else
			rp = RP_FIXED;
	}

	if (rp == RP_VERTEXSHADER)
	{
		if (!m->CanUseRenderPathVertexShader())
		{
			LOG(WARNING, LOG_CATEGORY, "Falling back to fixed function\n");
			rp = RP_FIXED;
		}
	}

	m_Options.m_RenderPath = rp;
}


CStr CRenderer::GetRenderPathName(RenderPath rp)
{
	switch(rp) {
	case RP_DEFAULT: return "default";
	case RP_FIXED: return "fixed";
	case RP_VERTEXSHADER: return "vertexshader";
	default: return "(invalid)";
	}
}

CRenderer::RenderPath CRenderer::GetRenderPathByName(const CStr& name)
{
	if (name == "fixed")
		return RP_FIXED;
	if (name == "vertexshader")
		return RP_VERTEXSHADER;
	if (name == "default")
		return RP_DEFAULT;

	LOG(WARNING, LOG_CATEGORY, "Unknown render path name '%hs', assuming 'default'", name.c_str());
	return RP_DEFAULT;
}


//////////////////////////////////////////////////////////////////////////////////////////
// SetFastPlayerColor
void CRenderer::SetFastPlayerColor(bool fast)
{
	m_FastPlayerColor = fast;

	if (m_FastPlayerColor)
	{
		if (!FastPlayerColorRender::IsAvailable())
		{
			LOG(WARNING, LOG_CATEGORY, "Falling back to slower player color rendering.");
			m_FastPlayerColor = false;
		}
	}

	if (m_FastPlayerColor)
		m->Model.ModPlayerUnlit = RenderModifierPtr(new FastPlayerColorRender);
	else
		m->Model.ModPlayerUnlit = RenderModifierPtr(new SlowPlayerColorRender);
}

//////////////////////////////////////////////////////////////////////////////////////////
// BeginFrame: signal frame start
void CRenderer::BeginFrame()
{
	// bump frame counter
	m_FrameCounter++;

	if (m_VertexShader)
		m_VertexShader->BeginFrame();

	// zero out all the per-frame stats
	m_Stats.Reset();

	// choose model renderers for this frame
	int vertexType;

	if (m_Options.m_Shadows && m->shadow->GetUseDepthTexture())
	{
		vertexType = OnlyDiffuse;
		m->Model.ModNormal = m->Model.ModPlainLit;
		m->Model.ModPlainLit->SetShadowMap(m->shadow);
		m->Model.ModPlainLit->SetLightEnv(m_LightEnv);

		m->Model.ModPlayer = m->Model.ModPlayerLit;
		m->Model.ModPlayerLit->SetShadowMap(m->shadow);
		m->Model.ModPlayerLit->SetLightEnv(m_LightEnv);

		m->Model.ModTransparent = m->Model.ModTransparentLit;
		m->Model.ModTransparentLit->SetShadowMap(m->shadow);
		m->Model.ModTransparentLit->SetLightEnv(m_LightEnv);
	}
	else
	{
		vertexType = AmbientDiffuse;
		m->Model.ModNormal = m->Model.ModPlain;
		m->Model.ModPlayer = m->Model.ModPlayerUnlit;
		m->Model.ModTransparent = m->Model.ModTransparentUnlit;
	}

	if (m_Options.m_RenderPath == RP_VERTEXSHADER)
	{
		debug_assert(m->Model.pal_NormalHWLit[vertexType] != 0);

		if (m->Model.pal_NormalInstancing)
			m->Model.NormalInstancing = m->Model.pal_NormalInstancing[vertexType];
		else
			m->Model.NormalInstancing = m->Model.pal_NormalHWLit[vertexType];
		m->Model.Normal = m->Model.pal_NormalHWLit[vertexType];

		if (m->Model.pal_PlayerInstancing)
			m->Model.PlayerInstancing = m->Model.pal_PlayerInstancing[vertexType];
		else
			m->Model.PlayerInstancing = m->Model.pal_PlayerHWLit[vertexType];
		m->Model.Player = m->Model.pal_PlayerHWLit[vertexType];
	}
	else
	{
		m->Model.NormalInstancing = m->Model.pal_NormalFF[vertexType];
		m->Model.Normal = m->Model.pal_NormalFF[vertexType];

		m->Model.PlayerInstancing = m->Model.pal_PlayerFF[vertexType];
		m->Model.Player = m->Model.pal_PlayerFF[vertexType];
	}

	if (m_SortAllTransparent)
		m->Model.Transp = m->Model.pal_TranspSortAll;
	else if (m_Options.m_RenderPath == RP_VERTEXSHADER)
		m->Model.Transp = m->Model.pal_TranspHWLit[vertexType];
	else
		m->Model.Transp = m->Model.pal_TranspFF[vertexType];
}


//////////////////////////////////////////////////////////////////////////////////////////
// SetClearColor: set color used to clear screen in BeginFrame()
void CRenderer::SetClearColor(u32 color)
{
	m_ClearColor[0]=float(color & 0xff)/255.0f;
	m_ClearColor[1]=float((color>>8) & 0xff)/255.0f;
	m_ClearColor[2]=float((color>>16) & 0xff)/255.0f;
	m_ClearColor[3]=float((color>>24) & 0xff)/255.0f;
}

void CRenderer::RenderShadowMap()
{
	PROFILE( "render shadow map" );

	m->shadow->BeginRender();

	float shadowTransp = m_LightEnv->GetTerrainShadowTransparency();
	glColor3f(shadowTransp, shadowTransp, shadowTransp);

	// Figure out transparent rendering strategy
	RenderModifierPtr transparentShadows = m->Model.ModTransparentShadow;

	if (m->shadow->GetUseDepthTexture())
		transparentShadows = m->Model.ModTransparentDepthShadow;

	// Render all closed models (i.e. models where rendering back faces will produce
	// the correct result)
	glCullFace(GL_FRONT);

	if (m->shadow->GetUseDepthTexture())
		m->terrainRenderer->RenderPatches();

	glCullFace(GL_BACK);

	// Render models that aren't closed
	glDisable(GL_CULL_FACE);

	m->Model.Normal->Render(m->Model.ModSolidColor, MODELFLAG_CASTSHADOWS);
	if (m->Model.Normal != m->Model.NormalInstancing)
		m->Model.NormalInstancing->Render(m->Model.ModSolidColor, MODELFLAG_CASTSHADOWS);
	m->Model.Player->Render(m->Model.ModSolidColor, MODELFLAG_CASTSHADOWS);
	if (m->Model.Player != m->Model.PlayerInstancing)
		m->Model.PlayerInstancing->Render(m->Model.ModSolidColor, MODELFLAG_CASTSHADOWS);

	m->Model.Transp->Render(transparentShadows, MODELFLAG_CASTSHADOWS);

	glEnable(GL_CULL_FACE);

	glColor3f(1.0, 1.0, 1.0);

	m->shadow->EndRender();
}

void CRenderer::RenderPatches()
{
	PROFILE( "render patches" );

	// switch on wireframe if we need it
	if (m_TerrainRenderMode==WIREFRAME) {
		MICROLOG(L"wireframe on");
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	}

	// render all the patches, including blend pass
	MICROLOG(L"render patch submissions");
	m->terrainRenderer->RenderTerrain(m_Options.m_Shadows ? m->shadow : 0);

	if (m_TerrainRenderMode==WIREFRAME) {
		// switch wireframe off again
		MICROLOG(L"wireframe off");
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	} else if (m_TerrainRenderMode==EDGED_FACES) {
		// edged faces: need to make a second pass over the data:
		// first switch on wireframe
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);

		// setup some renderstate ..
		glDepthMask(0);
		SetTexture(0,0);
		glColor4f(1,1,1,0.35f);
		glLineWidth(2.0f);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		// render tiles edges
		m->terrainRenderer->RenderPatches();

		// set color for outline
		glColor3f(0,0,1);
		glLineWidth(4.0f);

		// render outline of each patch
		m->terrainRenderer->RenderOutlines();

		// .. and restore the renderstates
		glDisable(GL_BLEND);
		glDepthMask(1);

		// restore fill mode, and we're done
		glLineWidth(1.0f);
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	}
}

void CRenderer::RenderModels()
{
	PROFILE( "render models ");

	// switch on wireframe if we need it
	if (m_ModelRenderMode==WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	}

	m->Model.Normal->Render(m->Model.ModNormal, 0);
	m->Model.Player->Render(m->Model.ModPlayer, 0);
	if (m->Model.Normal != m->Model.NormalInstancing)
		m->Model.NormalInstancing->Render(m->Model.ModNormal, 0);
	if (m->Model.Player != m->Model.PlayerInstancing)
		m->Model.PlayerInstancing->Render(m->Model.ModPlayer, 0);

	if (m_ModelRenderMode==WIREFRAME) {
		// switch wireframe off again
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	} else if (m_ModelRenderMode==EDGED_FACES) {
		m->Model.Normal->Render(m->Model.ModWireframe, 0);
		m->Model.Player->Render(m->Model.ModWireframe, 0);
		if (m->Model.Normal != m->Model.NormalInstancing)
			m->Model.NormalInstancing->Render(m->Model.ModWireframe, 0);
		if (m->Model.Player != m->Model.PlayerInstancing)
			m->Model.PlayerInstancing->Render(m->Model.ModWireframe, 0);
	}
}

void CRenderer::RenderTransparentModels()
{
	PROFILE( "render transparent models ");

	// switch on wireframe if we need it
	if (m_ModelRenderMode==WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	}

	m->Model.Transp->Render(m->Model.ModTransparent, 0);

	if (m_ModelRenderMode==WIREFRAME) {
		// switch wireframe off again
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	} else if (m_ModelRenderMode==EDGED_FACES) {
		m->Model.Transp->Render(m->Model.ModWireframe, 0);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// GetModelViewProjectionMatrix: save the current OpenGL model-view-projection matrix
CMatrix3D CRenderer::GetModelViewProjectionMatrix()
{
	CMatrix3D proj;
	CMatrix3D view;

	glGetFloatv( GL_PROJECTION_MATRIX, &proj._11 );
    glGetFloatv( GL_MODELVIEW_MATRIX, &view._11 );

	return( proj * view );
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// SetObliqueFrustumClipping: change the near plane to the given clip plane (in world space)
// Based on code from Game Programming Gems 5, from http://www.terathon.com/code/oblique.html
// - cp is a clip plane in camera space (cp.dot(v) = 0 for any vector v on the plane)
// - sign is 1 or -1, to specify the side to clip on
void CRenderer::SetObliqueFrustumClipping(const CVector4D& cp, int sign)
{
    float matrix[16];
    CVector4D q;

	// First, we'll convert the given clip plane to camera space, then we'll 
	// Get the view matrix and normal matrix (top 3x3 part of view matrix)
	CMatrix3D viewMatrix;
	m_ViewCamera.m_Orientation.GetInverse(viewMatrix);
	CMatrix3D normalMatrix = viewMatrix;
	normalMatrix._14 = 0;
	normalMatrix._24 = 0;
	normalMatrix._34 = 0;
	normalMatrix._44 = 1;
	normalMatrix._41 = 0;
	normalMatrix._42 = 0;
	normalMatrix._43 = 0;

	// Convert the normal to camera space
	CVector4D planeNormal(cp.m_X, cp.m_Y, cp.m_Z, 0);
	planeNormal = normalMatrix.Transform(planeNormal);
	planeNormal.normalize();

	// Find a point on the plane: we'll take the normal times -D
	float oldD = cp.m_W;
	CVector4D pointOnPlane(-oldD * cp.m_X, -oldD * cp.m_Y, -oldD * cp.m_Z, 1);
	pointOnPlane = viewMatrix.Transform(pointOnPlane);
	float newD = -pointOnPlane.dot(planeNormal);

	// Now create a clip plane from the new normal and new D
	CVector4D camPlane = planeNormal;
	camPlane.m_W = newD;

    // Grab the current projection matrix from OpenGL
    glGetFloatv(GL_PROJECTION_MATRIX, matrix);
    
    // Calculate the clip-space corner point opposite the clipping plane
    // as (sgn(camPlane.x), sgn(camPlane.y), 1, 1) and
    // transform it into camera space by multiplying it
    // by the inverse of the projection matrix
    
    q.m_X = (sgn(camPlane.m_X) + matrix[8]) / matrix[0];
    q.m_Y = (sgn(camPlane.m_Y) + matrix[9]) / matrix[5];
    q.m_Z = -1.0f;
    q.m_W = (1.0f + matrix[10]) / matrix[14];
    
    // Calculate the scaled plane vector
    CVector4D c = camPlane * (sign * 2.0f / camPlane.dot(q));
    
    // Replace the third row of the projection matrix
    matrix[2] = c.m_X;
    matrix[6] = c.m_Y;
    matrix[10] = c.m_Z + 1.0f;
    matrix[14] = c.m_W;

    // Load it back into OpenGL
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(matrix);

	glMatrixMode(GL_MODELVIEW);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// RenderReflections: render the water reflections to the reflection texture
void CRenderer::RenderReflections()
{
	MICROLOG(L"render reflections");

	WaterManager& wm = m->waterManager;

	// Remember old camera
	CCamera normalCamera = m_ViewCamera;

	// Temporarily change the camera to one that is reflected.
	// Also, for texturing purposes, make it render to a view port the size of the
	// water texture, stretch the image according to our aspect ratio so it covers
	// the whole screen despite being rendered into a square, and cover slightly more
	// of the view so we can see wavy reflections of slightly off-screen objects.
	m_ViewCamera.m_Orientation.Translate(0, -wm.m_WaterHeight, 0);
	m_ViewCamera.m_Orientation.Scale(1, -1, 1);
	m_ViewCamera.m_Orientation.Translate(0, wm.m_WaterHeight, 0);
	SViewPort vp;
	vp.m_Height = wm.m_ReflectionTextureSize;
	vp.m_Width = wm.m_ReflectionTextureSize;
	vp.m_X = 0;
	vp.m_Y = 0;
	m_ViewCamera.SetViewPort(&vp);
	m_ViewCamera.SetProjection(CGameView::defaultNear, CGameView::defaultFar, CGameView::defaultFOV*1.05f); // Slightly higher than view FOV
	CMatrix3D scaleMat;
	scaleMat.SetScaling(m_Height/float(std::max(1, m_Width)), 1.0f, 1.0f);
	m_ViewCamera.m_ProjMat = scaleMat * m_ViewCamera.m_ProjMat;

	m->SetOpenGLCamera(m_ViewCamera);

	CVector4D camPlane(0, 1, 0, -wm.m_WaterHeight);
	SetObliqueFrustumClipping(camPlane, -1);

	// Save the model-view-projection matrix so the shaders can use it for projective texturing
	wm.m_ReflectionMatrix = GetModelViewProjectionMatrix();

	// Disable backface culling so trees render properly (it might also be possible to flip
	// the culling direction here, but this seems to lead to problems)
	glDisable(GL_CULL_FACE);

	// Make the depth buffer work backwards; there seems to be some oddness with 
	// oblique frustum clipping and the "sign" parameter here
	glClearDepth(0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glDepthFunc(GL_GEQUAL);

	// Render sky, terrain and models
	m->skyManager.RenderSky();
	oglCheck();
	RenderPatches();
	oglCheck();
	RenderModels();
	oglCheck();
	RenderTransparentModels();
	oglCheck();

	// Copy the image to a texture
	pglActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, wm.m_ReflectionTexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
		wm.m_ReflectionTextureSize, wm.m_ReflectionTextureSize);

	//Reset old camera and re-enable backface culling
	m_ViewCamera = normalCamera;
	m->SetOpenGLCamera(m_ViewCamera);

	glEnable(GL_CULL_FACE);
	//glClearDepth(1);
	//glClear(GL_DEPTH_BUFFER_BIT);
	//glDepthFunc(GL_LEQUAL);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// RenderRefractions: render the water refractions to the refraction texture
void CRenderer::RenderRefractions()
{
	MICROLOG(L"render refractions");

	WaterManager& wm = m->waterManager;

	// Remember old camera
	CCamera normalCamera = m_ViewCamera;

	// Temporarily change the camera to make it render to a view port the size of the
	// water texture, stretch the image according to our aspect ratio so it covers
	// the whole screen despite being rendered into a square, and cover slightly more
	// of the view so we can see wavy refractions of slightly off-screen objects.
	SViewPort vp;
	vp.m_Height = wm.m_RefractionTextureSize;
	vp.m_Width = wm.m_RefractionTextureSize;
	vp.m_X = 0;
	vp.m_Y = 0;
	m_ViewCamera.SetViewPort(&vp);
	m_ViewCamera.SetProjection(CGameView::defaultNear, CGameView::defaultFar, CGameView::defaultFOV*1.05f); // Slightly higher than view FOV
	CMatrix3D scaleMat;
	scaleMat.SetScaling(m_Height/float(std::max(1, m_Width)), 1.0f, 1.0f);
	m_ViewCamera.m_ProjMat = scaleMat * m_ViewCamera.m_ProjMat;
	m->SetOpenGLCamera(m_ViewCamera);

	CVector4D camPlane(0, 1, 0, -wm.m_WaterHeight);
	SetObliqueFrustumClipping(camPlane, -1);

	// Save the model-view-projection matrix so the shaders can use it for projective texturing
	wm.m_RefractionMatrix = GetModelViewProjectionMatrix();

	// Make the depth buffer work backwards; there seems to be some oddness with 
	// oblique frustum clipping and the "sign" parameter here
	glClearDepth(0);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);	// a neutral gray to blend in with shores
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthFunc(GL_GEQUAL);

	// Render terrain and models
	RenderPatches();
	oglCheck();
	RenderModels();
	oglCheck();
	RenderTransparentModels();
	oglCheck();

	// Copy the image to a texture
	pglActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, wm.m_RefractionTexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
		wm.m_RefractionTextureSize, wm.m_RefractionTextureSize);

	//Reset old camera and re-enable backface culling
	m_ViewCamera = normalCamera;
	m->SetOpenGLCamera(m_ViewCamera);

	glEnable(GL_CULL_FACE);
	glClearDepth(1);
	glDepthFunc(GL_LEQUAL);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// RenderSubmissions: force rendering of any batched objects
void CRenderer::RenderSubmissions()
{
	oglCheck();

	// Set the camera
	m->SetOpenGLCamera(m_ViewCamera);

	// Prepare model renderers
	PROFILE_START("prepare models");
	m->Model.Normal->PrepareModels();
	m->Model.Player->PrepareModels();
	if (m->Model.Normal != m->Model.NormalInstancing)
		m->Model.NormalInstancing->PrepareModels();
	if (m->Model.Player != m->Model.PlayerInstancing)
		m->Model.PlayerInstancing->PrepareModels();
	m->Model.Transp->PrepareModels();
	PROFILE_END("prepare models");

	PROFILE_START("prepare terrain");
	m->terrainRenderer->PrepareForRendering();
	PROFILE_END("prepare terrain");

	if (m_Options.m_Shadows)
	{
		MICROLOG(L"render shadows");
		RenderShadowMap();
	}

	// clear buffers
	glClearColor(m_ClearColor[0],m_ClearColor[1],m_ClearColor[2],m_ClearColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	oglCheck();

	if (m_WaterManager->m_RenderWater && m_Options.m_FancyWater)
	{
		// render reflected and refracted scenes, then re-clear the screen
		RenderReflections();
		RenderRefractions();
		glClearColor(m_ClearColor[0],m_ClearColor[1],m_ClearColor[2],m_ClearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	// render sky; this is done before everything so that 
	// (a) we can use a box around the camera instead of placing it "infinitely far away"
	//     (we just disable depth write so that this doesn't affect future rendering)
	// (b) transparent objects properly overlap the sky
	if (m_SkyManager->m_RenderSky)
	{
		MICROLOG(L"render sky");
		m->skyManager.RenderSky();
		oglCheck();
	}

	// render submitted patches and models
	MICROLOG(L"render patches");
	RenderPatches();
	oglCheck();

	if (g_Game && m_RenderTerritories)
	{
		g_Game->GetWorld()->GetTerritoryManager()->renderTerritories();
		oglCheck();
	}

	// render debug-related terrain overlays
	TerrainOverlay::RenderOverlays();
	oglCheck();

	MICROLOG(L"render models");
	RenderModels();
	oglCheck();

	// render transparent stuff, so it can overlap models/terrain
	MICROLOG(L"render transparent");
	RenderTransparentModels();
	oglCheck();

	// render water
	if (m_WaterManager->m_RenderWater && g_Game)
	{
		MICROLOG(L"render water");
		m->terrainRenderer->RenderWater();
		oglCheck();
		
		// render transparent stuff again, so it can overlap the water
		MICROLOG(L"render transparent 2");
		RenderTransparentModels();
		oglCheck();
		
		// TODO: Maybe think of a better way to deal with transparent objects;
		// they can appear both under and above water (seaweed vs. trees), but doing
		// 2 renders causes (a) inefficiency and (b) darker over-water objects (e.g.
		// trees) than usual because the transparent bits get overwritten twice.
		// This doesn't look particularly bad, but it is noticeable if you try
		// turning the water off. On the other hand every user will have water
		// on all the time, so it might not be worth worrying about.
	}

	// Clean up texture blend mode so particles and other things render OK 
	// (really this should be cleaned up by whoever set it)
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	//// Particle Engine Rendering.
	MICROLOG(L"render particles");
	CParticleEngine::GetInstance()->renderParticles();
	oglCheck();

	// render debug lines
	if (m_DisplayFrustum)
	{
		MICROLOG(L"display frustum");
		DisplayFrustum();
		m->shadow->RenderDebugDisplay();
		oglCheck();
	}

	// empty lists
	MICROLOG(L"empty lists");
	m->terrainRenderer->EndFrame();

	// Finish model renderers
	m->Model.Normal->EndFrame();
	m->Model.Player->EndFrame();
	if (m->Model.Normal != m->Model.NormalInstancing)
		m->Model.NormalInstancing->EndFrame();
	if (m->Model.Player != m->Model.PlayerInstancing)
		m->Model.PlayerInstancing->EndFrame();
	m->Model.Transp->EndFrame();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// EndFrame: signal frame end
void CRenderer::EndFrame()
{
	g_Renderer.SetTexture(0,0);

	static bool once=false;
	if (!once && glGetError()) {
		LOG(ERROR, LOG_CATEGORY, "CRenderer::EndFrame: GL errors occurred");
		once=true;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// DisplayFrustum: debug displays
//  - white: cull camera frustum
//  - red: bounds of shadow casting objects
void CRenderer::DisplayFrustum()
{
	glDepthMask(0);
	glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4ub(255,255,255,64);
	m_CullCamera.Render(2);
	glDisable(GL_BLEND);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glColor3ub(255,255,255);
	m_CullCamera.Render(2);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glEnable(GL_CULL_FACE);
	glDepthMask(1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SetSceneCamera: setup projection and transform of camera and adjust viewport to current view
// The camera always represents the actual camera used to render a scene, not any virtual camera
// used for shadow rendering or reflections.
void CRenderer::SetSceneCamera(const CCamera& viewCamera, const CCamera& cullCamera)
{
	m_ViewCamera = viewCamera;
	m_CullCamera = cullCamera;

	m->shadow->SetupFrame(m_CullCamera, m_LightEnv->GetSunDir());
}


void CRenderer::SetViewport(const SViewPort &vp)
{
	glViewport(vp.m_X,vp.m_Y,vp.m_Width,vp.m_Height);
}

void CRenderer::Submit(CPatch* patch)
{
	m->terrainRenderer->Submit(patch);
}

void CRenderer::SubmitNonRecursive(CModel* model)
{
	if (model->GetFlags() & MODELFLAG_CASTSHADOWS) {
		PROFILE( "updating shadow bounds" );
		m->shadow->AddShadowedBound(model->GetBounds());
	}

	// Tricky: The call to GetBounds() above can invalidate the position
	model->ValidatePosition();

	bool canUseInstancing = false;

	if (model->GetModelDef()->GetNumBones() == 0)
		canUseInstancing = true;

	if (model->GetMaterial().IsPlayer())
	{
		if (canUseInstancing)
			m->Model.PlayerInstancing->Submit(model);
		else
			m->Model.Player->Submit(model);
	}
	else if (model->GetMaterial().UsesAlpha())
	{
		m->Model.Transp->Submit(model);
	}
	else
	{
		if (canUseInstancing)
			m->Model.NormalInstancing->Submit(model);
		else
			m->Model.Normal->Submit(model);
	}
}


///////////////////////////////////////////////////////////
// Render the given scene
void CRenderer::RenderScene(Scene *scene)
{
	CFrustum frustum = m_CullCamera.GetFrustum();

	MICROLOG(L"collect objects");
	scene->EnumerateObjects(frustum, this);

	oglCheck();

	MICROLOG(L"flush objects");
	RenderSubmissions();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LoadTexture: try and load the given texture; set clamp/repeat flags on texture object if necessary
bool CRenderer::LoadTexture(CTexture* texture,u32 wrapflags)
{
	const Handle errorhandle = -1;

	Handle h = texture->GetHandle();
	// already tried to load this texture
	if (h)
	{
		// nothing to do here - just return success according to
		// whether this is a valid handle or not
		return h==errorhandle ? true : false;
	}

	h = ogl_tex_load(texture->GetName());
	if (h <= 0)
	{
		LOG(ERROR, LOG_CATEGORY, "LoadTexture failed on \"%s\"",(const char*) texture->GetName());
		texture->SetHandle(errorhandle);
		return false;
	}

	if(!wrapflags)
		wrapflags = GL_CLAMP_TO_EDGE;
	(void)ogl_tex_set_wrap(h, wrapflags);
	(void)ogl_tex_set_filter(h, GL_LINEAR_MIPMAP_LINEAR);

	// (this also verifies that the texture is a power-of-two)
	if(ogl_tex_upload(h) < 0)
	{
		LOG(ERROR, LOG_CATEGORY, "LoadTexture failed on \"%s\" : upload failed",(const char*) texture->GetName());
		ogl_tex_free(h);
		texture->SetHandle(errorhandle);
		return false;
	}

	texture->SetHandle(h);
	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BindTexture: bind a GL texture object to current active unit
void CRenderer::BindTexture(int unit,GLuint tex)
{
	pglActiveTextureARB(GL_TEXTURE0+unit);

	glBindTexture(GL_TEXTURE_2D,tex);
	if (tex) {
		glEnable(GL_TEXTURE_2D);
	} else {
		glDisable(GL_TEXTURE_2D);
	}
	m_ActiveTextures[unit]=tex;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SetTexture: set the given unit to reference the given texture; pass a null texture to disable texturing on any unit
void CRenderer::SetTexture(int unit,CTexture* texture)
{
	Handle h = texture? texture->GetHandle() : 0;

	// Errored textures will give a handle of -1
	if (h == -1)
		h = 0;

	ogl_tex_bind(h, unit);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IsTextureTransparent: return true if given texture is transparent, else false - note texture must be loaded
// beforehand
bool CRenderer::IsTextureTransparent(CTexture* texture)
{
	if (!texture) return false;
	Handle h=texture->GetHandle();

	uint flags = 0;	// assume no alpha on failure
	(void)ogl_tex_get_format(h, &flags, 0);
	return (flags & TEX_ALPHA) != 0;
}

static inline void CopyTriple(unsigned char* dst,const unsigned char* src)
{
	dst[0]=src[0];
	dst[1]=src[1];
	dst[2]=src[2];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// LoadAlphaMaps: load the 14 default alpha maps, pack them into one composite texture and
// calculate the coordinate of each alphamap within this packed texture
int CRenderer::LoadAlphaMaps()
{
	const char* const key = "(alpha map composite)";
	Handle ht = ogl_tex_find(key);
	// alpha map texture had already been created and is still in memory:
	// reuse it, do not load again.
	if(ht > 0)
	{
		m_hCompositeAlphaMap = ht;
		return 0;
	}

	//
	// load all textures and store Handle in array
	//
	Handle textures[NumAlphaMaps] = {0};
	PathPackage pp;
	(void)path_package_set_dir(&pp, "art/textures/terrain/alphamaps/special");
	const char* fnames[NumAlphaMaps] = {
		"blendcircle.dds",
		"blendlshape.dds",
		"blendedge.dds",
		"blendedgecorner.dds",
		"blendedgetwocorners.dds",
		"blendfourcorners.dds",
		"blendtwooppositecorners.dds",
		"blendlshapecorner.dds",
		"blendtwocorners.dds",
		"blendcorner.dds",
		"blendtwoedges.dds",
		"blendthreecorners.dds",
		"blendushape.dds",
		"blendbad.dds"
	};
	uint base = 0;	// texture width/height (see below)
	// for convenience, we require all alpha maps to be of the same BPP
	// (avoids another ogl_tex_get_size call, and doesn't hurt)
	uint bpp = 0;
	for(uint i=0;i<NumAlphaMaps;i++)
	{
		(void)path_package_append_file(&pp, fnames[i]);
		// note: these individual textures can be discarded afterwards;
		// we cache the composite.
		textures[i] = ogl_tex_load(pp.path, RES_NO_CACHE);
		RETURN_ERR(textures[i]);

// quick hack: we require plain RGB(A) format, so convert to that.
// ideally the texture would be in uncompressed form; then this wouldn't
// be necessary.
uint flags;
ogl_tex_get_format(textures[i], &flags, 0);
ogl_tex_transform_to(textures[i], flags & ~TEX_DXT);

		// get its size and make sure they are all equal.
		// (the packing algo assumes this)
		uint this_width = 0, this_bpp = 0;	// fail-safe
		(void)ogl_tex_get_size(textures[i], &this_width, 0, &this_bpp);
		// .. first iteration: establish size
		if(i == 0)
		{
			base = this_width;
			bpp  = this_bpp;
		}
		// .. not first: make sure texture size matches
		else if(base != this_width || bpp != this_bpp)
			DISPLAY_ERROR(L"Alpha maps are not identically sized (including pixel depth)");
	}

	//
	// copy each alpha map (tile) into one buffer, arrayed horizontally.
	//
	uint tile_w = 2+base+2;	// 2 pixel border (avoids bilinear filtering artifacts)
	uint total_w = RoundUpToPowerOf2(tile_w * NumAlphaMaps);
	uint total_h = base; debug_assert(is_pow2(total_h));
	u8* data=new u8[total_w*total_h*3];
	// for each tile on row
	for(uint i=0;i<NumAlphaMaps;i++)
	{
		// get src of copy
		const u8* src = 0;
		(void)ogl_tex_get_data(textures[i], (void**)&src);

		uint srcstep=bpp/8;

		// get destination of copy
		u8* dst=data+3*(i*tile_w);

		// for each row of image
		for (uint j=0;j<base;j++) {
			// duplicate first pixel
			CopyTriple(dst,src);
			dst+=3;
			CopyTriple(dst,src);
			dst+=3;

			// copy a row
			for (uint k=0;k<base;k++) {
				CopyTriple(dst,src);
				dst+=3;
				src+=srcstep;
			}
			// duplicate last pixel
			CopyTriple(dst,(src-srcstep));
			dst+=3;
			CopyTriple(dst,(src-srcstep));
			dst+=3;

			// advance write pointer for next row
			dst+=3*(total_w-tile_w);
		}

		m_AlphaMapCoords[i].u0=float(i*tile_w+2)/float(total_w);
		m_AlphaMapCoords[i].u1=float((i+1)*tile_w-2)/float(total_w);
		m_AlphaMapCoords[i].v0=0.0f;
		m_AlphaMapCoords[i].v1=1.0f;
	}

	for (uint i=0;i<NumAlphaMaps;i++)
		(void)ogl_tex_free(textures[i]);

	// upload the composite texture
	Tex t;
	(void)tex_wrap(total_w, total_h, 24, 0, data, &t);
	m_hCompositeAlphaMap = ogl_tex_wrap(&t, key);
	(void)ogl_tex_set_filter(m_hCompositeAlphaMap, GL_LINEAR);
	(void)ogl_tex_set_wrap  (m_hCompositeAlphaMap, GL_CLAMP_TO_EDGE);
	int ret = ogl_tex_upload(m_hCompositeAlphaMap, 0, 0, GL_INTENSITY);
	delete[] data;

	return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// UnloadAlphaMaps: frees the resources allocates by LoadAlphaMaps
void CRenderer::UnloadAlphaMaps()
{
	ogl_tex_free(m_hCompositeAlphaMap);
	m_hCompositeAlphaMap = 0;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// Scripting Interface

jsval CRenderer::JSI_GetFastPlayerColor(JSContext*)
{
	return ToJSVal(m_FastPlayerColor);
}

void CRenderer::JSI_SetFastPlayerColor(JSContext* ctx, jsval newval)
{
	bool fast;

	if (!ToPrimitive(ctx, newval, fast))
		return;

	SetFastPlayerColor(fast);
}

jsval CRenderer::JSI_GetRenderPath(JSContext*)
{
	return ToJSVal(GetRenderPathName(m_Options.m_RenderPath));
}

void CRenderer::JSI_SetRenderPath(JSContext* ctx, jsval newval)
{
	CStr name;

	if (!ToPrimitive(ctx, newval, name))
		return;

	SetRenderPath(GetRenderPathByName(name));
}

jsval CRenderer::JSI_GetUseDepthTexture(JSContext*)
{
	return ToJSVal(m->shadow->GetUseDepthTexture());
}

void CRenderer::JSI_SetUseDepthTexture(JSContext* ctx, jsval newval)
{
	bool depthTexture;

	if (!ToPrimitive(ctx, newval, depthTexture))
		return;

	m->shadow->SetUseDepthTexture(depthTexture);
}

jsval CRenderer::JSI_GetDepthTextureBits(JSContext*)
{
	return ToJSVal(m->shadow->GetDepthTextureBits());
}

void CRenderer::JSI_SetDepthTextureBits(JSContext* ctx, jsval newval)
{
	int depthTextureBits;

	if (!ToPrimitive(ctx, newval, depthTextureBits))
		return;

	m->shadow->SetDepthTextureBits(depthTextureBits);
}

jsval CRenderer::JSI_GetSky(JSContext*)
{
	return ToJSVal(m->skyManager.GetSkySet());
}

void CRenderer::JSI_SetSky(JSContext* ctx, jsval newval)
{
	CStrW skySet;
	if (!ToPrimitive<CStrW>(ctx, newval, skySet)) return;
	m->skyManager.SetSkySet(skySet);
}

void CRenderer::ScriptingInit()
{
	AddProperty(L"fastPlayerColor", &CRenderer::JSI_GetFastPlayerColor, &CRenderer::JSI_SetFastPlayerColor);
	AddProperty(L"renderpath", &CRenderer::JSI_GetRenderPath, &CRenderer::JSI_SetRenderPath);
	AddProperty(L"useDepthTexture", &CRenderer::JSI_GetUseDepthTexture, &CRenderer::JSI_SetUseDepthTexture);
	AddProperty(L"sortAllTransparent", &CRenderer::m_SortAllTransparent);
	AddProperty(L"displayFrustum", &CRenderer::m_DisplayFrustum);
	AddProperty(L"shadowZBias", &CRenderer::m_ShadowZBias);
	AddProperty(L"shadowMapSize", &CRenderer::m_ShadowMapSize);
	AddProperty(L"disableCopyShadow", &CRenderer::m_DisableCopyShadow);
	AddProperty(L"depthTextureBits", &CRenderer::JSI_GetDepthTextureBits, &CRenderer::JSI_SetDepthTextureBits);
	AddProperty(L"skipSubmit", &CRenderer::m_SkipSubmit);
	AddProperty(L"skySet", &CRenderer::JSI_GetSky, &CRenderer::JSI_SetSky);

	CJSObject<CRenderer>::ScriptingInit("Renderer");
}

