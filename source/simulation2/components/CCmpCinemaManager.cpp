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
#include "ICmpCinemaManager.h"

#include "ps/CLogger.h"
#include "simulation2/components/ICmpOverlayRenderer.h"
#include "simulation2/components/ICmpRangeManager.h"
#include "simulation2/components/ICmpSelectable.h"
#include "simulation2/components/ICmpTerritoryManager.h"
#include "simulation2/MessageTypes.h"
#include "simulation2/Simulation2.h"


class CCmpCinemaManager final : public ICmpCinemaManager
{
public:
	static void ClassInit(CComponentManager& componentManager)
	{
		componentManager.SubscribeToMessageType(MT_Update);
	}

	DEFAULT_COMPONENT_ALLOCATOR(CinemaManager)

	static std::string GetSchema()
	{
		return "<a:component type='system'/><empty/>";
	}

	void Init(const CParamNode&) override
	{
		m_Enabled = false;
		m_MapRevealed = false;
		m_ElapsedTime = fixed::Zero();
		m_TotalTime = fixed::Zero();
		m_CurrentPathElapsedTime = fixed::Zero();
	}

	void Deinit() override
	{
	}

	void Serialize(ISerializer& serializer) override
	{
		serializer.Bool("Enabled", m_Enabled);
		serializer.NumberFixed_Unbounded("ElapsedTime", m_ElapsedTime);
		serializer.NumberFixed_Unbounded("CurrentPathElapsedTime", m_CurrentPathElapsedTime);
		serializer.Bool("MapRevealed", m_MapRevealed);

		serializer.NumberU32_Unbounded("NumberOfPaths", m_Paths.size());
		for (const std::pair<const CStrW, CCinemaPath>& it : m_Paths)
			SerializePath(it.second, serializer);

		serializer.NumberU32_Unbounded("NumberOfQueuedPaths", m_PathQueue.size());
		for (const CCinemaPath& path : m_PathQueue)
			serializer.String("PathName", path.GetName(), 1, 128);
	}

	void Deserialize(const CParamNode&, IDeserializer& deserializer) override
	{
		deserializer.Bool("Enabled", m_Enabled);
		deserializer.NumberFixed_Unbounded("ElapsedTime", m_ElapsedTime);
		deserializer.NumberFixed_Unbounded("CurrentPathElapsedTime", m_CurrentPathElapsedTime);
		deserializer.Bool("MapRevealed", m_MapRevealed);

		uint32_t numberOfPaths = 0;
		deserializer.NumberU32_Unbounded("NumberOfPaths", numberOfPaths);
		for (uint32_t i = 0; i < numberOfPaths; ++i)
		{
			CCinemaPath path = DeserializePath(deserializer);
			m_Paths[path.GetName()] = path;
		}

		uint32_t numberOfQueuedPaths = 0;
		deserializer.NumberU32_Unbounded("NumberOfQueuedPaths", numberOfQueuedPaths);
		for (uint32_t i = 0; i < numberOfQueuedPaths; ++i)
		{
			CStrW pathName;
			deserializer.String("PathName", pathName, 1, 128);
			ENSURE(HasPath(pathName));
			AddCinemaPathToQueue(pathName);
		}

		if (!m_PathQueue.empty())
		{
			m_PathQueue.front().m_TimeElapsed = m_CurrentPathElapsedTime.ToFloat();
			m_PathQueue.front().Validate();
		}

		SetEnabled(m_Enabled);
	}

	void HandleMessage(const CMessage& msg, bool UNUSED(global)) override
	{
		switch (msg.GetType())
		{
		case MT_Update:
		{
			const CMessageUpdate &msgData = static_cast<const CMessageUpdate&>(msg);
			if (!m_Enabled)
				break;

			m_ElapsedTime += msgData.turnLength;
			m_CurrentPathElapsedTime += msgData.turnLength;
			if (m_CurrentPathElapsedTime >= m_PathQueue.front().GetDuration())
			{
				CMessageCinemaPathEnded msgCinemaPathEnded(m_PathQueue.front().GetName());
				m_PathQueue.pop_front();
				GetSimContext().GetComponentManager().PostMessage(SYSTEM_ENTITY, msgCinemaPathEnded);
				m_CurrentPathElapsedTime = fixed::Zero();

				if (!m_PathQueue.empty())
					m_PathQueue.front().Reset();
			}

			if (m_ElapsedTime >= m_TotalTime)
			{
				m_CurrentPathElapsedTime = fixed::Zero();
				m_ElapsedTime = fixed::Zero();
				m_TotalTime = fixed::Zero();
				SetEnabled(false);
				GetSimContext().GetComponentManager().PostMessage(SYSTEM_ENTITY, CMessageCinemaQueueEnded());
			}

			break;
		}
		default:
			break;
		}
	}

