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

#include "precompiled.h"

#include "Renderer.h"

#include "graphics/Canvas2D.h"
#include "graphics/CinemaManager.h"
#include "graphics/GameView.h"
#include "graphics/LightEnv.h"
#include "graphics/ModelDef.h"
#include "graphics/TerrainTextureManager.h"
#include "i18n/L10n.h"
#include "lib/allocators/shared_ptr.h"
#include "lib/hash.h"
#include "lib/tex/tex.h"
#include "gui/GUIManager.h"
#include "ps/CConsole.h"
#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/CStrInternStatic.h"
#include "ps/Game.h"
#include "ps/GameSetup/Config.h"
#include "ps/GameSetup/GameSetup.h"
#include "ps/Globals.h"
#include "ps/Loader.h"
#include "ps/Profile.h"
#include "ps/Filesystem.h"
#include "ps/World.h"
#include "ps/ProfileViewer.h"
#include "graphics/Camera.h"
#include "graphics/FontManager.h"
#include "graphics/ShaderManager.h"
#include "graphics/Terrain.h"
#include "graphics/Texture.h"
#include "graphics/TextureManager.h"
#include "ps/Util.h"
#include "ps/VideoMode.h"
#include "renderer/backend/IDevice.h"
#include "renderer/DebugRenderer.h"
#include "renderer/PostprocManager.h"
#include "renderer/RenderingOptions.h"
#include "renderer/RenderModifiers.h"
#include "renderer/SceneRenderer.h"
#include "renderer/TimeManager.h"
#include "renderer/VertexBufferManager.h"
#include "tools/atlas/GameInterface/GameLoop.h"
#include "tools/atlas/GameInterface/View.h"

#include <algorithm>

namespace
{

size_t g_NextScreenShotNumber = 0;

///////////////////////////////////////////////////////////////////////////////////
// CRendererStatsTable - Profile display of rendering stats

/**
 * Class CRendererStatsTable: Implementation of AbstractProfileTable to
 * display the renderer stats in-game.
 *
 * Accesses CRenderer::m_Stats by keeping the reference passed to the
 * constructor.
 */
class CRendererStatsTable : public AbstractProfileTable
{
	NONCOPYABLE(CRendererStatsTable);
public:
	CRendererStatsTable(const CRenderer::Stats& st);

	// Implementation of AbstractProfileTable interface
	CStr GetName() override;
	CStr GetTitle() override;
	size_t GetNumberRows() override;
	const std::vector<ProfileColumn>& GetColumns() override;
	CStr GetCellText(size_t row, size_t col) override;
	AbstractProfileTable* GetChild(size_t row) override;

private:
	/// Reference to the renderer singleton's stats
	const CRenderer::Stats& Stats;

	/// Column descriptions
	std::vector<ProfileColumn> columnDescriptions;

	enum
	{
		Row_DrawCalls = 0,
		Row_TerrainTris,
		Row_WaterTris,
		Row_ModelTris,
		Row_OverlayTris,
		Row_BlendSplats,
		Row_Particles,
		Row_VBReserved,
		Row_VBAllocated,
		Row_TextureMemory,
		Row_ShadersLoaded,

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
	case Row_DrawCalls:
		if (col == 0)
			return "# draw calls";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)Stats.m_DrawCalls);
		return buf;

