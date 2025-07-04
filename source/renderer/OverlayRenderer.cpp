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

#include "OverlayRenderer.h"

#include "graphics/Camera.h"
#include "graphics/LOSTexture.h"
#include "graphics/Overlay.h"
#include "graphics/ShaderManager.h"
#include "graphics/Terrain.h"
#include "graphics/TextureManager.h"
#include "lib/hash.h"
#include "maths/MathUtil.h"
#include "maths/Quaternion.h"
#include "ps/CStrInternStatic.h"
#include "ps/Game.h"
#include "ps/Profile.h"
#include "renderer/backend/PipelineState.h"
#include "renderer/DebugRenderer.h"
#include "renderer/Renderer.h"
#include "renderer/SceneRenderer.h"
#include "renderer/TexturedLineRData.h"
#include "renderer/VertexArray.h"
#include "renderer/VertexBuffer.h"
#include "renderer/VertexBufferManager.h"
#include "simulation2/components/ICmpWaterManager.h"
#include "simulation2/Simulation2.h"
#include "simulation2/system/SimContext.h"

#include <unordered_map>

namespace
{

struct Shader
{
	CShaderTechniquePtr technique, techniqueWireframe;

	const CShaderTechniquePtr& GetTechnique() const
	{
		return g_Renderer.GetSceneRenderer().GetOverlayRenderMode() == WIREFRAME
			? techniqueWireframe : technique;
	}
};

void AdjustOverlayGraphicsPipelineState(
	Renderer::Backend::SGraphicsPipelineStateDesc& pipelineStateDesc,
	const bool depthTestEnabled)
{
	pipelineStateDesc.depthStencilState.depthTestEnabled = depthTestEnabled;
	pipelineStateDesc.depthStencilState.depthWriteEnabled = false;
	pipelineStateDesc.blendState.enabled = true;
	pipelineStateDesc.blendState.srcColorBlendFactor = pipelineStateDesc.blendState.srcAlphaBlendFactor =
		Renderer::Backend::BlendFactor::SRC_ALPHA;
	pipelineStateDesc.blendState.dstColorBlendFactor = pipelineStateDesc.blendState.dstAlphaBlendFactor =
		Renderer::Backend::BlendFactor::ONE_MINUS_SRC_ALPHA;
	pipelineStateDesc.blendState.colorBlendOp = pipelineStateDesc.blendState.alphaBlendOp =
		Renderer::Backend::BlendOp::ADD;
}

Shader CreateShader(
	const CStrIntern name, const CShaderDefines& defines,
	const bool depthTestEnabled)
{
	Shader shader;

	shader.technique = g_Renderer.GetShaderManager().LoadEffect(
		name, defines,
		[depthTestEnabled](Renderer::Backend::SGraphicsPipelineStateDesc& pipelineStateDesc)
		{
			AdjustOverlayGraphicsPipelineState(pipelineStateDesc, depthTestEnabled);
		});
	shader.techniqueWireframe = g_Renderer.GetShaderManager().LoadEffect(
		name, defines,
		[depthTestEnabled](Renderer::Backend::SGraphicsPipelineStateDesc& pipelineStateDesc)
		{
			AdjustOverlayGraphicsPipelineState(pipelineStateDesc, depthTestEnabled);
			pipelineStateDesc.rasterizationState.polygonMode = Renderer::Backend::PolygonMode::LINE;
		});

	return shader;
}

/**
 * Key used to group quads into batches for more efficient rendering. Currently groups by the combination
 * of the main texture and the texture mask, to minimize texture swapping during rendering.
 */
struct QuadBatchKey
{
	QuadBatchKey (const CTexturePtr& texture, const CTexturePtr& textureMask)
		: m_Texture(texture), m_TextureMask(textureMask)
	{ }

	bool operator==(const QuadBatchKey& other) const
	{
		return (m_Texture == other.m_Texture && m_TextureMask == other.m_TextureMask);
	}

	CTexturePtr m_Texture;
	CTexturePtr m_TextureMask;
};

struct QuadBatchHash
{
	std::size_t operator()(const QuadBatchKey& d) const
	{
		size_t seed = 0;
		hash_combine(seed, d.m_Texture);
		hash_combine(seed, d.m_TextureMask);
		return seed;
	}
};

/**
 * Holds information about a single quad rendering batch.
 */
class QuadBatchData : public CRenderData
{
public:
	QuadBatchData() : m_IndicesBase(0), m_NumRenderQuads(0) { }