	void AddPath(const CCinemaPath& path) override
	{
		if (m_Paths.find(path.GetName()) != m_Paths.end())
		{
			LOGWARNING("Path with name '%s' already exists", path.GetName().ToUTF8());
			return;
		}
		m_Paths[path.GetName()] = path;
	}

	void AddCinemaPathToQueue(const CStrW& name) override
	{
		if (!HasPath(name))
		{
			LOGWARNING("Path with name '%s' doesn't exist", name.ToUTF8());
			return;
		}
		m_PathQueue.push_back(m_Paths[name]);

		if (m_PathQueue.size() == 1)
			m_PathQueue.front().Reset();
		m_TotalTime += m_Paths[name].GetDuration();
	}

	void Play() override
	{
		SetEnabled(true);
	}

	void Stop() override
	{
		SetEnabled(false);
	}

	bool HasPath(const CStrW& name) const override
	{
		return m_Paths.find(name) != m_Paths.end();
	}

	void ClearQueue() override
	{
		m_PathQueue.clear();
	}

	void DeletePath(const CStrW& name) override
	{
		if (!HasPath(name))
		{
			LOGWARNING("Path with name '%s' doesn't exist", name.ToUTF8());
			return;
		}
		m_PathQueue.remove_if([name](const CCinemaPath& path) { return path.GetName() == name; });
		m_Paths.erase(name);
	}

	const std::map<CStrW, CCinemaPath>& GetPaths() const override
	{
		return m_Paths;
	}

	void SetPaths(const std::map<CStrW, CCinemaPath>& newPaths) override
	{
		m_Paths = newPaths;
	}

	const std::list<CCinemaPath>& GetQueue() const override
	{
		return m_PathQueue;
	}

	bool IsEnabled() const override
	{
		return m_Enabled;
	}

	void SetEnabled(bool enabled) override
	{
		if (m_PathQueue.empty() && enabled)
			enabled = false;

		if (m_Enabled == enabled)
			return;

		CmpPtr<ICmpRangeManager> cmpRangeManager(GetSimContext().GetSystemEntity());
		CmpPtr<ICmpTerritoryManager> cmpTerritoryManager(GetSimContext().GetSystemEntity());
		if (cmpRangeManager)
		{
			if (enabled)
				m_MapRevealed = cmpRangeManager->GetLosRevealAll(-1);
			// TODO: improve m_MapRevealed state and without fade in
			cmpRangeManager->SetLosRevealAll(-1, enabled);
		}
		if (cmpTerritoryManager)
			cmpTerritoryManager->SetVisibility(!enabled);
		ICmpSelectable::SetOverrideVisibility(!enabled);
		ICmpOverlayRenderer::SetOverrideVisibility(!enabled);

		m_Enabled = enabled;
	}

	void PlayQueue(const float deltaRealTime, CCamera* camera) override
	{
		if (m_PathQueue.empty())
			return;
		m_PathQueue.front().Play(deltaRealTime, camera);
	}

private:

