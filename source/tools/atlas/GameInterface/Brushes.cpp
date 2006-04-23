#include "precompiled.h"

#include "Brushes.h"

#include "ps/Game.h"
#include "ps/World.h"
#include "graphics/Terrain.h"
#include "lib/ogl.h"
#include "maths/MathUtil.h"
#include "renderer/TerrainOverlay.h"

using namespace AtlasMessage;

class BrushTerrainOverlay : public TerrainOverlay
{
public:
	BrushTerrainOverlay(const Brush* brush)
		: TerrainOverlay(300), m_Brush(brush)
	{
	}

	void GetTileExtents(
		int& min_i_inclusive, int& min_j_inclusive,
		int& max_i_inclusive, int& max_j_inclusive)
	{
		m_Brush->GetBottomLeft(min_i_inclusive, min_j_inclusive);
		m_Brush->GetTopRight(max_i_inclusive, max_j_inclusive);

		// But since brushes deal with vertices instead of tiles,
		// we don't want to include the top/right row
		--max_i_inclusive;
		--max_j_inclusive;
	}

	void ProcessTile(int i, int j)
	{
		int i0, j0;
		m_Brush->GetBottomLeft(i0, j0);
		// Colour this tile based on the average of the surrounding vertices
		float avg = (
			m_Brush->Get(i-i0, j-j0)   + m_Brush->Get(i-i0+1, j-j0) +
			m_Brush->Get(i-i0, j-j0+1) + m_Brush->Get(i-i0+1, j-j0+1)
			) / 4.f;
		RenderTile(CColor(0, 1, 0, avg*0.8f), false);
		RenderTileOutline(CColor(1, 1, 1, 0.4f), 1, true);
	}

	const AtlasMessage::Brush* m_Brush;
};

Brush::Brush()
: m_W(0), m_H(0), m_TerrainOverlay(NULL)
{
}

Brush::~Brush()
{
	delete m_TerrainOverlay;
}

void Brush::SetData(int w, int h, const std::vector<float>& data)
{
	m_W = w;
	m_H = h;

	m_Data = data;
}

void Brush::GetCentre(int& x, int& y) const
{
	CVector3D c = m_Centre;
	if (m_W % 2) c.X += CELL_SIZE/2.f;
	if (m_H % 2) c.Z += CELL_SIZE/2.f;
	i32 cx, cy;
	CTerrain::CalcFromPosition(c, cx, cy);

	x = cx;
	y = cy;
}

void Brush::GetBottomLeft(int& x, int& y) const
{
	GetCentre(x, y);
	x -= (m_W-1)/2;
	y -= (m_H-1)/2;
}

void Brush::GetTopRight(int& x, int& y) const
{
	GetBottomLeft(x, y);
	x += m_W-1;
	y += m_H-1;
}

void Brush::SetRenderEnabled(bool enabled)
{
	if (enabled && !m_TerrainOverlay)
	{
		m_TerrainOverlay = new BrushTerrainOverlay(this);
	}
	else if (!enabled && m_TerrainOverlay)
	{
		delete m_TerrainOverlay;
		m_TerrainOverlay = NULL;
	}
}

Brush AtlasMessage::g_CurrentBrush;
