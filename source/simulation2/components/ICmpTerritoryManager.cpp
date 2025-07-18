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

#include "ICmpTerritoryManager.h"

#include "simulation2/system/InterfaceScripted.h"

BEGIN_INTERFACE_WRAPPER(TerritoryManager)
DEFINE_INTERFACE_METHOD("GetOwner", ICmpTerritoryManager, GetOwner)
DEFINE_INTERFACE_METHOD("GetNeighbours", ICmpTerritoryManager, GetNeighbours)
DEFINE_INTERFACE_METHOD("IsConnected", ICmpTerritoryManager, IsConnected)
DEFINE_INTERFACE_METHOD("SetTerritoryBlinking", ICmpTerritoryManager, SetTerritoryBlinking)
DEFINE_INTERFACE_METHOD("IsTerritoryBlinking", ICmpTerritoryManager, IsTerritoryBlinking)
DEFINE_INTERFACE_METHOD("GetTerritoryPercentage", ICmpTerritoryManager, GetTerritoryPercentage)
DEFINE_INTERFACE_METHOD("UpdateColors", ICmpTerritoryManager, UpdateColors)
DEFINE_INTERFACE_METHOD("IsVisible", ICmpTerritoryManager, IsVisible)
END_INTERFACE_WRAPPER(TerritoryManager)