	case Row_TerrainTris:
		if (col == 0)
			return "# terrain tris";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)Stats.m_TerrainTris);
		return buf;

	case Row_WaterTris:
		if (col == 0)
			return "# water tris";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)Stats.m_WaterTris);
		return buf;

	case Row_ModelTris:
		if (col == 0)
			return "# model tris";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)Stats.m_ModelTris);
		return buf;

	case Row_OverlayTris:
		if (col == 0)
			return "# overlay tris";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)Stats.m_OverlayTris);
		return buf;

	case Row_BlendSplats:
		if (col == 0)
			return "# blend splats";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)Stats.m_BlendSplats);
		return buf;

	case Row_Particles:
		if (col == 0)
			return "# particles";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)Stats.m_Particles);
		return buf;

	case Row_VBReserved:
		if (col == 0)
			return "VB reserved";
		sprintf_s(buf, sizeof(buf), "%lu kB", static_cast<unsigned long>(g_Renderer.GetVertexBufferManager().GetBytesReserved() / 1024));
		return buf;

	case Row_VBAllocated:
		if (col == 0)
			return "VB allocated";
		sprintf_s(buf, sizeof(buf), "%lu kB", static_cast<unsigned long>(g_Renderer.GetVertexBufferManager().GetBytesAllocated() / 1024));
		return buf;

	case Row_TextureMemory:
		if (col == 0)
			return "textures uploaded";
		sprintf_s(buf, sizeof(buf), "%lu kB", (unsigned long)g_Renderer.GetTextureManager().GetBytesUploaded() / 1024);
		return buf;

	case Row_ShadersLoaded:
		if (col == 0)
			return "shader effects loaded";
		sprintf_s(buf, sizeof(buf), "%lu", (unsigned long)g_Renderer.GetShaderManager().GetNumEffectsLoaded());
		return buf;

	default:
		return "???";
	}
}

