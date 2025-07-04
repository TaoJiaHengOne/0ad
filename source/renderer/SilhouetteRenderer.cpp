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

#include "SilhouetteRenderer.h"

#include "graphics/Camera.h"
#include "graphics/HFTracer.h"
#include "graphics/Model.h"
#include "graphics/Patch.h"
#include "graphics/ShaderManager.h"
#include "maths/MathUtil.h"
#include "ps/CStrInternStatic.h"
#include "ps/Profile.h"
#include "renderer/DebugRenderer.h"
#include "renderer/Renderer.h"
#include "renderer/Scene.h"

#include <cfloat>

extern int g_xres, g_yres;

// For debugging
static const bool g_DisablePreciseIntersections = false;

SilhouetteRenderer::SilhouetteRenderer()
{
	m_DebugEnabled = false;
}

void SilhouetteRenderer::AddOccluder(CPatch* patch)
{
	m_SubmittedPatchOccluders.push_back(patch);
}

void SilhouetteRenderer::AddOccluder(CModel* model)
{
	m_SubmittedModelOccluders.push_back(model);
}

void SilhouetteRenderer::AddCaster(CModel* model)
{
	m_SubmittedModelCasters.push_back(model);
}

/*
 * Silhouettes are the solid-colored versions of units that are rendered when
 * standing behind a building or terrain, so the player won't lose them.
 *
 * The rendering is done in CRenderer::RenderSilhouettes, by rendering the
 * units (silhouette casters) and buildings/terrain (silhouette occluders)
 * in an extra pass using depth and stencil buffers. It's very inefficient to
 * render those objects when they're not actually going to contribute to a
 * silhouette.
 *
 * This class is responsible for finding the subset of casters/occluders
 * that might contribute to a silhouette and will need to be rendered.
 *
 * The algorithm is largely based on sweep-and-prune for detecting intersection
 * along a single axis:
 *
 * First we compute the 2D screen-space bounding box of every occluder, and
 * their minimum distance from the camera. We also compute the screen-space
 * position of each caster (approximating them as points, which is not perfect
 * but almost always good enough).
 *
 * We split each occluder's screen-space bounds into a left ('in') edge and
 * right ('out') edge. We put those edges plus the caster points into a list,
 * and sort by x coordinate.
 *
 * Then we walk through the list, maintaining an active set of occluders.
 * An 'in' edge will add an occluder to the set, an 'out' edge will remove it.
 * When we reach a caster point, the active set contains all the occluders that
 * intersect it in x. We do a quick test of y and depth coordinates against
 * each occluder in the set. If they pass that test, we do a more precise ray
 * vs bounding box test (for model occluders) or ray vs patch (for terrain
 * occluders) to see if we really need to render that caster and occluder.
 *
 * Performance relies on the active set being quite small. Given the game's
 * typical occluder sizes and camera angles, this works out okay.
 *
 * We have to do precise ray/patch intersection tests for terrain, because
 * if we just used the patch's bounding box, pretty much every unit would
 * be seen as intersecting the patch it's standing on.
 *
 * We store screen-space coordinates as 14-bit integers (0..16383) because
 * that lets us pack and sort the edge/point list efficiently.
 */

static const u16 g_MaxCoord = 1 << 14;
static const u16 g_HalfMaxCoord = g_MaxCoord / 2;

struct Occluder
{
	CRenderableObject* renderable;
	bool isPatch;
	u16 x0, y0, x1, y1;
	float z;
	bool rendered;
};

struct Caster
{
	CModel* model;
	u16 x, y;
	float z;
	bool rendered;
};

enum { EDGE_IN, EDGE_OUT, POINT };

// Entry is essentially:
//   struct Entry {
//     u16 id; // index into occluders array
//     u16 type : 2;
//     u16 x : 14;
//  };
// where x is in the most significant bits, so that sorting as a uint32_t
// is the same as sorting by x. To avoid worrying about endianness and the
// compiler's ability to handle bitfields efficiently, we use uint32_t instead
// of the actual struct.

typedef uint32_t Entry;

static Entry EntryCreate(int type, u16 id, u16 x) { return (x << 18) | (type << 16) | id; }
static int EntryGetId(Entry e) { return e & 0xffff; }
static int EntryGetType(Entry e) { return (e >> 16) & 3; }

struct ActiveList
{
	std::vector<u16> m_Ids;

	void Add(u16 id)
	{
		m_Ids.push_back(id);
	}

	void Remove(u16 id)
	{
		ssize_t sz = m_Ids.size();
		for (ssize_t i = sz-1; i >= 0; --i)
		{
			if (m_Ids[i] == id)
			{
				m_Ids[i] = m_Ids[sz-1];
				m_Ids.pop_back();
				return;
			}
		}
		debug_warn(L"Failed to find id");
	}
};

