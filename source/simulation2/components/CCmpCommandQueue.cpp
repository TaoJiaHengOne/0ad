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
#include "ICmpCommandQueue.h"

#include "ps/CLogger.h"
#include "ps/Game.h"
#include "ps/Profile.h"
#include "scriptinterface/FunctionWrapper.h"
#include "scriptinterface/JSON.h"
#include "simulation2/system/TurnManager.h"

class CCmpCommandQueue final : public ICmpCommandQueue
{
public:
	static void ClassInit(CComponentManager&)
	{
	}

	DEFAULT_COMPONENT_ALLOCATOR(CommandQueue)

	std::vector<SimulationCommand> m_LocalQueue;

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

	void Serialize(ISerializer& serialize) override
	{
		ScriptRequest rq(GetSimContext().GetScriptInterface());

		serialize.NumberU32_Unbounded("num commands", (u32)m_LocalQueue.size());
		for (size_t i = 0; i < m_LocalQueue.size(); ++i)
		{
			serialize.NumberI32_Unbounded("player", m_LocalQueue[i].player);
			serialize.ScriptVal("data", &m_LocalQueue[i].data);
		}
	}

	void Deserialize(const CParamNode&, IDeserializer& deserialize) override
	{
		ScriptRequest rq(GetSimContext().GetScriptInterface());

		u32 numCmds;
		deserialize.NumberU32_Unbounded("num commands", numCmds);
		for (size_t i = 0; i < numCmds; ++i)
		{
			i32 player;
			JS::RootedValue data(rq.cx);
			deserialize.NumberI32_Unbounded("player", player);
			deserialize.ScriptVal("data", &data);
			m_LocalQueue.emplace_back(SimulationCommand(player, rq.cx, data));
		}
	}

	void PushLocalCommand(player_id_t player, JS::HandleValue cmd) override
	{
		ScriptRequest rq(GetSimContext().GetScriptInterface());
		m_LocalQueue.emplace_back(SimulationCommand(player, rq.cx, cmd));
	}

	void PostNetworkCommand(JS::HandleValue cmd1) override
	{
		ScriptRequest rq(GetSimContext().GetScriptInterface());

		// TODO: This is a workaround because we need to pass a MutableHandle to StringifyJSON.
		JS::RootedValue cmd(rq.cx, cmd1.get());

		PROFILE2_EVENT("post net command");
		PROFILE2_ATTR("command: %s", Script::StringifyJSON(rq, &cmd, false).c_str());

		// TODO: would be nicer to not use globals
		if (g_Game && g_Game->GetTurnManager())
			g_Game->GetTurnManager()->PostCommand(cmd);
	}

	void FlushTurn(const std::vector<SimulationCommand>& commands) override
	{
		const ScriptInterface& scriptInterface = GetSimContext().GetScriptInterface();
		ScriptRequest rq(scriptInterface);

		JS::RootedValue global(rq.cx, rq.globalValue());
		std::vector<SimulationCommand> localCommands;
		m_LocalQueue.swap(localCommands);

		for (size_t i = 0; i < localCommands.size(); ++i)
		{
			bool ok = ScriptFunction::CallVoid(rq, global, "ProcessCommand", localCommands[i].player, localCommands[i].data);
			if (!ok)
				LOGERROR("Failed to call ProcessCommand() global script function");
		}

		for (size_t i = 0; i < commands.size(); ++i)
		{
			bool ok = ScriptFunction::CallVoid(rq, global, "ProcessCommand", commands[i].player, commands[i].data);
			if (!ok)
				LOGERROR("Failed to call ProcessCommand() global script function");
		}
	}
};

REGISTER_COMPONENT_TYPE(CommandQueue)