AbstractProfileTable* CRendererStatsTable::GetChild(size_t UNUSED(row))
{
	return 0;
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////////
// CRenderer implementation

/**
 * Struct CRendererInternals: Truly hide data that is supposed to be hidden
 * in this structure so it won't even appear in header files.
 */
class CRenderer::Internals
{
	NONCOPYABLE(Internals);
public:
	Renderer::Backend::IDevice* device;

	std::unique_ptr<Renderer::Backend::IDeviceCommandContext> deviceCommandContext;

	/// true if CRenderer::Open has been called
	bool IsOpen;

	/// true if shaders need to be reloaded
	bool ShadersDirty;

	/// Table to display renderer stats in-game via profile system
	CRendererStatsTable profileTable;

	/// Shader manager
	CShaderManager shaderManager;

	/// Texture manager
	CTextureManager textureManager;

	CVertexBufferManager vertexBufferManager;

	/// Time manager
	CTimeManager timeManager;

	/// Postprocessing effect manager
	CPostprocManager postprocManager;

	CSceneRenderer sceneRenderer;

	CDebugRenderer debugRenderer;

	CFontManager fontManager;

	struct VertexAttributesHash
	{
		size_t operator()(const std::vector<Renderer::Backend::SVertexAttributeFormat>& attributes) const;
	};

	std::unordered_map<
		std::vector<Renderer::Backend::SVertexAttributeFormat>,
		std::unique_ptr<Renderer::Backend::IVertexInputLayout>, VertexAttributesHash> vertexInputLayouts;

	Internals(Renderer::Backend::IDevice* device) :
		device(device),
		deviceCommandContext(device->CreateCommandContext()),
		IsOpen(false), ShadersDirty(true), profileTable(g_Renderer.m_Stats),
		shaderManager(device), textureManager(g_VFS, false, device), vertexBufferManager(device),
		postprocManager(device), sceneRenderer(device)
	{
	}
};

size_t CRenderer::Internals::VertexAttributesHash::operator()(
	const std::vector<Renderer::Backend::SVertexAttributeFormat>& attributes) const
{
	size_t seed = 0;
	hash_combine(seed, attributes.size());
	for (const Renderer::Backend::SVertexAttributeFormat& attribute : attributes)
	{
		hash_combine(seed, attribute.stream);
		hash_combine(seed, attribute.format);
		hash_combine(seed, attribute.offset);
		hash_combine(seed, attribute.stride);
		hash_combine(seed, attribute.rate);
		hash_combine(seed, attribute.bindingSlot);
	}
	return seed;
}

CRenderer::CRenderer(Renderer::Backend::IDevice* device)
{
	TIMER(L"InitRenderer");

	m = std::make_unique<Internals>(device);

	g_ProfileViewer.AddRootTable(&m->profileTable);

	m_Width = 0;
	m_Height = 0;

	m_Stats.Reset();

	// Create terrain related stuff.
	new CTerrainTextureManager(device);

	Open(g_xres, g_yres);

	// Setup lighting environment. Since the Renderer accesses the
	// lighting environment through a pointer, this has to be done before
	// the first Frame.
	GetSceneRenderer().SetLightEnv(&g_LightEnv);

	ModelDefActivateFastImpl();
	ColorActivateFastImpl();
	ModelRenderer::Init();
}

CRenderer::~CRenderer()
{
	delete &g_TexMan;

	// We no longer UnloadWaterTextures here -
	// that is the responsibility of the module that asked for
	// them to be loaded (i.e. CGameView).
	m.reset();
}

void CRenderer::ReloadShaders()
{
	ENSURE(m->IsOpen);

	m->sceneRenderer.ReloadShaders(m->device);
	m->ShadersDirty = false;
}

bool CRenderer::Open(int width, int height)
{
	m->IsOpen = true;

	// Dimensions
	m_Width = width;
	m_Height = height;

	// Validate the currently selected render path
	SetRenderPath(g_RenderingOptions.GetRenderPath());

	m->debugRenderer.Initialize();

	if (m->postprocManager.IsEnabled())
		m->postprocManager.Initialize();

	m->sceneRenderer.Initialize();

	return true;
}

void CRenderer::Resize(int width, int height)
{
	m_Width = width;
	m_Height = height;

	m->postprocManager.Resize();

	m->sceneRenderer.Resize(width, height);
}

void CRenderer::SetRenderPath(RenderPath rp)
{
	if (!m->IsOpen)
	{
		// Delay until Open() is called.
		return;
	}

	// Renderer has been opened, so validate the selected renderpath
	const bool hasShadersSupport =
		m->device->GetCapabilities().ARBShaders ||
		m->device->GetBackend() != Renderer::Backend::Backend::GL_ARB;
	if (rp == RenderPath::DEFAULT)
	{
		if (hasShadersSupport)
			rp = RenderPath::SHADER;
		else
			rp = RenderPath::FIXED;
	}

	if (rp == RenderPath::SHADER)
	{
		if (!hasShadersSupport)
		{
			LOGWARNING("Falling back to fixed function\n");
			rp = RenderPath::FIXED;
		}
	}

	// TODO: remove this once capabilities have been properly extracted and the above checks have been moved elsewhere.
	g_RenderingOptions.m_RenderPath = rp;

	MakeShadersDirty();
}

bool CRenderer::ShouldRender() const
{
	return !g_app_minimized && (g_app_has_focus || !g_VideoMode.IsInFullscreen());
}

void CRenderer::RenderFrame(const bool needsPresent)
{
	// Do not render if not focused while in fullscreen or minimised,
	// as that triggers a difficult-to-reproduce crash on some graphic cards.
	if (!ShouldRender())
		return;

	if (m_ScreenShotType == ScreenShotType::BIG)
	{
		RenderBigScreenShot(needsPresent);
	}
	else if (m_ScreenShotType == ScreenShotType::DEFAULT)
	{
		RenderScreenShot(needsPresent);
	}
	else
	{
		if (needsPresent)
		{
			// In case of no acquired backbuffer we have nothing render to.
			if (!m->device->AcquireNextBackbuffer())
				return;
		}

		if (m_ShouldPreloadResourcesBeforeNextFrame)
		{
			m_ShouldPreloadResourcesBeforeNextFrame = false;
			// We don't need to render logger for the preload.
			RenderFrameImpl(true, false);
		}

		RenderFrameImpl(true, true);

		m->deviceCommandContext->Flush();
		if (needsPresent)
			m->device->Present();
	}
}

void CRenderer::RenderFrameImpl(const bool renderGUI, const bool renderLogger)
{
	PROFILE3("render");

	g_Profiler2.RecordGPUFrameStart(m->deviceCommandContext.get());

	g_TexMan.UploadResourcesIfNeeded(m->deviceCommandContext.get());

	m->textureManager.MakeUploadProgress(m->deviceCommandContext.get());

	// prepare before starting the renderer frame
	if (g_Game && g_Game->IsGameStarted())
		g_Game->GetView()->BeginFrame();

	if (g_Game)
		m->sceneRenderer.SetSimulation(g_Game->GetSimulation2());

	// start new frame
	BeginFrame();

	if (g_Game && g_Game->IsGameStarted())
	{
		g_Game->GetView()->Prepare(m->deviceCommandContext.get());

		Renderer::Backend::IFramebuffer* framebuffer = nullptr;
		Renderer::Backend::IDeviceCommandContext::Rect viewportRect{};

		CPostprocManager& postprocManager = GetPostprocManager();
		if (postprocManager.IsEnabled())
		{
			// We have to update the post process manager with real near/far planes
			// that we use for the scene rendering.
			postprocManager.SetDepthBufferClipPlanes(
				m->sceneRenderer.GetViewCamera().GetNearPlane(),
				m->sceneRenderer.GetViewCamera().GetFarPlane()
			);
			postprocManager.Initialize();
			framebuffer = postprocManager.PrepareAndGetOutputFramebuffer();
			viewportRect.width = framebuffer->GetWidth();
			viewportRect.height = framebuffer->GetHeight();
		}
		else
		{
			// We don't need to clear the color attachment of the framebuffer as the sky
			// is going to be rendered anyway.
			framebuffer =
				m->deviceCommandContext->GetDevice()->GetCurrentBackbuffer(
					Renderer::Backend::AttachmentLoadOp::DONT_CARE,
					Renderer::Backend::AttachmentStoreOp::STORE,
					Renderer::Backend::AttachmentLoadOp::CLEAR,
					Renderer::Backend::AttachmentStoreOp::DONT_CARE);

			viewportRect.width = m_Width;
			viewportRect.height = m_Height;
		}

		m->deviceCommandContext->BeginFramebufferPass(framebuffer);
		m->deviceCommandContext->SetViewports(1, &viewportRect);

		g_Game->GetView()->Render(m->deviceCommandContext.get());

		if (postprocManager.IsEnabled())
		{
			m->deviceCommandContext->EndFramebufferPass();

			if (postprocManager.IsMultisampleEnabled())
				postprocManager.ResolveMultisampleFramebuffer(m->deviceCommandContext.get());

			postprocManager.ApplyPostproc(m->deviceCommandContext.get());

			Renderer::Backend::IFramebuffer* backbuffer =
				m->deviceCommandContext->GetDevice()->GetCurrentBackbuffer(
					Renderer::Backend::AttachmentLoadOp::LOAD,
					Renderer::Backend::AttachmentStoreOp::STORE,
					Renderer::Backend::AttachmentLoadOp::LOAD,
					Renderer::Backend::AttachmentStoreOp::DONT_CARE);
			postprocManager.BlitOutputFramebuffer(
				m->deviceCommandContext.get(), backbuffer);

			m->deviceCommandContext->BeginFramebufferPass(backbuffer);

			Renderer::Backend::IDeviceCommandContext::Rect viewportRect{};
			viewportRect.width = m_Width;
			viewportRect.height = m_Height;
			m->deviceCommandContext->SetViewports(1, &viewportRect);
		}

		g_Game->GetView()->RenderOverlays(m->deviceCommandContext.get());

		g_Game->GetView()->GetCinema()->Render();
	}
	else
	{
		// We have a fullscreen background in our UI so we don't need
		// to clear the color attachment.
		// We don't need a depth test to render so we don't care about the
		// depth-stencil attachment content.
		// In case of Atlas we don't have g_Game, so we still need to clear depth.
		const Renderer::Backend::AttachmentLoadOp depthStencilLoadOp =
			g_AtlasGameLoop && g_AtlasGameLoop->view
				? Renderer::Backend::AttachmentLoadOp::CLEAR
				: Renderer::Backend::AttachmentLoadOp::DONT_CARE;
		Renderer::Backend::IFramebuffer* backbuffer =
			m->deviceCommandContext->GetDevice()->GetCurrentBackbuffer(
				Renderer::Backend::AttachmentLoadOp::DONT_CARE,
				Renderer::Backend::AttachmentStoreOp::STORE,
				depthStencilLoadOp,
				Renderer::Backend::AttachmentStoreOp::DONT_CARE);
		m->deviceCommandContext->BeginFramebufferPass(backbuffer);

		Renderer::Backend::IDeviceCommandContext::Rect viewportRect{};
		viewportRect.width = m_Width;
		viewportRect.height = m_Height;
		m->deviceCommandContext->SetViewports(1, &viewportRect);
	}

	// If we're in Atlas game view, render special tools
	if (g_AtlasGameLoop && g_AtlasGameLoop->view)
	{
		g_AtlasGameLoop->view->DrawCinemaPathTool();
	}

	RenderFrame2D(renderGUI, renderLogger);

	m->deviceCommandContext->EndFramebufferPass();

	EndFrame();

	const Stats& stats = GetStats();
	PROFILE2_ATTR("draw calls: %zu", stats.m_DrawCalls);
	PROFILE2_ATTR("terrain tris: %zu", stats.m_TerrainTris);
	PROFILE2_ATTR("water tris: %zu", stats.m_WaterTris);
	PROFILE2_ATTR("model tris: %zu", stats.m_ModelTris);
	PROFILE2_ATTR("overlay tris: %zu", stats.m_OverlayTris);
	PROFILE2_ATTR("blend splats: %zu", stats.m_BlendSplats);
	PROFILE2_ATTR("particles: %zu", stats.m_Particles);

	g_Profiler2.RecordGPUFrameEnd(m->deviceCommandContext.get());
}

void CRenderer::RenderFrame2D(const bool renderGUI, const bool renderLogger)
{
	CCanvas2D canvas(g_xres, g_yres, g_VideoMode.GetScale(), m->deviceCommandContext.get());

	m->sceneRenderer.RenderTextOverlays(canvas);

	if (renderGUI)
	{
		GPU_SCOPED_LABEL(m->deviceCommandContext.get(), "Render GUI");
		// All GUI elements are drawn in Z order to render semi-transparent
		// objects correctly.
		g_GUI->Draw(canvas);
	}

	// If we're in Atlas game view, render special overlays (e.g. editor bandbox).
	if (g_AtlasGameLoop && g_AtlasGameLoop->view)
	{
		g_AtlasGameLoop->view->DrawOverlays(canvas);
	}

	{
		GPU_SCOPED_LABEL(m->deviceCommandContext.get(), "Render console");
		g_Console->Render(canvas);
	}

	if (renderLogger)
	{
		GPU_SCOPED_LABEL(m->deviceCommandContext.get(), "Render logger");
		g_Logger->Render(canvas);
	}

	{
		GPU_SCOPED_LABEL(m->deviceCommandContext.get(), "Render profiler");
		// Profile information
		g_ProfileViewer.RenderProfile(canvas);
	}

	this->GetFontManager().UploadTexturesAtlasToGPU();
}

void CRenderer::RenderScreenShot(const bool needsPresent)
{
	m_ScreenShotType = ScreenShotType::NONE;

	// get next available numbered filename
	// note: %04d -> always 4 digits, so sorting by filename works correctly.
	const VfsPath filenameFormat(L"screenshots/screenshot%04d.png");
	VfsPath filename;
	vfs::NextNumberedFilename(g_VFS, filenameFormat, g_NextScreenShotNumber, filename);

	const size_t width = static_cast<size_t>(g_xres), height = static_cast<size_t>(g_yres);
	const size_t bpp = 24;

	if (needsPresent && !m->device->AcquireNextBackbuffer())
		return;

	// Hide log messages and re-render
	RenderFrameImpl(true, false);

	const size_t img_size = width * height * bpp / 8;
	const size_t hdr_size = tex_hdr_size(filename);
	std::shared_ptr<u8> buf;
	AllocateAligned(buf, hdr_size + img_size, maxSectorSize);
	void* img = buf.get() + hdr_size;
	Tex t;
	if (t.wrap(width, height, bpp, TEX_BOTTOM_UP, buf, hdr_size) < 0)
		return;

	m->deviceCommandContext->ReadbackFramebufferSync(0, 0, width, height, img);
	m->deviceCommandContext->Flush();
	if (needsPresent)
		m->device->Present();

	if (tex_write(&t, filename) == INFO::OK)
	{
		OsPath realPath;
		g_VFS->GetRealPath(filename, realPath);

		LOGMESSAGERENDER("Screenshot written to '%s'", realPath.string8());

		debug_printf(
			CStr(g_L10n.Translate("Screenshot written to '%s'") + "\n").c_str(),
			realPath.string8().c_str());
	}
	else
		LOGERROR("Error writing screenshot to '%s'", filename.string8());
}

void CRenderer::RenderBigScreenShot(const bool needsPresent)
{
	m_ScreenShotType = ScreenShotType::NONE;

	// If the game hasn't started yet then use WriteScreenshot to generate the image.
	if (!g_Game)
		return RenderScreenShot(needsPresent);

	const int tiles{g_ConfigDB.Get("screenshot.tiles", 4)};
	const int tileWidth{g_ConfigDB.Get("screenshot.tilewidth", 256)};
	const int tileHeight{g_ConfigDB.Get("screenshot.tileheight", 256)};
	if (tiles <= 0 || tileWidth <= 0 || tileHeight <= 0 || tileWidth * tiles % 4 != 0 || tileHeight * tiles % 4 != 0)
	{
		LOGWARNING("Invalid big screenshot size: tiles=%d tileWidth=%d tileHeight=%d", tiles, tileWidth, tileHeight);
		return;
	}

	if (g_xres < tileWidth && g_yres < tileHeight)
	{
		LOGWARNING(
			"The window size is too small for a big screenshot, increase the"
			" window size %dx%d or decrease the tile size %dx%d",
			g_xres, g_yres, tileWidth, tileHeight);
		return;
	}

	// get next available numbered filename
	// note: %04d -> always 4 digits, so sorting by filename works correctly.
	const VfsPath filenameFormat(L"screenshots/screenshot%04d.bmp");
	VfsPath filename;
	vfs::NextNumberedFilename(g_VFS, filenameFormat, g_NextScreenShotNumber, filename);

	const int imageWidth = tileWidth * tiles, imageHeight = tileHeight * tiles;
	const int bpp = 24;

	const size_t imageSize = imageWidth * imageHeight * bpp / 8;
	const size_t tileSize = tileWidth * tileHeight * bpp / 8;
	const size_t headerSize = tex_hdr_size(filename);
	void* tileData = malloc(tileSize);
	if (!tileData)
	{
		WARN_IF_ERR(ERR::NO_MEM);
		return;
	}
	std::shared_ptr<u8> imageBuffer;
	AllocateAligned(imageBuffer, headerSize + imageSize, maxSectorSize);

	Tex t;
	void* img = imageBuffer.get() + headerSize;
	if (t.wrap(imageWidth, imageHeight, bpp, TEX_BOTTOM_UP, imageBuffer, headerSize) < 0)
	{
		free(tileData);
		return;
	}

	CCamera oldCamera = *g_Game->GetView()->GetCamera();

	// Resize various things so that the sizes and aspect ratios are correct
	{
		g_Renderer.Resize(tileWidth, tileHeight);
		SViewPort vp = { 0, 0, tileWidth, tileHeight };
		g_Game->GetView()->SetViewport(vp);
	}

	// Render each tile
	CMatrix3D projection;
	projection.SetIdentity();
	const float aspectRatio = 1.0f * tileWidth / tileHeight;
	for (int tileY = 0; tileY < tiles; ++tileY)
	{
		for (int tileX = 0; tileX < tiles; ++tileX)
		{
			// Adjust the camera to render the appropriate region
			if (oldCamera.GetProjectionType() == CCamera::ProjectionType::PERSPECTIVE)
			{
				projection.SetPerspectiveTile(
					oldCamera.GetFOV(), aspectRatio, oldCamera.GetNearPlane(), oldCamera.GetFarPlane(),
					tiles, tileX, tileY);
			}
			g_Game->GetView()->GetCamera()->SetProjection(projection);

			if (!needsPresent || m->device->AcquireNextBackbuffer())
			{
				RenderFrameImpl(false, false);

				m->deviceCommandContext->ReadbackFramebufferSync(0, 0, tileWidth, tileHeight, tileData);
				m->deviceCommandContext->Flush();

				if (needsPresent)
					m->device->Present();
			}

			// Copy the tile pixels into the main image
			for (int y = 0; y < tileHeight; ++y)
			{
				void* dest = static_cast<char*>(img) + ((tileY * tileHeight + y) * imageWidth + (tileX * tileWidth)) * bpp / 8;
				void* src = static_cast<char*>(tileData) + y * tileWidth * bpp / 8;
				memcpy(dest, src, tileWidth * bpp / 8);
			}
		}
	}

	// Restore the viewport settings
	{
		g_Renderer.Resize(g_xres, g_yres);
		SViewPort vp = { 0, 0, g_xres, g_yres };
		g_Game->GetView()->SetViewport(vp);
		g_Game->GetView()->GetCamera()->SetProjectionFromCamera(oldCamera);
	}

	if (tex_write(&t, filename) == INFO::OK)
	{
		OsPath realPath;
		g_VFS->GetRealPath(filename, realPath);

		LOGMESSAGERENDER("Screenshot written to '%s'", realPath.string8());

		debug_printf(
			CStr(g_L10n.Translate("Screenshot written to '%s'") + "\n").c_str(),
			realPath.string8().c_str());
	}
	else
		LOGERROR("Error writing screenshot to '%s'", filename.string8());

	free(tileData);
}

void CRenderer::BeginFrame()
{
	PROFILE("begin frame");

	// Zero out all the per-frame stats.
	m_Stats.Reset();

	if (m->ShadersDirty)
		ReloadShaders();

	m->sceneRenderer.BeginFrame();
}

void CRenderer::EndFrame()
{
	PROFILE3("end frame");

	m->sceneRenderer.EndFrame();
}

void CRenderer::MakeShadersDirty()
{
	m->ShadersDirty = true;
	m->sceneRenderer.MakeShadersDirty();
}

CTextureManager& CRenderer::GetTextureManager()
{
	return m->textureManager;
}

CVertexBufferManager& CRenderer::GetVertexBufferManager()
{
	return m->vertexBufferManager;
}

CShaderManager& CRenderer::GetShaderManager()
{
	return m->shaderManager;
}

CTimeManager& CRenderer::GetTimeManager()
{
	return m->timeManager;
}

CPostprocManager& CRenderer::GetPostprocManager()
{
	return m->postprocManager;
}

CSceneRenderer& CRenderer::GetSceneRenderer()
{
	return m->sceneRenderer;
}

CDebugRenderer& CRenderer::GetDebugRenderer()
{
	return m->debugRenderer;
}

CFontManager& CRenderer::GetFontManager()
{
	return m->fontManager;
}

void CRenderer::PreloadResourcesBeforeNextFrame()
{
	m_ShouldPreloadResourcesBeforeNextFrame = true;
}

void CRenderer::MakeScreenShotOnNextFrame(ScreenShotType screenShotType)
{
	m_ScreenShotType = screenShotType;
}

Renderer::Backend::IDeviceCommandContext* CRenderer::GetDeviceCommandContext()
{
	return m->deviceCommandContext.get();
}

Renderer::Backend::IVertexInputLayout* CRenderer::GetVertexInputLayout(
	const PS::span<const Renderer::Backend::SVertexAttributeFormat> attributes)
{
	const auto [it, inserted] = m->vertexInputLayouts.emplace(
		std::vector<Renderer::Backend::SVertexAttributeFormat>{attributes.begin(), attributes.end()}, nullptr);
	if (inserted)
		it->second = m->device->CreateVertexInputLayout(attributes);
	return it->second.get();
}