static void ComputeScreenBounds(Occluder& occluder, const CBoundingBoxAligned& bounds, CMatrix3D& proj)
{
	u16 x0 = std::numeric_limits<u16>::max();
	u16 y0 = std::numeric_limits<u16>::max();
	u16 x1 = std::numeric_limits<u16>::min();
	u16 y1 = std::numeric_limits<u16>::min();
	float z0 = std::numeric_limits<float>::max();
	for (size_t ix = 0; ix <= 1; ++ix)
	{
		for (size_t iy = 0; iy <= 1; ++iy)
		{
			for (size_t iz = 0; iz <= 1; ++iz)
			{
				CVector4D svec = proj.Transform(CVector4D(bounds[ix].X, bounds[iy].Y, bounds[iz].Z, 1.0f));
				// Avoid overflows
				u16 svx = static_cast<u16>(Clamp(g_HalfMaxCoord + g_HalfMaxCoord * (svec.X / svec.W), 0.f, static_cast<float>(g_MaxCoord - 1)));
				u16 svy = static_cast<u16>(Clamp(g_HalfMaxCoord + g_HalfMaxCoord * (svec.Y / svec.W), 0.f, static_cast<float>(g_MaxCoord - 1)));
				x0 = std::min(x0, svx);
				y0 = std::min(y0, svy);
				x1 = std::max(x1, svx);
				y1 = std::max(y1, svy);
				z0 = std::min(z0, svec.Z / svec.W);
			}
		}
	}
	// TODO: there must be a quicker way to do this than to test every vertex,
	// given the symmetry of the bounding box

	occluder.x0 = Clamp(x0, std::numeric_limits<u16>::min(), static_cast<u16>(g_MaxCoord - 1));
	occluder.y0 = Clamp(y0, std::numeric_limits<u16>::min(), static_cast<u16>(g_MaxCoord - 1));
	occluder.x1 = Clamp(x1, std::numeric_limits<u16>::min(), static_cast<u16>(g_MaxCoord - 1));
	occluder.y1 = Clamp(y1, std::numeric_limits<u16>::min(), static_cast<u16>(g_MaxCoord - 1));
	occluder.z = z0;
}

static void ComputeScreenPos(Caster& caster, const CVector3D& pos, CMatrix3D& proj)
{
	CVector4D svec = proj.Transform(CVector4D(pos.X, pos.Y, pos.Z, 1.0f));
	caster.x = static_cast<u16>(Clamp(g_HalfMaxCoord + g_HalfMaxCoord * (svec.X / svec.W), 0.f, static_cast<float>(g_MaxCoord - 1)));
	caster.y = static_cast<u16>(Clamp(g_HalfMaxCoord + g_HalfMaxCoord * (svec.Y / svec.W), 0.f, static_cast<float>(g_MaxCoord - 1)));
	caster.z = svec.Z / svec.W;
}

