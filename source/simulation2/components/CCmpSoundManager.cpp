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

#include "simulation2/system/Component.h"
#include "ICmpSoundManager.h"

#include "simulation2/MessageTypes.h"
#include "simulation2/components/ICmpPosition.h"
#include "simulation2/components/ICmpRangeManager.h"
#include "simulation2/components/ICmpOwnership.h"

#include "soundmanager/ISoundManager.h"

class CCmpSoundManager final : public ICmpSoundManager
{
public:
	static void ClassInit(CComponentManager&)
	{
	}

	DEFAULT_COMPONENT_ALLOCATOR(SoundManager)

	static std::string GetSchema()
	{
		return "<a:component type='system'/><empty/>";
	}

	void Init(const CParamNode&) override
	{
	}

	void Deinit() override
	{
	}

	void Serialize(ISerializer&) override
	{
		// Do nothing here - sounds are purely local, and don't need to be preserved across saved games etc
		// (If we add music support in here then we might want to save the music state, though)
	}

	void Deserialize(const CParamNode& paramNode, IDeserializer&) override
	{
		Init(paramNode);
	}

	void PlaySoundGroup(const std::wstring& name, entity_id_t source) override
	{
		if (!g_SoundManager || (source == INVALID_ENTITY))
			return;

		int currentPlayer = GetSimContext().GetCurrentDisplayedPlayer();

		CmpPtr<ICmpRangeManager> cmpRangeManager(GetSystemEntity());
		if (!cmpRangeManager || (cmpRangeManager->GetLosVisibility(source, currentPlayer) != LosVisibility::VISIBLE))
			return;

		CmpPtr<ICmpPosition> cmpPosition(GetSimContext(), source);
		if (!cmpPosition || !cmpPosition->IsInWorld())
			return;

		bool playerOwned = false;
		CmpPtr<ICmpOwnership> cmpOwnership(GetSimContext(), source);
		if (cmpOwnership)
			playerOwned = cmpOwnership->GetOwner() == currentPlayer;

		CVector3D sourcePos = CVector3D(cmpPosition->GetPosition());
		g_SoundManager->PlayAsGroup(name, sourcePos, source, playerOwned);
	}

	void PlaySoundGroupAtPosition(const std::wstring& name, const CFixedVector3D& sourcePos) override
	{
		if (!g_SoundManager)
			return;
		g_SoundManager->PlayAsGroup(name, CVector3D(sourcePos), INVALID_ENTITY, false);
	}

	void PlaySoundGroupForPlayer(const VfsPath& groupPath, const player_id_t playerId) const override
	{
		if (!g_SoundManager)
			return;
		g_SoundManager->PlayAsGroup(groupPath, CVector3D(0.f, 0.f, 0.f), INVALID_ENTITY, GetSimContext().GetCurrentDisplayedPlayer() == playerId);
	}

	void StopMusic() override
	{
		if (!g_SoundManager)
			return;
		g_SoundManager->Pause(true);
	}

};

REGISTER_COMPONENT_TYPE(SoundManager)


