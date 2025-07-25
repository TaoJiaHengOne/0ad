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

#include "gui/CGUI.h"
#include "gui/SettingTypes/EScrollOrientation.h"
#include "gui/SettingTypes/CGUIString.h"
#include "ps/CLogger.h"

class CGUIList;
class CGUISeries;

template <>
bool CGUI::ParseString<bool>(const CGUI*, const CStrW& Value, bool& Output)
{
	if (Value == L"true")
		Output = true;
	else if (Value == L"false")
		Output = false;
	else
		return false;

	return true;
}

template <>
bool CGUI::ParseString<i32>(const CGUI*, const CStrW& Value, int& Output)
{
	Output = Value.ToInt();
	return true;
}

template <>
bool CGUI::ParseString<u32>(const CGUI*, const CStrW& Value, u32& Output)
{
	Output = Value.ToUInt();
	return true;
}

template <>
bool CGUI::ParseString<float>(const CGUI*, const CStrW& Value, float& Output)
{
	Output = Value.ToFloat();
	return true;
}

template <>
bool CGUI::ParseString<CRect>(const CGUI*, const CStrW& Value, CRect& Output)
{
	const unsigned int NUM_COORDS = 4;
	float coords[NUM_COORDS];
	std::wstringstream stream;
	stream.str(Value);
	// Parse each coordinate
	for (unsigned int i = 0; i < NUM_COORDS; ++i)
	{
		if (stream.eof())
		{
			LOGWARNING("Too few CRect parameters (min %i). Your input: '%s'", NUM_COORDS, Value.ToUTF8().c_str());
			return false;
		}
		stream >> coords[i];
		if ((stream.rdstate() & std::wstringstream::failbit) != 0)
		{
			LOGWARNING("Unable to parse CRect parameters. Your input: '%s'", Value.ToUTF8().c_str());
			return false;
		}
	}

	if (!stream.eof())
	{
		LOGWARNING("Too many CRect parameters (max %i). Your input: '%s'", NUM_COORDS, Value.ToUTF8().c_str());
		return false;
	}

	// Finally the rectangle values
	Output = CRect(coords[0], coords[1], coords[2], coords[3]);

	return true;
}

template <>
bool CGUI::ParseString<CGUISize>(const CGUI*, const CStrW& Value, CGUISize& Output)
{
	return Output.FromString(Value.ToUTF8());
}

template <>
bool CGUI::ParseString<CGUIColor>(const CGUI* pGUI, const CStrW& Value, CGUIColor& Output)
{
	return Output.ParseString(*pGUI, Value.ToUTF8());
}

template <>
bool CGUI::ParseString<CSize2D>(const CGUI*, const CStrW& Value, CSize2D& Output)
{
	const unsigned int NUM_COORDS = 2;
	float coords[NUM_COORDS];
	std::wstringstream stream;
	stream.str(Value);
	// Parse each coordinate
	for (unsigned int i = 0; i < NUM_COORDS; ++i)
	{
		if (stream.eof())
		{
			LOGWARNING("Too few CSize2D parameters (min %i). Your input: '%s'", NUM_COORDS, Value.ToUTF8().c_str());
			return false;
		}
		stream >> coords[i];
		if ((stream.rdstate() & std::wstringstream::failbit) != 0)
		{
			LOGWARNING("Unable to parse CSize2D parameters. Your input: '%s'", Value.ToUTF8().c_str());
			return false;
		}
	}

	Output.Width = coords[0];
	Output.Height = coords[1];

	if (!stream.eof())
	{
		LOGWARNING("Too many CSize2D parameters (max %i). Your input: '%s'", NUM_COORDS, Value.ToUTF8().c_str());
		return false;
	}

	return true;
}

template <>
bool CGUI::ParseString<CVector2D>(const CGUI*, const CStrW& Value, CVector2D& Output)
{
	const unsigned int NUM_COORDS = 2;
	float coords[NUM_COORDS];
	std::wstringstream stream;
	stream.str(Value);
	// Parse each coordinate
	for (unsigned int i = 0; i < NUM_COORDS; ++i)
	{
		if (stream.eof())
		{
			LOGWARNING("Too few CVector2D parameters (min %i). Your input: '%s'", NUM_COORDS, Value.ToUTF8().c_str());
			return false;
		}
		stream >> coords[i];
		if ((stream.rdstate() & std::wstringstream::failbit) != 0)
		{
			LOGWARNING("Unable to parse CVector2D parameters. Your input: '%s'", Value.ToUTF8().c_str());
			return false;
		}
	}

	Output.X = coords[0];
	Output.Y = coords[1];

	if (!stream.eof())
	{
		LOGWARNING("Too many CVector2D parameters (max %i). Your input: '%s'", NUM_COORDS, Value.ToUTF8().c_str());
		return false;
	}

	return true;
}

template <>
bool CGUI::ParseString<EAlign>(const CGUI*, const CStrW& Value, EAlign& Output)
{
	if (Value == L"left")
		Output = EAlign::LEFT;
	else if (Value == L"center")
		Output = EAlign::CENTER;
	else if (Value == L"right")
		Output = EAlign::RIGHT;
	else
		return false;

	return true;
}

template <>
bool CGUI::ParseString<EVAlign>(const CGUI*, const CStrW& Value, EVAlign& Output)
{
	if (Value == L"top")
		Output = EVAlign::TOP;
	else if (Value == L"center")
		Output = EVAlign::CENTER;
	else if (Value == L"bottom")
		Output = EVAlign::BOTTOM;
	else
		return false;

	return true;
}

template <>
bool CGUI::ParseString<CGUIString>(const CGUI*, const CStrW& Value, CGUIString& Output)
{
	Output.SetValue(Value);
	return true;
}

template <>
bool CGUI::ParseString<CStr>(const CGUI*, const CStrW& Value, CStr& Output)
{
	Output = Value.ToUTF8();
	return true;
}

template <>
bool CGUI::ParseString<CStrW>(const CGUI*, const CStrW& Value, CStrW& Output)
{
	Output = Value;
	return true;
}

template <>
bool CGUI::ParseString<CGUISpriteInstance>(const CGUI*, const CStrW& Value, CGUISpriteInstance& Output)
{
	Output = CGUISpriteInstance(Value.ToUTF8());
	return true;
}

template <>
bool CGUI::ParseString<CGUISeries>(const CGUI*, const CStrW& UNUSED(Value), CGUISeries& UNUSED(Output))
{
	return false;
}

template <>
bool CGUI::ParseString<CGUIList>(const CGUI*, const CStrW& UNUSED(Value), CGUIList& UNUSED(Output))
{
	return false;
}

template <>
bool CGUI::ParseString<EScrollOrientation>(const CGUI*, const CStrW& Value, EScrollOrientation& Output)
{
	if (Value == L"vertical")
		Output = EScrollOrientation::VERTICAL;
	else if (Value == L"horizontal")
		Output = EScrollOrientation::HORIZONTAL;
	else if (Value == L"both")
		Output = EScrollOrientation::BOTH;
	else
		return false;

	return true;
}