void SilhouetteRenderer::ComputeSubmissions(const CCamera& camera)
{
	PROFILE3("compute silhouettes");

	m_DebugBounds.clear();
	m_DebugRects.clear();
	m_DebugSpheres.clear();

	m_VisiblePatchOccluders.clear();
	m_VisibleModelOccluders.clear();
	m_VisibleModelCasters.clear();

	std::vector<Occluder> occluders;
	std::vector<Caster> casters;
	std::vector<Entry> entries;

	occluders.reserve(m_SubmittedModelOccluders.size() + m_SubmittedPatchOccluders.size());
	casters.reserve(m_SubmittedModelCasters.size());
	entries.reserve((m_SubmittedModelOccluders.size() + m_SubmittedPatchOccluders.size()) * 2 + m_SubmittedModelCasters.size());

	CMatrix3D proj = camera.GetViewProjection();

	// Bump the positions of unit casters upwards a bit, so they're not always
	// detected as intersecting the terrain they're standing on
	CVector3D posOffset(0.0f, 0.1f, 0.0f);

#if 0
	// For debugging ray-patch intersections - casts a ton of rays and draws
	// a sphere where they intersect
	for (int y = 0; y < g_yres; y += 8)
	{
		for (int x = 0; x < g_xres; x += 8)
		{
			SOverlaySphere sphere;
			sphere.m_Color = CColor(1, 0, 0, 1);
			sphere.m_Radius = 0.25f;
			sphere.m_Center = camera.GetWorldCoordinates(x, y, false);

			CVector3D origin, dir;
			camera.BuildCameraRay(x, y, origin, dir);

			for (size_t i = 0; i < m_SubmittedPatchOccluders.size(); ++i)
			{
				CPatch* occluder = m_SubmittedPatchOccluders[i];
				if (CHFTracer::PatchRayIntersect(occluder, origin, dir, &sphere.m_Center))
					sphere.m_Color = CColor(0, 0, 1, 1);
			}
			m_DebugSpheres.push_back(sphere);
		}
	}
#endif

	{
		PROFILE("compute bounds");

		for (size_t i = 0; i < m_SubmittedModelOccluders.size(); ++i)
		{
			CModel* occluder = m_SubmittedModelOccluders[i];

			Occluder d;
			d.renderable = occluder;
			d.isPatch = false;
			d.rendered = false;
			ComputeScreenBounds(d, occluder->GetWorldBounds(), proj);

			// Skip zero-sized occluders, so we don't need to worry about EDGE_OUT
			// getting sorted before EDGE_IN
			if (d.x0 == d.x1 || d.y0 == d.y1)
				continue;

			u16 id = static_cast<u16>(occluders.size());
			occluders.push_back(d);

			entries.push_back(EntryCreate(EDGE_IN, id, d.x0));
			entries.push_back(EntryCreate(EDGE_OUT, id, d.x1));
		}

		for (size_t i = 0; i < m_SubmittedPatchOccluders.size(); ++i)
		{
			CPatch* occluder = m_SubmittedPatchOccluders[i];

			Occluder d;
			d.renderable = occluder;
			d.isPatch = true;
			d.rendered = false;
			ComputeScreenBounds(d, occluder->GetWorldBounds(), proj);

			// Skip zero-sized occluders
			if (d.x0 == d.x1 || d.y0 == d.y1)
				continue;

			u16 id = static_cast<u16>(occluders.size());
			occluders.push_back(d);

			entries.push_back(EntryCreate(EDGE_IN, id, d.x0));
			entries.push_back(EntryCreate(EDGE_OUT, id, d.x1));
		}

		for (size_t i = 0; i < m_SubmittedModelCasters.size(); ++i)
		{
			CModel* model = m_SubmittedModelCasters[i];
			CVector3D pos = model->GetTransform().GetTranslation() + posOffset;

			Caster d;
			d.model = model;
			d.rendered = false;
			ComputeScreenPos(d, pos, proj);

			u16 id = static_cast<u16>(casters.size());
			casters.push_back(d);

			entries.push_back(EntryCreate(POINT, id, d.x));
		}
	}

	// Make sure the u16 id didn't overflow
	ENSURE(occluders.size() < 65536 && casters.size() < 65536);

	{
		PROFILE("sorting");
		std::sort(entries.begin(), entries.end());
	}

	{
		PROFILE("sweeping");

		ActiveList active;
		CVector3D cameraPos = camera.GetOrientation().GetTranslation();

		for (size_t i = 0; i < entries.size(); ++i)
		{
			Entry e = entries[i];
			int type = EntryGetType(e);
			u16 id = EntryGetId(e);
			if (type == EDGE_IN)
				active.Add(id);
			else if (type == EDGE_OUT)
				active.Remove(id);
			else
			{
				Caster& caster = casters[id];
				for (size_t j = 0; j < active.m_Ids.size(); ++j)
				{
					Occluder& occluder = occluders[active.m_Ids[j]];

					if (caster.y < occluder.y0 || caster.y > occluder.y1)
						continue;

					if (caster.z < occluder.z)
						continue;

					// No point checking further if both are already being rendered
					if (caster.rendered && occluder.rendered)
						continue;

					if (!g_DisablePreciseIntersections)
					{
						CVector3D pos = caster.model->GetTransform().GetTranslation() + posOffset;
						if (occluder.isPatch)
						{
							CPatch* patch = static_cast<CPatch*>(occluder.renderable);
							if (!CHFTracer::PatchRayIntersect(patch, pos, cameraPos - pos, NULL))
								continue;
						}
						else
						{
							float tmin, tmax;
							if (!occluder.renderable->GetWorldBounds().RayIntersect(pos, cameraPos - pos, tmin, tmax))
								continue;
						}
					}

					caster.rendered = true;
					occluder.rendered = true;
				}
			}
		}
	}

	if (m_DebugEnabled)
	{
		for (size_t i = 0; i < occluders.size(); ++i)
		{
			DebugRect r;
			r.color = occluders[i].rendered ? CColor(1.0f, 1.0f, 0.0f, 1.0f) : CColor(0.2f, 0.2f, 0.0f, 1.0f);
			r.x0 = occluders[i].x0;
			r.y0 = occluders[i].y0;
			r.x1 = occluders[i].x1;
			r.y1 = occluders[i].y1;
			m_DebugRects.push_back(r);

			DebugBounds b;
			b.color = r.color;
			b.bounds = occluders[i].renderable->GetWorldBounds();
			m_DebugBounds.push_back(b);
		}
	}

	for (size_t i = 0; i < occluders.size(); ++i)
	{
		if (occluders[i].rendered)
		{
			if (occluders[i].isPatch)
				m_VisiblePatchOccluders.push_back(static_cast<CPatch*>(occluders[i].renderable));
			else
				m_VisibleModelOccluders.push_back(static_cast<CModel*>(occluders[i].renderable));
		}
	}

	for (size_t i = 0; i < casters.size(); ++i)
		if (casters[i].rendered)
			m_VisibleModelCasters.push_back(casters[i].model);
}