	/// Holds the quad overlay structures requested to be rendered in this batch. Must be cleared
	/// after each frame.
	std::vector<SOverlayQuad*> m_Quads;

	/// Start index of this batch into the dedicated quad indices VertexArray (see OverlayInternals).
	size_t m_IndicesBase;
	/// Amount of quads to actually render in this batch. Potentially (although unlikely to be)
	/// different from m_Quads.size() due to restrictions on the total amount of quads that can be
	/// rendered. Must be reset after each frame.
	size_t m_NumRenderQuads;
};

} // anonymous namespace

struct OverlayRendererInternals
{
	using QuadBatchMap = std::unordered_map<QuadBatchKey, QuadBatchData, QuadBatchHash>;

	OverlayRendererInternals();
	~OverlayRendererInternals() = default;

	Renderer::Backend::IDevice* device = nullptr;

	std::vector<SOverlayLine*> lines;
	std::vector<SOverlayTexturedLine*> texlines;
	std::vector<SOverlaySprite*> sprites;
	std::vector<SOverlayQuad*> quads;
	std::vector<SOverlaySphere*> spheres;

	QuadBatchMap quadBatchMap;

	// Dedicated vertex/index buffers for rendering all quads (to within the limits set by
	// MAX_QUAD_OVERLAYS).
	VertexArray quadVertices;
	VertexArray::Attribute quadAttributePos;
	VertexArray::Attribute quadAttributeColor;
	VertexArray::Attribute quadAttributeUV;
	VertexIndexArray quadIndices;

	// Maximum amount of quad overlays we support for rendering. This limit is set to be able to
	// render all quads from a single dedicated VB without having to reallocate it, which is much
	// faster in the typical case of rendering only a handful of quads. When modifying this value,
	// you must take care for the new amount of quads to fit in a single backend buffer (which is
	// not likely to be a problem).
	static const size_t MAX_QUAD_OVERLAYS = 1024;

	// Sets of commonly-(re)used shader defines.
	CShaderDefines defsOverlayLineNormal;
	CShaderDefines defsOverlayLineAlwaysVisible;
	CShaderDefines defsQuadOverlay;

	Shader shaderTexLineNormal;
	Shader shaderTexLineAlwaysVisible;
	Shader shaderQuadOverlay;
	Shader shaderForegroundOverlay;
	Shader shaderOverlaySolid;

	Renderer::Backend::IVertexInputLayout* quadVertexInputLayout = nullptr;
	Renderer::Backend::IVertexInputLayout* foregroundVertexInputLayout = nullptr;
	Renderer::Backend::IVertexInputLayout* sphereVertexInputLayout = nullptr;

	Renderer::Backend::IVertexInputLayout* texturedLineVertexInputLayout = nullptr;

	// Geometry for a unit sphere
	std::vector<float> sphereVertexes;
	std::vector<u16> sphereIndexes;
	void GenerateSphere();

	// Performs one-time setup. Called from CRenderer::Open, after graphics capabilities have
	// been detected. Note that no backend buffer must be created before this is called, since
	// the shader path and graphics capabilities are not guaranteed to be stable before this
	// point.
	void Initialize();
};

const float OverlayRenderer::OVERLAY_VOFFSET = 0.2f;

OverlayRendererInternals::OverlayRendererInternals()
	: quadVertices(Renderer::Backend::IBuffer::Type::VERTEX,
		Renderer::Backend::IBuffer::Usage::DYNAMIC | Renderer::Backend::IBuffer::Usage::TRANSFER_DST),
	quadIndices(Renderer::Backend::IBuffer::Usage::TRANSFER_DST)
{
	quadAttributePos.format = Renderer::Backend::Format::R32G32B32_SFLOAT;
	quadVertices.AddAttribute(&quadAttributePos);

	quadAttributeColor.format = Renderer::Backend::Format::R8G8B8A8_UNORM;
	quadVertices.AddAttribute(&quadAttributeColor);

	quadAttributeUV.format = Renderer::Backend::Format::R32G32_SFLOAT;
	quadVertices.AddAttribute(&quadAttributeUV);

	// Note that we're reusing the textured overlay line shader for the quad overlay rendering. This
	// is because their code is almost identical; the only difference is that for the quad overlays
	// we want to use a vertex color stream as opposed to an objectColor uniform. To this end, the
	// shader has been set up to switch between the two behaviours based on the USE_OBJECTCOLOR define.
	defsOverlayLineNormal.Add(str_USE_OBJECTCOLOR, str_1);
	defsOverlayLineAlwaysVisible.Add(str_USE_OBJECTCOLOR, str_1);
	defsOverlayLineAlwaysVisible.Add(str_IGNORE_LOS, str_1);
}

