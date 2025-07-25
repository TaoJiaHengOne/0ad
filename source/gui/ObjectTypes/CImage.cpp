/* Copyright (C) 2024 Wildfire Games.
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

#include "CImage.h"

#include "gui/CGUI.h"

CImage::CImage(CGUI& pGUI)
	: IGUIObject(pGUI),
	  m_Sprite(this, "sprite")
{
}

CImage::~CImage()
{
}

void CImage::Draw(CCanvas2D& canvas)
{
	m_pGUI.DrawSprite(m_Sprite, canvas, m_CachedActualSize, m_VisibleArea);
}
