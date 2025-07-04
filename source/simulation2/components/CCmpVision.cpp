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
#include "ICmpVision.h"

#include "simulation2/MessageTypes.h"
#include "simulation2/components/ICmpPlayerManager.h"
#include "simulation2/components/ICmpRangeManager.h"
#include "simulation2/components/ICmpValueModificationManager.h"

class CCmpVision final : public ICmpVision
{
public:
	static void ClassInit(CComponentManager& componentManager)
	{
		componentManager.SubscribeToMessageType(MT_OwnershipChanged);
		componentManager.SubscribeToMessageType(MT_ValueModification);
		componentManager.SubscribeToMessageType(MT_Deserialized);
	}

	DEFAULT_COMPONENT_ALLOCATOR(Vision)

	// Template state:

	entity_pos_t m_Range, m_BaseRange;

	// TODO: The reveal shore system should be replaced by a general
	// system of "special" vision methods that are not ranges.
	bool m_RevealShore;

	static std::string GetSchema()
	{
		return
			"<element name='Range'>"
				"<data type='nonNegativeInteger'/>"
			"</element>"
			"<optional>"
				"<element name='RevealShore'>"
					"<data type='boolean'/>"
				"</element>"
			"</optional>";
	}

	void Init(const CParamNode& paramNode) override
	{
		m_BaseRange = m_Range = paramNode.GetChild("Range").ToFixed();

		if (paramNode.GetChild("RevealShore").IsOk())
			m_RevealShore = paramNode.GetChild("RevealShore").ToBool();
		else
			m_RevealShore = false;
	}

	void Deinit() override
	{
	}

	void Serialize(ISerializer&) override
	{
		// No dynamic state to serialize
	}

	void Deserialize(const CParamNode& paramNode, IDeserializer&) override
	{
		Init(paramNode);
	}

	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_OwnershipChanged:
		{
			const CMessageOwnershipChanged& msgData = static_cast<const CMessageOwnershipChanged&> (msg);
			if (msgData.entity != GetEntityId())
				break;

			ReloadRange();
			break;
		}
		case MT_ValueModification:
		{
			const CMessageValueModification& msgData = static_cast<const CMessageValueModification&> (msg);
			if (msgData.component != L"Vision")
				break;
			ReloadRange();
			break;
		}
		case MT_Deserialized:
		{
			CmpPtr<ICmpValueModificationManager> cmpValueModificationManager(GetSystemEntity());
			if (cmpValueModificationManager)
				m_Range = cmpValueModificationManager->ApplyModifications(L"Vision/Range", m_BaseRange, GetEntityId());
			break;
		}
		}
	}

	virtual void ReloadRange()
	{
		CmpPtr<ICmpValueModificationManager> cmpValueModificationManager(GetSystemEntity());
		if (!cmpValueModificationManager)
			return;

		entity_pos_t newRange = cmpValueModificationManager->ApplyModifications(L"Vision/Range", m_BaseRange, GetEntityId());
		if (newRange == m_Range)
			return;

		// Update our vision range and broadcast message
		entity_pos_t oldRange = m_Range;
		m_Range = newRange;
		CMessageVisionRangeChanged msg(GetEntityId(), oldRange, newRange);
		GetSimContext().GetComponentManager().BroadcastMessage(msg);
	}

	entity_pos_t GetRange() const override
	{
		return m_Range;
	}

	bool GetRevealShore() const override
	{
		return m_RevealShore;
	}
};

REGISTER_COMPONENT_TYPE(Vision)