void OverlayRendererInternals::Initialize()
{
	// Perform any initialization after graphics capabilities have been detected. Notably,
	// only at this point can we safely allocate backend buffer (in contrast to e.g. in the constructor),
	// because their creation depends on the shader path, which is not reliably set before this point.

	quadVertices.SetNumberOfVertices(MAX_QUAD_OVERLAYS * 4);
	quadVertices.Layout(); // allocate backing store

	quadIndices.SetNumberOfVertices(MAX_QUAD_OVERLAYS * 6);
	quadIndices.Layout(); // allocate backing store

	// Since the quads in the vertex array are independent and always consist of exactly 4 vertices per quad, the
	// indices are always the same; we can therefore fill in all the indices once and pretty much forget about
	// them. We then also no longer need its backing store, since we never change any indices afterwards.
	VertexArrayIterator<u16> index = quadIndices.GetIterator();
	for (u16 i = 0; i < static_cast<u16>(MAX_QUAD_OVERLAYS); ++i)
	{
		*index++ = i * 4 + 0;
		*index++ = i * 4 + 1;
		*index++ = i * 4 + 2;
		*index++ = i * 4 + 2;
		*index++ = i * 4 + 3;
		*index++ = i * 4 + 0;
	}
	quadIndices.Upload();
	quadIndices.FreeBackingStore();

	shaderTexLineNormal =
		CreateShader(str_overlay_line, defsOverlayLineNormal, true);
	shaderTexLineAlwaysVisible =
		CreateShader(str_overlay_line, defsOverlayLineAlwaysVisible, true);
	shaderQuadOverlay =
		CreateShader(str_overlay_line, defsQuadOverlay, true);
	shaderForegroundOverlay =
		CreateShader(str_foreground_overlay, {}, false);
	shaderOverlaySolid =
		CreateShader(str_overlay_solid, {}, true);

	const uint32_t quadStride = quadVertices.GetStride();
	const std::array<Renderer::Backend::SVertexAttributeFormat, 3> quadAttributes{{
		{Renderer::Backend::VertexAttributeStream::POSITION,
			quadAttributePos.format, quadAttributePos.offset, quadStride,
			Renderer::Backend::VertexAttributeRate::PER_VERTEX, 0},
		{Renderer::Backend::VertexAttributeStream::COLOR,
			quadAttributeColor.format, quadAttributeColor.offset, quadStride,
			Renderer::Backend::VertexAttributeRate::PER_VERTEX, 0},
		{Renderer::Backend::VertexAttributeStream::UV0,
			quadAttributeUV.format, quadAttributeUV.offset, quadStride,
			Renderer::Backend::VertexAttributeRate::PER_VERTEX, 0}
	}};
	quadVertexInputLayout = g_Renderer.GetVertexInputLayout(quadAttributes);

	const std::array<Renderer::Backend::SVertexAttributeFormat, 2> foregroundAttributes{{
		{Renderer::Backend::VertexAttributeStream::POSITION,
			Renderer::Backend::Format::R32G32B32_SFLOAT, 0, sizeof(float) * 3,
			Renderer::Backend::VertexAttributeRate::PER_VERTEX, 0},
		{Renderer::Backend::VertexAttributeStream::UV0,
			Renderer::Backend::Format::R32G32_SFLOAT, 0, sizeof(float) * 2,
			Renderer::Backend::VertexAttributeRate::PER_VERTEX, 1}
	}};
	foregroundVertexInputLayout = g_Renderer.GetVertexInputLayout(foregroundAttributes);

	const std::array<Renderer::Backend::SVertexAttributeFormat, 1> shpereAttributes{{
		{Renderer::Backend::VertexAttributeStream::POSITION,
			Renderer::Backend::Format::R32G32B32_SFLOAT, 0, sizeof(float) * 3,
			Renderer::Backend::VertexAttributeRate::PER_VERTEX, 0}
	}};
	sphereVertexInputLayout = g_Renderer.GetVertexInputLayout(shpereAttributes);

	texturedLineVertexInputLayout = CTexturedLineRData::GetVertexInputLayout();
}

