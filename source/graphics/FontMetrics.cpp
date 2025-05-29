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
#include "FontMetrics.h"

#include "graphics/Font.h"
#include "graphics/FontManager.h"
#include "ps/Filesystem.h"
#include "ps/CLogger.h"
#include "renderer/Renderer.h"

CFontMetrics::CFontMetrics(CStrIntern font)
{
	m_Font = g_Renderer.GetFontManager().LoadFont(font);
}

float CFontMetrics::GetHeight() const
{
	if (!m_Font)
		return 6;
	return m_Font->GetHeight();
}

float CFontMetrics::GetCharacterWidth(wchar_t c) const
{
	if (!m_Font)
		return 6;
	return m_Font->GetCharacterWidth(c);
}

void CFontMetrics::CalculateStringSize(const wchar_t* string, float& w, float& h) const
{
	if (!m_Font)
		w = h = 0;
	else
		m_Font->CalculateStringSize(string, w, h);
}

float CFontMetrics::GetCapHeight()
{
	if (!m_Font)
		return 0.0f;
	return m_Font->GetCapHeight();
}