	void SerializePath(const CCinemaPath& path, ISerializer& serializer)
	{
		const CCinemaData* data = path.GetData();

		serializer.String("PathName", data->m_Name, 1, 128);
		serializer.String("PathOrientation", data->m_Orientation, 1, 128);
		serializer.String("PathMode", data->m_Mode, 1, 128);
		serializer.String("PathStyle", data->m_Style, 1, 128);
		serializer.NumberFixed_Unbounded("PathTimescale", data->m_Timescale);
		serializer.Bool("LookAtTarget", data->m_LookAtTarget);

		serializer.NumberU32("NumberOfNodes", path.GetAllNodes().size(), 1, MAX_SPLINE_NODES);
		const std::vector<SplineData>& nodes = path.GetAllNodes();
		for (size_t i = 0; i < nodes.size(); ++i)
		{
			if (i > 0)
				serializer.NumberFixed_Unbounded("NodeDeltaTime", nodes[i - 1].Distance);
			else
				serializer.NumberFixed_Unbounded("NodeDeltaTime", fixed::Zero());

			serializer.NumberFixed_Unbounded("PositionX", nodes[i].Position.X);
			serializer.NumberFixed_Unbounded("PositionY", nodes[i].Position.Y);
			serializer.NumberFixed_Unbounded("PositionZ", nodes[i].Position.Z);

			serializer.NumberFixed_Unbounded("RotationX", nodes[i].Rotation.X);
			serializer.NumberFixed_Unbounded("RotationY", nodes[i].Rotation.Y);
			serializer.NumberFixed_Unbounded("RotationZ", nodes[i].Rotation.Z);
		}

		if (!data->m_LookAtTarget)
			return;

		const std::vector<SplineData>& targetNodes = path.GetTargetSpline().GetAllNodes();
		serializer.NumberU32("NumberOfTargetNodes", targetNodes.size(), 1, MAX_SPLINE_NODES);
		for (size_t i = 0; i < targetNodes.size(); ++i)
		{
			if (i > 0)
				serializer.NumberFixed_Unbounded("NodeDeltaTime", targetNodes[i - 1].Distance);
			else
				serializer.NumberFixed_Unbounded("NodeDeltaTime", fixed::Zero());
			serializer.NumberFixed_Unbounded("PositionX", targetNodes[i].Position.X);
			serializer.NumberFixed_Unbounded("PositionY", targetNodes[i].Position.Y);
			serializer.NumberFixed_Unbounded("PositionZ", targetNodes[i].Position.Z);
		}
	}

	CCinemaPath DeserializePath(IDeserializer& deserializer)
	{
		CCinemaData data;

		deserializer.String("PathName", data.m_Name, 1, 128);
		deserializer.String("PathOrientation", data.m_Orientation, 1, 128);
		deserializer.String("PathMode", data.m_Mode, 1, 128);
		deserializer.String("PathStyle", data.m_Style, 1, 128);
		deserializer.NumberFixed_Unbounded("PathTimescale", data.m_Timescale);
		deserializer.Bool("LookAtTarget", data.m_LookAtTarget);

		TNSpline pathSpline, targetSpline;
		uint32_t numberOfNodes = 0;
		deserializer.NumberU32("NumberOfNodes", numberOfNodes, 1, MAX_SPLINE_NODES);
		for (uint32_t j = 0; j < numberOfNodes; ++j)
		{
			SplineData node;
			deserializer.NumberFixed_Unbounded("NodeDeltaTime", node.Distance);

			deserializer.NumberFixed_Unbounded("PositionX", node.Position.X);
			deserializer.NumberFixed_Unbounded("PositionY", node.Position.Y);
			deserializer.NumberFixed_Unbounded("PositionZ", node.Position.Z);

			deserializer.NumberFixed_Unbounded("RotationX", node.Rotation.X);
			deserializer.NumberFixed_Unbounded("RotationY", node.Rotation.Y);
			deserializer.NumberFixed_Unbounded("RotationZ", node.Rotation.Z);

			pathSpline.AddNode(node.Position, node.Rotation, node.Distance);
		}

		if (data.m_LookAtTarget)
		{
			uint32_t numberOfTargetNodes = 0;
			deserializer.NumberU32("NumberOfTargetNodes", numberOfTargetNodes, 1, MAX_SPLINE_NODES);
			for (uint32_t j = 0; j < numberOfTargetNodes; ++j)
			{
				SplineData node;
				deserializer.NumberFixed_Unbounded("NodeDeltaTime", node.Distance);

				deserializer.NumberFixed_Unbounded("PositionX", node.Position.X);
				deserializer.NumberFixed_Unbounded("PositionY", node.Position.Y);
				deserializer.NumberFixed_Unbounded("PositionZ", node.Position.Z);

				targetSpline.AddNode(node.Position, CFixedVector3D(), node.Distance);
			}
		}

		return CCinemaPath(data, pathSpline, targetSpline);
	}

	bool m_Enabled;
	std::map<CStrW, CCinemaPath> m_Paths;
	std::list<CCinemaPath> m_PathQueue;

	// States before playing
	bool m_MapRevealed;

	fixed m_ElapsedTime;
	fixed m_TotalTime;
	fixed m_CurrentPathElapsedTime;
};

REGISTER_COMPONENT_TYPE(CinemaManager)