OverlayRenderer::OverlayRenderer()
{
	m = new OverlayRendererInternals();
}

OverlayRenderer::~OverlayRenderer()
{
	delete m;
}

void OverlayRenderer::Initialize()
{
	m->Initialize();
}

void OverlayRenderer::Submit(SOverlayLine* line)
{
	m->lines.push_back(line);
}

void OverlayRenderer::Submit(SOverlayTexturedLine* line)
{
	// Simplify the rest of the code by guaranteeing non-empty lines
	if (line->m_Coords.empty())
		return;

	m->texlines.push_back(line);
}

void OverlayRenderer::Submit(SOverlaySprite* overlay)
{
	m->sprites.push_back(overlay);
}

void OverlayRenderer::Submit(SOverlayQuad* overlay)
{
	m->quads.push_back(overlay);
}

void OverlayRenderer::Submit(SOverlaySphere* overlay)
{
	m->spheres.push_back(overlay);
}

void OverlayRenderer::EndFrame()
{
	m->lines.clear();
	m->texlines.clear();
	m->sprites.clear();
	m->quads.clear();
	m->spheres.clear();

	// this should leave the capacity unchanged, which is okay since it
	// won't be very large or very variable

	// Empty the batch rendering data structures, but keep their key mappings around for the next frames
	for (OverlayRendererInternals::QuadBatchMap::iterator it = m->quadBatchMap.begin(); it != m->quadBatchMap.end(); ++it)
	{
		QuadBatchData& quadBatchData = (it->second);
		quadBatchData.m_Quads.clear();
		quadBatchData.m_NumRenderQuads = 0;
		quadBatchData.m_IndicesBase = 0;
	}
}

void OverlayRenderer::PrepareForRendering()
{
	PROFILE3("prepare overlays");

	// This is where we should do something like sort the overlays by
	// color/sprite/etc for more efficient rendering

	for (size_t i = 0; i < m->texlines.size(); ++i)
	{
		SOverlayTexturedLine* line = m->texlines[i];
		if (!line->m_RenderData)
		{
			line->m_RenderData = std::make_shared<CTexturedLineRData>();
			line->m_RenderData->Update(*line);
			// We assume the overlay line will get replaced by the caller
			// if terrain changes, so we don't need to detect that here and
			// call Update again. Also we assume the caller won't change
			// any of the parameters after first submitting the line.
		}
	}

	// Group quad overlays by their texture/mask combination for efficient rendering
	// TODO: consider doing this directly in Submit()
	for (size_t i = 0; i < m->quads.size(); ++i)
	{
		SOverlayQuad* const quad = m->quads[i];

		QuadBatchKey textures(quad->m_Texture, quad->m_TextureMask);
		QuadBatchData& batchRenderData = m->quadBatchMap[textures]; // will create entry if it doesn't already exist

		// add overlay to list of quads
		batchRenderData.m_Quads.push_back(quad);
	}

	const CVector3D vOffset(0, OverlayRenderer::OVERLAY_VOFFSET, 0);

	// Write quad overlay vertices/indices to VA backing store
	VertexArrayIterator<CVector3D> vertexPos = m->quadAttributePos.GetIterator<CVector3D>();
	VertexArrayIterator<SColor4ub> vertexColor = m->quadAttributeColor.GetIterator<SColor4ub>();
	VertexArrayIterator<float[2]> vertexUV = m->quadAttributeUV.GetIterator<float[2]>();

	size_t indicesIdx = 0;
	size_t totalNumQuads = 0;

	for (OverlayRendererInternals::QuadBatchMap::iterator it = m->quadBatchMap.begin(); it != m->quadBatchMap.end(); ++it)
	{
		QuadBatchData& batchRenderData = (it->second);
		batchRenderData.m_NumRenderQuads = 0;

		if (batchRenderData.m_Quads.empty())
			continue;

		// Remember the current index into the (entire) indices array as our base offset for this batch
		batchRenderData.m_IndicesBase = indicesIdx;

		// points to the index where each iteration's vertices will be appended
		for (size_t i = 0; i < batchRenderData.m_Quads.size() && totalNumQuads < OverlayRendererInternals::MAX_QUAD_OVERLAYS; i++)
		{
			const SOverlayQuad* quad = batchRenderData.m_Quads[i];

			const SColor4ub quadColor = quad->m_Color.AsSColor4ub();

			*vertexPos++ = quad->m_Corners[0] + vOffset;
			*vertexPos++ = quad->m_Corners[1] + vOffset;
			*vertexPos++ = quad->m_Corners[2] + vOffset;
			*vertexPos++ = quad->m_Corners[3] + vOffset;

			(*vertexUV)[0] = 0;
			(*vertexUV)[1] = 0;
			++vertexUV;
			(*vertexUV)[0] = 0;
			(*vertexUV)[1] = 1;
			++vertexUV;
			(*vertexUV)[0] = 1;
			(*vertexUV)[1] = 1;
			++vertexUV;
			(*vertexUV)[0] = 1;
			(*vertexUV)[1] = 0;
			++vertexUV;

			*vertexColor++ = quadColor;
			*vertexColor++ = quadColor;
			*vertexColor++ = quadColor;
			*vertexColor++ = quadColor;

			indicesIdx += 6;

			totalNumQuads++;
			batchRenderData.m_NumRenderQuads++;
		}
	}

	m->quadVertices.Upload();
	// don't free the backing store! we'll overwrite it on the next frame to save a reallocation.

	m->quadVertices.PrepareForRendering();
}

