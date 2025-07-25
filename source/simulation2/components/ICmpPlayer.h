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

#ifndef INCLUDED_ICMPPLAYER
#define INCLUDED_ICMPPLAYER

#include "simulation2/system/Interface.h"

struct CColor;
class CFixedVector3D;

/**
 * Player data.
 * (This interface includes the functions needed by native code for loading maps,
 * and for minimap rendering; most player interaction is handled by scripts instead.
 * Also includes some functions needed for the non visual autostart.)
 */
class ICmpPlayer : public IComponent
{
public:
	virtual CColor GetDisplayedColor() = 0;
	virtual CFixedVector3D GetStartingCameraPos() = 0;
	virtual CFixedVector3D GetStartingCameraRot() = 0;

	virtual bool HasStartingCamera() = 0;
	virtual bool IsRemoved() = 0;
	virtual std::string GetState() = 0;

	// See the cpp file for why this is implemented in C++.
	virtual bool IsActive() = 0;

	DECLARE_INTERFACE_TYPE(Player)
};

#endif // INCLUDED_ICMPPLAYER