void SilhouetteRenderer::RenderSubmitOverlays(SceneCollector& collector)
{
	for (size_t i = 0; i < m_DebugSpheres.size(); i++)
		collector.Submit(&m_DebugSpheres[i]);
}

void SilhouetteRenderer::RenderSubmitOccluders(SceneCollector& collector)
{
	for (size_t i = 0; i < m_VisiblePatchOccluders.size(); ++i)
		collector.Submit(m_VisiblePatchOccluders[i]);

	for (size_t i = 0; i < m_VisibleModelOccluders.size(); ++i)
		collector.SubmitNonRecursive(m_VisibleModelOccluders[i]);
}

void SilhouetteRenderer::RenderSubmitCasters(SceneCollector& collector)
{
	for (size_t i = 0; i < m_VisibleModelCasters.size(); ++i)
		collector.SubmitNonRecursive(m_VisibleModelCasters[i]);
}

void SilhouetteRenderer::RenderDebugBounds(Renderer::Backend::IDeviceCommandContext*)
{
	if (m_DebugBounds.empty())
		return;

	for (size_t i = 0; i < m_DebugBounds.size(); ++i)
		g_Renderer.GetDebugRenderer().DrawBoundingBox(m_DebugBounds[i].bounds, m_DebugBounds[i].color, true);
}

void SilhouetteRenderer::RenderDebugOverlays(
	Renderer::Backend::IDeviceCommandContext* deviceCommandContext)
{
	if (m_DebugRects.empty())
		return;

	// TODO: use CCanvas2D for drawing rects.
	CMatrix3D m;
	m.SetIdentity();
	m.Scale(1.0f, -1.f, 1.0f);
	m.Translate(0.0f, (float)g_yres, -1000.0f);

	CMatrix3D proj;
	proj.SetOrtho(0.f, g_MaxCoord, 0.f, g_MaxCoord, -1.f, 1000.f);
	m = proj * m;

	if (!m_ShaderTech)
	{
		m_ShaderTech = g_Renderer.GetShaderManager().LoadEffect(
			str_solid, {},
			[](Renderer::Backend::SGraphicsPipelineStateDesc& pipelineStateDesc)
			{
				pipelineStateDesc.rasterizationState.polygonMode = Renderer::Backend::PolygonMode::LINE;
				pipelineStateDesc.rasterizationState.cullMode = Renderer::Backend::CullMode::NONE;
			});

		const std::array<Renderer::Backend::SVertexAttributeFormat, 1> attributes{{
			{Renderer::Backend::VertexAttributeStream::POSITION,
				Renderer::Backend::Format::R32G32_SFLOAT, 0, sizeof(float) * 2,
				Renderer::Backend::VertexAttributeRate::PER_VERTEX, 0}
		}};
		m_VertexInputLayout = g_Renderer.GetVertexInputLayout(attributes);
	}

	deviceCommandContext->BeginPass();
	deviceCommandContext->SetGraphicsPipelineState(
		m_ShaderTech->GetGraphicsPipelineState());

	Renderer::Backend::IShaderProgram* shader = m_ShaderTech->GetShader();
	deviceCommandContext->SetUniform(
		shader->GetBindingSlot(str_transform), proj.AsFloatArray());

	const int32_t colorBindingSlot = shader->GetBindingSlot(str_color);
	for (const DebugRect& r : m_DebugRects)
	{
		deviceCommandContext->SetUniform(
			colorBindingSlot, r.color.AsFloatArray());
		const float vertices[] =
		{
			r.x0, r.y0,
			r.x1, r.y0,
			r.x1, r.y1,
			r.x0, r.y0,
			r.x1, r.y1,
			r.x0, r.y1,
		};

		deviceCommandContext->SetVertexInputLayout(m_VertexInputLayout);

		deviceCommandContext->SetVertexBufferData(
			0, vertices, std::size(vertices) * sizeof(vertices[0]));

		deviceCommandContext->Draw(0, 6);
	}

	deviceCommandContext->EndPass();
}

void SilhouetteRenderer::EndFrame()
{
	m_SubmittedPatchOccluders.clear();
	m_SubmittedModelOccluders.clear();
	m_SubmittedModelCasters.clear();
}