void OverlayRenderer::Upload(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext)
{
	m->quadVertices.UploadIfNeeded(deviceCommandContext);
	m->quadIndices.UploadIfNeeded(deviceCommandContext);
}

void OverlayRenderer::RenderOverlaysBeforeWater(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext)
{
	PROFILE3("overlays (before)");
	GPU_SCOPED_LABEL(deviceCommandContext, "Render overlays before water");

	for (SOverlayLine* line : m->lines)
	{
		if (line->m_Coords.empty())
			continue;

		g_Renderer.GetDebugRenderer().DrawLine(line->m_Coords, line->m_Color, static_cast<float>(line->m_Thickness));
	}
}

void OverlayRenderer::RenderOverlaysAfterWater(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext)
{
	PROFILE3("overlays (after)");
	GPU_SCOPED_LABEL(deviceCommandContext, "Render overlays after water");

	RenderTexturedOverlayLines(deviceCommandContext);
	RenderQuadOverlays(deviceCommandContext);
	RenderSphereOverlays(deviceCommandContext);
}

void OverlayRenderer::RenderTexturedOverlayLines(Renderer::Backend::IDeviceCommandContext* deviceCommandContext)
{
	if (m->texlines.empty())
		return;

	CLOSTexture& los = g_Renderer.GetSceneRenderer().GetScene().GetLOSTexture();

	if (m->shaderTexLineNormal.technique)
	{
		const CShaderTechniquePtr& shaderTechnique = m->shaderTexLineNormal.GetTechnique();
		deviceCommandContext->SetGraphicsPipelineState(
			shaderTechnique->GetGraphicsPipelineState());
		deviceCommandContext->BeginPass();

		Renderer::Backend::IShaderProgram* shaderTexLineNormal = shaderTechnique->GetShader();

		deviceCommandContext->SetTexture(
			shaderTexLineNormal->GetBindingSlot(str_losTex), los.GetTexture());

		const CMatrix3D transform =
			g_Renderer.GetSceneRenderer().GetViewCamera().GetViewProjection();
		deviceCommandContext->SetUniform(
			shaderTexLineNormal->GetBindingSlot(str_transform), transform.AsFloatArray());
		deviceCommandContext->SetUniform(
			shaderTexLineNormal->GetBindingSlot(str_losTransform),
			los.GetTextureMatrix()[0], los.GetTextureMatrix()[12]);

		// batch render only the non-always-visible overlay lines using the normal shader
		RenderTexturedOverlayLines(deviceCommandContext, shaderTexLineNormal, false);

		deviceCommandContext->EndPass();
	}

	if (m->shaderTexLineAlwaysVisible.technique)
	{
		const CShaderTechniquePtr& shaderTechnique = m->shaderTexLineAlwaysVisible.GetTechnique();
		deviceCommandContext->SetGraphicsPipelineState(
			shaderTechnique->GetGraphicsPipelineState());
		deviceCommandContext->BeginPass();

		Renderer::Backend::IShaderProgram* shaderTexLineAlwaysVisible =
			shaderTechnique->GetShader();

		// TODO: losTex and losTransform are unused in the always visible shader; see if these can be safely omitted
		deviceCommandContext->SetTexture(
			shaderTexLineAlwaysVisible->GetBindingSlot(str_losTex), los.GetTexture());

		const CMatrix3D transform =
			g_Renderer.GetSceneRenderer().GetViewCamera().GetViewProjection();
		deviceCommandContext->SetUniform(
			shaderTexLineAlwaysVisible->GetBindingSlot(str_transform), transform.AsFloatArray());
		deviceCommandContext->SetUniform(
			shaderTexLineAlwaysVisible->GetBindingSlot(str_losTransform),
			los.GetTextureMatrix()[0], los.GetTextureMatrix()[12]);

		// batch render only the always-visible overlay lines using the LoS-ignored shader
		RenderTexturedOverlayLines(deviceCommandContext, shaderTexLineAlwaysVisible, true);

		deviceCommandContext->EndPass();
	}
}

void OverlayRenderer::RenderTexturedOverlayLines(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext,
	Renderer::Backend::IShaderProgram* shader, bool alwaysVisible)
{
	for (size_t i = 0; i < m->texlines.size(); ++i)
	{
		SOverlayTexturedLine* line = m->texlines[i];

		// render only those lines matching the requested alwaysVisible status
		if (!line->m_RenderData || line->m_AlwaysVisible != alwaysVisible)
			continue;

		ENSURE(line->m_RenderData);
		line->m_RenderData->Render(
			deviceCommandContext, m->texturedLineVertexInputLayout, *line, shader);
	}
}

void OverlayRenderer::RenderQuadOverlays(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext)
{
	if (m->quadBatchMap.empty())
		return;

	if (!m->shaderQuadOverlay.technique)
		return;

	const CShaderTechniquePtr& shaderTechnique = m->shaderQuadOverlay.GetTechnique();
	deviceCommandContext->SetGraphicsPipelineState(
		shaderTechnique->GetGraphicsPipelineState());
	deviceCommandContext->BeginPass();

	Renderer::Backend::IShaderProgram* shader = shaderTechnique->GetShader();

	CLOSTexture& los = g_Renderer.GetSceneRenderer().GetScene().GetLOSTexture();

	deviceCommandContext->SetTexture(
		shader->GetBindingSlot(str_losTex), los.GetTexture());
	deviceCommandContext->SetUniform(
		shader->GetBindingSlot(str_losTransform),
		los.GetTextureMatrix()[0], los.GetTextureMatrix()[12]);

	const CMatrix3D transform =
		g_Renderer.GetSceneRenderer().GetViewCamera().GetViewProjection();
	deviceCommandContext->SetUniform(
		shader->GetBindingSlot(str_transform), transform.AsFloatArray());

	const uint32_t vertexStride = m->quadVertices.GetStride();
	const uint32_t firstVertexOffset = m->quadVertices.GetOffset() * vertexStride;

	deviceCommandContext->SetVertexInputLayout(m->quadVertexInputLayout);

	deviceCommandContext->SetVertexBuffer(
		0, m->quadVertices.GetBuffer(), firstVertexOffset);
	deviceCommandContext->SetIndexBuffer(m->quadIndices.GetBuffer());

	const int32_t baseTexBindingSlot = shader->GetBindingSlot(str_baseTex);
	const int32_t maskTexBindingSlot = shader->GetBindingSlot(str_maskTex);

	for (OverlayRendererInternals::QuadBatchMap::iterator it = m->quadBatchMap.begin(); it != m->quadBatchMap.end(); ++it)
	{
		QuadBatchData& batchRenderData = it->second;
		const size_t batchNumQuads = batchRenderData.m_NumRenderQuads;

		if (batchNumQuads == 0)
			continue;

		const QuadBatchKey& maskPair = it->first;

		maskPair.m_Texture->UploadBackendTextureIfNeeded(deviceCommandContext);
		maskPair.m_TextureMask->UploadBackendTextureIfNeeded(deviceCommandContext);

		deviceCommandContext->SetTexture(
			baseTexBindingSlot, maskPair.m_Texture->GetBackendTexture());
		deviceCommandContext->SetTexture(
			maskTexBindingSlot, maskPair.m_TextureMask->GetBackendTexture());

		deviceCommandContext->DrawIndexed(m->quadIndices.GetOffset() + batchRenderData.m_IndicesBase, batchNumQuads * 6, 0);

		g_Renderer.GetStats().m_DrawCalls++;
		g_Renderer.GetStats().m_OverlayTris += batchNumQuads*2;
	}

	deviceCommandContext->EndPass();
}

void OverlayRenderer::RenderForegroundOverlays(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext,
	const CCamera& viewCamera)
{
	PROFILE3("overlays (fg)");
	GPU_SCOPED_LABEL(deviceCommandContext, "Render foreground overlays");

	const CVector3D right = -viewCamera.GetOrientation().GetLeft();
	const CVector3D up = viewCamera.GetOrientation().GetUp();

	const CShaderTechniquePtr& shaderTechnique = m->shaderForegroundOverlay.GetTechnique();
	deviceCommandContext->SetGraphicsPipelineState(
		shaderTechnique->GetGraphicsPipelineState());
	deviceCommandContext->BeginPass();

	Renderer::Backend::IShaderProgram* shader = shaderTechnique->GetShader();

	const CMatrix3D transform =
		g_Renderer.GetSceneRenderer().GetViewCamera().GetViewProjection();
	deviceCommandContext->SetUniform(
		shader->GetBindingSlot(str_transform), transform.AsFloatArray());

	const CVector2D uvs[6] =
	{
		{0.0f, 1.0f},
		{1.0f, 1.0f},
		{1.0f, 0.0f},
		{0.0f, 1.0f},
		{1.0f, 0.0f},
		{0.0f, 0.0f},
	};

	deviceCommandContext->SetVertexInputLayout(m->foregroundVertexInputLayout);

	deviceCommandContext->SetVertexBufferData(
		1, &uvs[0], std::size(uvs) * sizeof(uvs[0]));

	const int32_t baseTexBindingSlot = shader->GetBindingSlot(str_baseTex);
	const int32_t colorMulBindingSlot = shader->GetBindingSlot(str_colorMul);

	for (size_t i = 0; i < m->sprites.size(); ++i)
	{
		SOverlaySprite* sprite = m->sprites[i];
		if (!i || sprite->m_Texture != m->sprites[i - 1]->m_Texture)
		{
			sprite->m_Texture->UploadBackendTextureIfNeeded(deviceCommandContext);
			deviceCommandContext->SetTexture(
				baseTexBindingSlot, sprite->m_Texture->GetBackendTexture());
		}

		deviceCommandContext->SetUniform(
			colorMulBindingSlot, sprite->m_Color.AsFloatArray());

		const CVector3D position[6] =
		{
			sprite->m_Position + right*sprite->m_X0 + up*sprite->m_Y0,
			sprite->m_Position + right*sprite->m_X1 + up*sprite->m_Y0,
			sprite->m_Position + right*sprite->m_X1 + up*sprite->m_Y1,
			sprite->m_Position + right*sprite->m_X0 + up*sprite->m_Y0,
			sprite->m_Position + right*sprite->m_X1 + up*sprite->m_Y1,
			sprite->m_Position + right*sprite->m_X0 + up*sprite->m_Y1
		};

		deviceCommandContext->SetVertexBufferData(
			0, &position[0].X, std::size(position) * sizeof(position[0]));

		deviceCommandContext->Draw(0, 6);

		g_Renderer.GetStats().m_DrawCalls++;
		g_Renderer.GetStats().m_OverlayTris += 2;
	}

	deviceCommandContext->EndPass();
}

static void TessellateSphereFace(const CVector3D& a, u16 ai,
								 const CVector3D& b, u16 bi,
								 const CVector3D& c, u16 ci,
								 std::vector<float>& vertexes, std::vector<u16>& indexes, int level)
{
	if (level == 0)
	{
		indexes.push_back(ai);
		indexes.push_back(bi);
		indexes.push_back(ci);
	}
	else
	{
		CVector3D d = (a + b).Normalized();
		CVector3D e = (b + c).Normalized();
		CVector3D f = (c + a).Normalized();
		int di = vertexes.size() / 3; vertexes.push_back(d.X); vertexes.push_back(d.Y); vertexes.push_back(d.Z);
		int ei = vertexes.size() / 3; vertexes.push_back(e.X); vertexes.push_back(e.Y); vertexes.push_back(e.Z);
		int fi = vertexes.size() / 3; vertexes.push_back(f.X); vertexes.push_back(f.Y); vertexes.push_back(f.Z);
		TessellateSphereFace(a,ai, d,di, f,fi, vertexes, indexes, level-1);
		TessellateSphereFace(d,di, b,bi, e,ei, vertexes, indexes, level-1);
		TessellateSphereFace(f,fi, e,ei, c,ci, vertexes, indexes, level-1);
		TessellateSphereFace(d,di, e,ei, f,fi, vertexes, indexes, level-1);
	}
}

static void TessellateSphere(std::vector<float>& vertexes, std::vector<u16>& indexes, int level)
{
	/* Start with a tetrahedron, then tessellate */
	float s = sqrtf(0.5f);
#define VERT(a,b,c) vertexes.push_back(a); vertexes.push_back(b); vertexes.push_back(c);
	VERT(-s,  0, -s);
	VERT( s,  0, -s);
	VERT( s,  0,  s);
	VERT(-s,  0,  s);
	VERT( 0, -1,  0);
	VERT( 0,  1,  0);
#define FACE(a,b,c) \
	TessellateSphereFace( \
		CVector3D(vertexes[a*3], vertexes[a*3+1], vertexes[a*3+2]), a, \
		CVector3D(vertexes[b*3], vertexes[b*3+1], vertexes[b*3+2]), b, \
		CVector3D(vertexes[c*3], vertexes[c*3+1], vertexes[c*3+2]), c, \
		vertexes, indexes, level);
	FACE(0,4,1);
	FACE(1,4,2);
	FACE(2,4,3);
	FACE(3,4,0);
	FACE(1,5,0);
	FACE(2,5,1);
	FACE(3,5,2);
	FACE(0,5,3);
#undef FACE
#undef VERT
}

void OverlayRendererInternals::GenerateSphere()
{
	if (sphereVertexes.empty())
		TessellateSphere(sphereVertexes, sphereIndexes, 3);
}

void OverlayRenderer::RenderSphereOverlays(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext)
{
	PROFILE3("overlays (spheres)");

	if (m->spheres.empty() || m->shaderOverlaySolid.technique)
		return;

	const CShaderTechniquePtr& shaderTechnique = m->shaderOverlaySolid.GetTechnique();
	deviceCommandContext->SetGraphicsPipelineState(
		shaderTechnique->GetGraphicsPipelineState());
	deviceCommandContext->BeginPass();

	Renderer::Backend::IShaderProgram* shader = shaderTechnique->GetShader();

	const CMatrix3D transform =
		g_Renderer.GetSceneRenderer().GetViewCamera().GetViewProjection();
	deviceCommandContext->SetUniform(
		shader->GetBindingSlot(str_transform), transform.AsFloatArray());

	m->GenerateSphere();

	deviceCommandContext->SetVertexInputLayout(m->sphereVertexInputLayout);

	deviceCommandContext->SetVertexBufferData(
		0, m->sphereVertexes.data(), m->sphereVertexes.size() * sizeof(m->sphereVertexes[0]));
	deviceCommandContext->SetIndexBufferData(
		m->sphereIndexes.data(), m->sphereIndexes.size() * sizeof(m->sphereIndexes[0]));

	for (const SOverlaySphere* sphere : m->spheres)
	{
		const CVector4D instancingTransform{
			sphere->m_Center.X, sphere->m_Center.Y, sphere->m_Center.Z, sphere->m_Radius};

		deviceCommandContext->SetUniform(
			shader->GetBindingSlot(str_instancingTransform),
			instancingTransform.AsFloatArray());

		deviceCommandContext->SetUniform(
			shader->GetBindingSlot(str_color), sphere->m_Color.AsFloatArray());

		deviceCommandContext->DrawIndexed(0, m->sphereIndexes.size(), 0);

		g_Renderer.GetStats().m_DrawCalls++;
		g_Renderer.GetStats().m_OverlayTris = m->sphereIndexes.size()/3;
	}

	deviceCommandContext->EndPass();
}
