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

#include "Simulation2.h"

#include "graphics/MapReader.h"
#include "graphics/Terrain.h"
#include "lib/timer.h"
#include "lib/file/vfs/vfs_util.h"
#include "maths/MathUtil.h"
#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/Filesystem.h"
#include "ps/Loader.h"
#include "ps/Profile.h"
#include "ps/Pyrogenesis.h"
#include "ps/Util.h"
#include "ps/XML/Xeromyces.h"
#include "scriptinterface/FunctionWrapper.h"
#include "scriptinterface/JSON.h"
#include "scriptinterface/Object.h"
#include "scriptinterface/ScriptContext.h"
#include "scriptinterface/ScriptInterface.h"
#include "scriptinterface/StructuredClone.h"
#include "simulation2/MessageTypes.h"
#include "simulation2/system/ComponentManager.h"
#include "simulation2/system/ParamNode.h"
#include "simulation2/system/SimContext.h"
#include "simulation2/components/ICmpAIManager.h"
#include "simulation2/components/ICmpCommandQueue.h"
#include "simulation2/components/ICmpTemplateManager.h"

#include <fstream>
#include <iomanip>
#include <memory>

class CSimulation2Impl
{
public:
	CSimulation2Impl(CUnitManager* unitManager, ScriptContext& cx, CTerrain* terrain) :
		m_SimContext{terrain, unitManager},
		m_ComponentManager{m_SimContext, cx},
		m_InitAttributes{cx.GetGeneralJSContext()},
		m_MapSettings{cx.GetGeneralJSContext()},
		// Tests won't have config initialised
		m_EnableOOSLog{CConfigDB::GetIfInitialised("ooslog", false)},
		m_EnableSerializationTest{CConfigDB::GetIfInitialised("serializationtest", false)},
		// Handle bogus values of the arg
		m_RejoinTestTurn{std::max(CConfigDB::GetIfInitialised("rejointest", -1), -1)}
	{
		m_ComponentManager.LoadComponentTypes();

		RegisterFileReloadFunc(ReloadChangedFileCB, this);

		if (m_EnableOOSLog)
		{
			m_OOSLogPath = createDateIndexSubdirectory(psLogDir() / "oos_logs");
			debug_printf("Writing ooslogs to %s\n", m_OOSLogPath.string8().c_str());
		}
	}

	~CSimulation2Impl()
	{
		UnregisterFileReloadFunc(ReloadChangedFileCB, this);
	}

	void ResetState(bool skipScriptedComponents, bool skipAI)
	{
		m_DeltaTime = 0.0;
		m_LastFrameOffset = 0.0f;
		m_TurnNumber = 0;
		ResetComponentState(m_ComponentManager, skipScriptedComponents, skipAI);
	}

	static void ResetComponentState(CComponentManager& componentManager, bool skipScriptedComponents, bool skipAI)
	{
		componentManager.ResetState();
		componentManager.InitSystemEntity();
		componentManager.AddSystemComponents(skipScriptedComponents, skipAI);
	}

	static bool LoadDefaultScripts(CComponentManager& componentManager, std::set<VfsPath>* loadedScripts);
	static bool LoadScripts(CComponentManager& componentManager, std::set<VfsPath>* loadedScripts, const VfsPath& path);
	static bool LoadTriggerScripts(CComponentManager& componentManager, JS::HandleValue mapSettings, std::set<VfsPath>* loadedScripts);
	Status ReloadChangedFile(const VfsPath& path);

	static Status ReloadChangedFileCB(void* param, const VfsPath& path)
	{
		return static_cast<CSimulation2Impl*>(param)->ReloadChangedFile(path);
	}

	int ProgressiveLoad();
	void Update(int turnLength, const std::vector<SimulationCommand>& commands);
	static void UpdateComponents(CSimContext& simContext, fixed turnLengthFixed, const std::vector<SimulationCommand>& commands);
	void Interpolate(float simFrameLength, float frameOffset, float realFrameLength);

	void DumpState();

	CSimContext m_SimContext;
	CComponentManager m_ComponentManager;
	double m_DeltaTime;
	float m_LastFrameOffset;

	std::string m_StartupScript;
	JS::PersistentRootedValue m_InitAttributes;
	JS::PersistentRootedValue m_MapSettings;

	std::set<VfsPath> m_LoadedScripts;

	uint32_t m_TurnNumber;

	bool m_EnableOOSLog{false};
	OsPath m_OOSLogPath;

	// Functions and data for the serialization test mode: (see Update() for relevant comments)

	bool m_EnableSerializationTest{false};
	int m_RejoinTestTurn{-1};
	bool m_TestingRejoin{false};

	// Secondary simulation (NB: order matters for destruction).
	std::unique_ptr<CComponentManager> m_SecondaryComponentManager;
	std::unique_ptr<CTerrain> m_SecondaryTerrain;
	std::unique_ptr<CSimContext> m_SecondaryContext;
	std::unique_ptr<std::set<VfsPath>> m_SecondaryLoadedScripts;

	struct SerializationTestState
	{
		std::stringstream state;
		std::stringstream debug;
		std::string hash;
	};

	void DumpSerializationTestState(SerializationTestState& state, const OsPath& path, const OsPath::String& suffix);

	void ReportSerializationFailure(
		SerializationTestState* primaryStateBefore, SerializationTestState* primaryStateAfter,
		SerializationTestState* secondaryStateBefore, SerializationTestState* secondaryStateAfter);

	void InitRNGSeedSimulation();
	void InitRNGSeedAI();

	static std::vector<SimulationCommand> CloneCommandsFromOtherCompartment(const ScriptInterface& newScript, const ScriptInterface& oldScript,
		const std::vector<SimulationCommand>& commands)
	{
		std::vector<SimulationCommand> newCommands;
		newCommands.reserve(commands.size());

		ScriptRequest rqNew(newScript);
		for (const SimulationCommand& command : commands)
		{
			JS::RootedValue tmpCommand(rqNew.cx, Script::CloneValueFromOtherCompartment(newScript, oldScript, command.data));
			Script::DeepFreezeObject(rqNew, tmpCommand);
			SimulationCommand cmd(command.player, rqNew.cx, tmpCommand);
			newCommands.emplace_back(std::move(cmd));
		}
		return newCommands;
	}
};

bool CSimulation2Impl::LoadDefaultScripts(CComponentManager& componentManager, std::set<VfsPath>* loadedScripts)
{
	return (
		LoadScripts(componentManager, loadedScripts, L"simulation/components/interfaces/") &&
		LoadScripts(componentManager, loadedScripts, L"simulation/helpers/") &&
		LoadScripts(componentManager, loadedScripts, L"simulation/components/")
	);
}

bool CSimulation2Impl::LoadScripts(CComponentManager& componentManager, std::set<VfsPath>* loadedScripts, const VfsPath& path)
{
	VfsPaths pathnames;
	if (vfs::GetPathnames(g_VFS, path, L"*.js", pathnames) < 0)
		return false;

	bool ok = true;
	for (const VfsPath& scriptPath : pathnames)
	{
		if (loadedScripts)
			loadedScripts->insert(scriptPath);
		LOGMESSAGE("Loading simulation script '%s'", scriptPath.string8());
		if (!componentManager.LoadScript(scriptPath))
			ok = false;
	}
	return ok;
}

bool CSimulation2Impl::LoadTriggerScripts(CComponentManager& componentManager, JS::HandleValue mapSettings, std::set<VfsPath>* loadedScripts)
{
	bool ok = true;
	ScriptRequest rq(componentManager.GetScriptInterface());
	if (Script::HasProperty(rq, mapSettings, "TriggerScripts"))
	{
		std::vector<std::string> scriptNames;
		Script::GetProperty(rq, mapSettings, "TriggerScripts", scriptNames);
		for (const std::string& triggerScript : scriptNames)
		{
			std::string scriptName = "maps/" + triggerScript;
			if (loadedScripts)
			{
				if (loadedScripts->find(scriptName) != loadedScripts->end())
					continue;
				loadedScripts->insert(scriptName);
			}
			LOGMESSAGE("Loading trigger script '%s'", scriptName.c_str());
			if (!componentManager.LoadScript(scriptName.data()))
				ok = false;
		}
	}
	return ok;
}

Status CSimulation2Impl::ReloadChangedFile(const VfsPath& path)
{
	// Ignore if this file wasn't loaded as a script
	// (TODO: Maybe we ought to load in any new .js files that are created in the right directories)
	if (m_LoadedScripts.find(path) == m_LoadedScripts.end())
		return INFO::OK;

	// If the file doesn't exist (e.g. it was deleted), don't bother loading it since that'll give an error message.
	// (Also don't bother trying to 'unload' it from the component manager, because that's not possible)
	if (!VfsFileExists(path))
		return INFO::OK;

	LOGMESSAGE("Reloading simulation script '%s'", path.string8());
	if (!m_ComponentManager.LoadScript(path, true))
		return ERR::FAIL;

	return INFO::OK;
}

int CSimulation2Impl::ProgressiveLoad()
{
	// yield after this time is reached. balances increased progress bar
	// smoothness vs. slowing down loading.
	const double end_time = timer_Time() + 200e-3;

	int ret;

	do
	{
		bool progressed = false;
		int total = 0;
		int progress = 0;

		CMessageProgressiveLoad msg(&progressed, &total, &progress);

		m_ComponentManager.BroadcastMessage(msg);

		if (!progressed || total == 0)
			return 0; // we have nothing left to load

		ret = Clamp(100*progress / total, 1, 100);
	}
	while (timer_Time() < end_time);

	return ret;
}

void CSimulation2Impl::DumpSerializationTestState(SerializationTestState& state, const OsPath& path, const OsPath::String& suffix)
{
	if (!state.hash.empty())
	{
		std::ofstream file (OsString(path / (L"hash." + suffix)), std::ofstream::out | std::ofstream::trunc);
		file << Hexify(state.hash);
	}

	if (!state.debug.str().empty())
	{
		std::ofstream file (OsString(path / (L"debug." + suffix)), std::ofstream::out | std::ofstream::trunc);
		file << state.debug.str();
	}

	if (!state.state.str().empty())
	{
		std::ofstream file (OsString(path / (L"state." + suffix)), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
		file << state.state.str();
	}
}

void CSimulation2Impl::ReportSerializationFailure(
	SerializationTestState* primaryStateBefore, SerializationTestState* primaryStateAfter,
	SerializationTestState* secondaryStateBefore, SerializationTestState* secondaryStateAfter)
{
	const OsPath path = createDateIndexSubdirectory(psLogDir() / "serializationtest");
	debug_printf("Writing serializationtest-data to %s\n", path.string8().c_str());

	// Clean up obsolete files from previous runs
	wunlink(path / "hash.before.a");
	wunlink(path / "hash.before.b");
	wunlink(path / "debug.before.a");
	wunlink(path / "debug.before.b");
	wunlink(path / "state.before.a");
	wunlink(path / "state.before.b");
	wunlink(path / "hash.after.a");
	wunlink(path / "hash.after.b");
	wunlink(path / "debug.after.a");
	wunlink(path / "debug.after.b");
	wunlink(path / "state.after.a");
	wunlink(path / "state.after.b");

	if (primaryStateBefore)
		DumpSerializationTestState(*primaryStateBefore, path, L"before.a");
	if (primaryStateAfter)
		DumpSerializationTestState(*primaryStateAfter, path, L"after.a");
	if (secondaryStateBefore)
		DumpSerializationTestState(*secondaryStateBefore, path, L"before.b");
	if (secondaryStateAfter)
		DumpSerializationTestState(*secondaryStateAfter, path, L"after.b");

	debug_warn(L"Serialization test failure");
}

void CSimulation2Impl::InitRNGSeedSimulation()
{
	u32 seed = 0;
	ScriptRequest rq(m_ComponentManager.GetScriptInterface());
	if (!Script::HasProperty(rq, m_MapSettings, "Seed") ||
		!Script::GetProperty(rq, m_MapSettings, "Seed", seed))
		LOGWARNING("CSimulation2Impl::InitRNGSeedSimulation: No seed value specified - using %d", seed);

	m_ComponentManager.SetRNGSeed(seed);
}

void CSimulation2Impl::InitRNGSeedAI()
{
	u32 seed = 0;
	ScriptRequest rq(m_ComponentManager.GetScriptInterface());
	if (!Script::HasProperty(rq, m_MapSettings, "AISeed") ||
		!Script::GetProperty(rq, m_MapSettings, "AISeed", seed))
		LOGWARNING("CSimulation2Impl::InitRNGSeedAI: No seed value specified - using %d", seed);

	CmpPtr<ICmpAIManager> cmpAIManager(m_SimContext, SYSTEM_ENTITY);
	if (cmpAIManager)
		cmpAIManager->SetRNGSeed(seed);
}

void CSimulation2Impl::Update(int turnLength, const std::vector<SimulationCommand>& commands)
{
	PROFILE3("sim update");
	PROFILE2_ATTR("turn %d", (int)m_TurnNumber);

	fixed turnLengthFixed = fixed::FromInt(turnLength) / 1000;

	/*
	 * In serialization test mode, we save the original (primary) simulation state before each turn update.
	 * We run the update, then load the saved state into a secondary context.
	 * We serialize that again and compare to the original serialization (to check that
	 * serialize->deserialize->serialize is equivalent to serialize).
	 * Then we run the update on the secondary context, and check that its new serialized
	 * state matches the primary context after the update (to check that the simulation doesn't depend
	 * on anything that's not serialized).
	 *
	 * In rejoin test mode, the secondary simulation is initialized from serialized data at turn N, then both
	 * simulations run independantly while comparing their states each turn. This is way faster than a
	 * complete serialization test and allows us to reproduce OOSes on rejoin.
	 */

	const bool serializationTestDebugDump = false; // set true to save human-readable state dumps before an error is detected, for debugging (but slow)
	const bool serializationTestHash = true; // set true to save and compare hash of state

	SerializationTestState primaryStateBefore;
	const ScriptInterface& scriptInterface = m_ComponentManager.GetScriptInterface();

	const bool startRejoinTest = (int64_t) m_RejoinTestTurn == m_TurnNumber;
	if (startRejoinTest)
		m_TestingRejoin = true;

	if (m_EnableSerializationTest || m_TestingRejoin)
	{
		ENSURE(m_ComponentManager.SerializeState(primaryStateBefore.state));
		if (serializationTestDebugDump)
			ENSURE(m_ComponentManager.DumpDebugState(primaryStateBefore.debug, false));
		if (serializationTestHash)
			ENSURE(m_ComponentManager.ComputeStateHash(primaryStateBefore.hash, false));
	}

	UpdateComponents(m_SimContext, turnLengthFixed, commands);

	if (m_EnableSerializationTest || startRejoinTest)
	{
		if (startRejoinTest)
			debug_printf("Initializing the secondary simulation\n");

		m_SecondaryTerrain = std::make_unique<CTerrain>();

		m_SecondaryContext = std::make_unique<CSimContext>(m_SecondaryTerrain.get());

		m_SecondaryComponentManager = std::make_unique<CComponentManager>(*m_SecondaryContext, scriptInterface.GetContext());
		m_SecondaryComponentManager->LoadComponentTypes();

		m_SecondaryLoadedScripts = std::make_unique<std::set<VfsPath>>();
		ENSURE(LoadDefaultScripts(*m_SecondaryComponentManager, m_SecondaryLoadedScripts.get()));
		ResetComponentState(*m_SecondaryComponentManager, false, false);

		ScriptRequest rq(scriptInterface);

		// Load the trigger scripts after we have loaded the simulation.
		{
			ScriptRequest rq2(m_SecondaryComponentManager->GetScriptInterface());
			JS::RootedValue mapSettingsCloned(rq2.cx, Script::CloneValueFromOtherCompartment(m_SecondaryComponentManager->GetScriptInterface(), scriptInterface, m_MapSettings));
			ENSURE(LoadTriggerScripts(*m_SecondaryComponentManager, mapSettingsCloned, m_SecondaryLoadedScripts.get()));
		}

		// Load the map into the secondary simulation

		LDR_BeginRegistering();
		std::unique_ptr<CMapReader> mapReader = std::make_unique<CMapReader>();

		std::string mapType;
		Script::GetProperty(rq, m_InitAttributes, "mapType", mapType);
		if (mapType == "random")
		{
			// TODO: support random map scripts
			debug_warn(L"Serialization test mode does not support random maps");
		}
		else
		{
			std::wstring mapFile;
			Script::GetProperty(rq, m_InitAttributes, "map", mapFile);

			VfsPath mapfilename = VfsPath(mapFile).ChangeExtension(L".pmp");
			mapReader->LoadMap(mapfilename, scriptInterface.GetContext(), JS::UndefinedHandleValue,
				m_SecondaryTerrain.get(), NULL, NULL, NULL, NULL, NULL, NULL,
				NULL, NULL, m_SecondaryContext.get(), INVALID_PLAYER, true); // throws exception on failure
		}

		LDR_EndRegistering();
		ENSURE(LDR_NonprogressiveLoad() == INFO::OK);
		ENSURE(m_SecondaryComponentManager->DeserializeState(primaryStateBefore.state));
	}

	if (m_EnableSerializationTest || m_TestingRejoin)
	{
		SerializationTestState secondaryStateBefore;
		ENSURE(m_SecondaryComponentManager->SerializeState(secondaryStateBefore.state));
		if (serializationTestDebugDump)
			ENSURE(m_SecondaryComponentManager->DumpDebugState(secondaryStateBefore.debug, false));
		if (serializationTestHash)
			ENSURE(m_SecondaryComponentManager->ComputeStateHash(secondaryStateBefore.hash, false));

		if (primaryStateBefore.state.str() != secondaryStateBefore.state.str() ||
			primaryStateBefore.hash != secondaryStateBefore.hash)
		{
			ReportSerializationFailure(&primaryStateBefore, NULL, &secondaryStateBefore, NULL);
		}

		SerializationTestState primaryStateAfter;
		ENSURE(m_ComponentManager.SerializeState(primaryStateAfter.state));
		if (serializationTestHash)
			ENSURE(m_ComponentManager.ComputeStateHash(primaryStateAfter.hash, false));

		UpdateComponents(*m_SecondaryContext, turnLengthFixed,
			CloneCommandsFromOtherCompartment(m_SecondaryComponentManager->GetScriptInterface(), scriptInterface, commands));
		SerializationTestState secondaryStateAfter;
		ENSURE(m_SecondaryComponentManager->SerializeState(secondaryStateAfter.state));
		if (serializationTestHash)
			ENSURE(m_SecondaryComponentManager->ComputeStateHash(secondaryStateAfter.hash, false));

		if (primaryStateAfter.state.str() != secondaryStateAfter.state.str() ||
			primaryStateAfter.hash != secondaryStateAfter.hash)
		{
			// Only do the (slow) dumping now we know we're going to need to report it
			ENSURE(m_ComponentManager.DumpDebugState(primaryStateAfter.debug, false));
			ENSURE(m_SecondaryComponentManager->DumpDebugState(secondaryStateAfter.debug, false));

			ReportSerializationFailure(&primaryStateBefore, &primaryStateAfter, &secondaryStateBefore, &secondaryStateAfter);
		}
	}

	// (TODO: we ought to schedule this for a frame where we're not
	// running the sim update, to spread the load)
	scriptInterface.GetContext().MaybeIncrementalGC();

	if (m_EnableOOSLog)
		DumpState();

	++m_TurnNumber;
}

void CSimulation2Impl::UpdateComponents(CSimContext& simContext, fixed turnLengthFixed, const std::vector<SimulationCommand>& commands)
{
	// TODO: the update process is pretty ugly, with lots of messages and dependencies
	// between different components. Ought to work out a nicer way to do this.

	CComponentManager& componentManager = simContext.GetComponentManager();

	CmpPtr<ICmpPathfinder> cmpPathfinder(simContext, SYSTEM_ENTITY);
	if (cmpPathfinder)
		cmpPathfinder->SendRequestedPaths();

	{
		PROFILE2("Sim - Update Start");
		CMessageTurnStart msgTurnStart;
		componentManager.BroadcastMessage(msgTurnStart);
	}


	CmpPtr<ICmpCommandQueue> cmpCommandQueue(simContext, SYSTEM_ENTITY);
	if (cmpCommandQueue)
		cmpCommandQueue->FlushTurn(commands);

	// Process newly generated move commands so the UI feels snappy
	if (cmpPathfinder)
	{
		cmpPathfinder->StartProcessingMoves(true);
		cmpPathfinder->SendRequestedPaths();
	}
	// Send all the update phases
	{
		PROFILE2("Sim - Update");
		CMessageUpdate msgUpdate(turnLengthFixed);
		componentManager.BroadcastMessage(msgUpdate);
	}

	{
		CMessageUpdate_MotionFormation msgUpdate(turnLengthFixed);
		componentManager.BroadcastMessage(msgUpdate);
	}

	// Process move commands for formations (group proxy)
	if (cmpPathfinder)
	{
		cmpPathfinder->StartProcessingMoves(true);
		cmpPathfinder->SendRequestedPaths();
	}

	{
		PROFILE2("Sim - Motion Unit");
		CMessageUpdate_MotionUnit msgUpdate(turnLengthFixed);
		componentManager.BroadcastMessage(msgUpdate);
	}
	{
		PROFILE2("Sim - Update Final");
		CMessageUpdate_Final msgUpdate(turnLengthFixed);
		componentManager.BroadcastMessage(msgUpdate);
	}

	// Clean up any entities destroyed during the simulation update
	componentManager.FlushDestroyedComponents();

	// Compute AI immediately at turn's end.
	CmpPtr<ICmpAIManager> cmpAIManager(simContext, SYSTEM_ENTITY);
	if (cmpAIManager)
	{
		cmpAIManager->StartComputation();
		cmpAIManager->PushCommands();
	}

	// Process all remaining moves
	if (cmpPathfinder)
	{
		cmpPathfinder->UpdateGrid();
		cmpPathfinder->StartProcessingMoves(false);
	}

	componentManager.GetScriptInterface().GetContext().RunJobs();
}

void CSimulation2Impl::Interpolate(float simFrameLength, float frameOffset, float realFrameLength)
{
	PROFILE3("sim interpolate");

	m_LastFrameOffset = frameOffset;

	CMessageInterpolate msg(simFrameLength, frameOffset, realFrameLength);
	m_ComponentManager.BroadcastMessage(msg);

	// Clean up any entities destroyed during interpolate (e.g. local corpses)
	m_ComponentManager.FlushDestroyedComponents();
}

void CSimulation2Impl::DumpState()
{
	PROFILE("DumpState");

	std::stringstream name;\
	name << std::setw(5) << std::setfill('0') << m_TurnNumber << ".txt";
	const OsPath path = m_OOSLogPath / name.str();
	std::ofstream file (OsString(path), std::ofstream::out | std::ofstream::trunc);

	if (!DirectoryExists(m_OOSLogPath))
	{
		LOGWARNING("OOS-log directory %s was deleted, creating it again.", m_OOSLogPath.string8().c_str());
		CreateDirectories(m_OOSLogPath, 0700);
	}

	file << "State hash: " << std::hex;
	std::string hashRaw;
	m_ComponentManager.ComputeStateHash(hashRaw, false);
	for (size_t i = 0; i < hashRaw.size(); ++i)
		file << std::setfill('0') << std::setw(2) << (int)(unsigned char)hashRaw[i];
	file << std::dec << "\n";

	file << "\n";

	m_ComponentManager.DumpDebugState(file, true);

	std::ofstream binfile (OsString(path.ChangeExtension(L".dat")), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
	m_ComponentManager.SerializeState(binfile);
}

////////////////////////////////////////////////////////////////

CSimulation2::CSimulation2(CUnitManager* unitManager, ScriptContext& cx, CTerrain* terrain) :
	m(std::make_unique<CSimulation2Impl>(unitManager, cx, terrain))
{
}

CSimulation2::~CSimulation2() = default;

// Forward all method calls to the appropriate CSimulation2Impl/CComponentManager methods:

void CSimulation2::EnableSerializationTest()
{
	m->m_EnableSerializationTest = true;
}

void CSimulation2::EnableRejoinTest(int rejoinTestTurn)
{
	m->m_RejoinTestTurn = rejoinTestTurn;
}

void CSimulation2::EnableOOSLog()
{
	if (m->m_EnableOOSLog)
		return;

	m->m_EnableOOSLog = true;
	m->m_OOSLogPath = createDateIndexSubdirectory(psLogDir() / "oos_logs");

	debug_printf("Writing ooslogs to %s\n", m->m_OOSLogPath.string8().c_str());
}

entity_id_t CSimulation2::AddEntity(const std::wstring& templateName)
{
	return m->m_ComponentManager.AddEntity(templateName, m->m_ComponentManager.AllocateNewEntity());
}

entity_id_t CSimulation2::AddEntity(const std::wstring& templateName, entity_id_t preferredId)
{
	return m->m_ComponentManager.AddEntity(templateName, m->m_ComponentManager.AllocateNewEntity(preferredId));
}

entity_id_t CSimulation2::AddLocalEntity(const std::wstring& templateName)
{
	return m->m_ComponentManager.AddEntity(templateName, m->m_ComponentManager.AllocateNewLocalEntity());
}

void CSimulation2::DestroyEntity(entity_id_t ent)
{
	m->m_ComponentManager.DestroyComponentsSoon(ent);
}

void CSimulation2::FlushDestroyedEntities()
{
	m->m_ComponentManager.FlushDestroyedComponents();
}

IComponent* CSimulation2::QueryInterface(entity_id_t ent, int iid) const
{
	return m->m_ComponentManager.QueryInterface(ent, iid);
}

void CSimulation2::PostMessage(entity_id_t ent, const CMessage& msg) const
{
	m->m_ComponentManager.PostMessage(ent, msg);
}

void CSimulation2::BroadcastMessage(const CMessage& msg) const
{
	m->m_ComponentManager.BroadcastMessage(msg);
}

CSimulation2::InterfaceList CSimulation2::GetEntitiesWithInterface(int iid)
{
	return m->m_ComponentManager.GetEntitiesWithInterface(iid);
}

const CSimulation2::InterfaceListUnordered& CSimulation2::GetEntitiesWithInterfaceUnordered(int iid)
{
	return m->m_ComponentManager.GetEntitiesWithInterfaceUnordered(iid);
}

const CSimContext& CSimulation2::GetSimContext() const
{
	return m->m_SimContext;
}

ScriptInterface& CSimulation2::GetScriptInterface() const
{
	return m->m_ComponentManager.GetScriptInterface();
}

void CSimulation2::PreInitGame()
{
	ScriptRequest rq(GetScriptInterface());
	JS::RootedValue global(rq.cx, rq.globalValue());
	ScriptFunction::CallVoid(rq, global, "PreInitGame");
}

void CSimulation2::InitGame()
{
	ScriptRequest rq(GetScriptInterface());
	JS::RootedValue global(rq.cx, rq.globalValue());

	JS::RootedValue settings(rq.cx);
	JS::RootedValue tmpInitAttributes(rq.cx, GetInitAttributes());
	Script::GetProperty(rq, tmpInitAttributes, "settings", &settings);

	ScriptFunction::CallVoid(rq, global, "InitGame", settings);
}

void CSimulation2::Update(int turnLength)
{
	std::vector<SimulationCommand> commands;
	m->Update(turnLength, commands);
}

void CSimulation2::Update(int turnLength, const std::vector<SimulationCommand>& commands)
{
	m->Update(turnLength, commands);
}

void CSimulation2::Interpolate(float simFrameLength, float frameOffset, float realFrameLength)
{
	m->Interpolate(simFrameLength, frameOffset, realFrameLength);
}

void CSimulation2::RenderSubmit(SceneCollector& collector, const CFrustum& frustum, bool culling)
{
	PROFILE3("sim submit");

	CMessageRenderSubmit msg(collector, frustum, culling);
	m->m_ComponentManager.BroadcastMessage(msg);
}

float CSimulation2::GetLastFrameOffset() const
{
	return m->m_LastFrameOffset;
}

bool CSimulation2::LoadScripts(const VfsPath& path)
{
	return m->LoadScripts(m->m_ComponentManager, &m->m_LoadedScripts, path);
}

bool CSimulation2::LoadDefaultScripts()
{
	return m->LoadDefaultScripts(m->m_ComponentManager, &m->m_LoadedScripts);
}

void CSimulation2::SetStartupScript(const std::string& code)
{
	m->m_StartupScript = code;
}

const std::string& CSimulation2::GetStartupScript()
{
	return m->m_StartupScript;
}

void CSimulation2::SetInitAttributes(JS::HandleValue attribs)
{
	m->m_InitAttributes = attribs;
}

JS::Value CSimulation2::GetInitAttributes()
{
	return m->m_InitAttributes.get();
}

void CSimulation2::GetInitAttributes(JS::MutableHandleValue ret)
{
	ret.set(m->m_InitAttributes);
}

void CSimulation2::SetMapSettings(const std::string& settings)
{
	Script::ParseJSON(ScriptRequest(m->m_ComponentManager.GetScriptInterface()), settings, &m->m_MapSettings);
}

void CSimulation2::SetMapSettings(JS::HandleValue settings)
{
	m->m_MapSettings = settings;

	m->InitRNGSeedSimulation();
	m->InitRNGSeedAI();
}

std::string CSimulation2::GetMapSettingsString()
{
	return Script::StringifyJSON(ScriptRequest(m->m_ComponentManager.GetScriptInterface()), &m->m_MapSettings);
}

void CSimulation2::GetMapSettings(JS::MutableHandleValue ret)
{
	ret.set(m->m_MapSettings);
}

void CSimulation2::LoadPlayerSettings(bool newPlayers)
{
	ScriptRequest rq(GetScriptInterface());
	JS::RootedValue global(rq.cx, rq.globalValue());
	ScriptFunction::CallVoid(rq, global, "LoadPlayerSettings", m->m_MapSettings, newPlayers);
}

void CSimulation2::LoadMapSettings()
{
	ScriptRequest rq(GetScriptInterface());

	JS::RootedValue global(rq.cx, rq.globalValue());

	// Initialize here instead of in Update()
	ScriptFunction::CallVoid(rq, global, "LoadMapSettings", m->m_MapSettings);

	Script::DeepFreezeObject(rq, m->m_InitAttributes);
	GetScriptInterface().SetGlobal("InitAttributes", m->m_InitAttributes, true, true, true);

	if (!m->m_StartupScript.empty())
		GetScriptInterface().LoadScript(L"map startup script", m->m_StartupScript);

	// Load the trigger scripts after we have loaded the simulation and the map.
	m->LoadTriggerScripts(m->m_ComponentManager, m->m_MapSettings, &m->m_LoadedScripts);
}

int CSimulation2::ProgressiveLoad()
{
	return m->ProgressiveLoad();
}

Status CSimulation2::ReloadChangedFile(const VfsPath& path)
{
	return m->ReloadChangedFile(path);
}

void CSimulation2::ResetState(bool skipScriptedComponents, bool skipAI)
{
	m->ResetState(skipScriptedComponents, skipAI);
}

bool CSimulation2::ComputeStateHash(std::string& outHash, bool quick)
{
	return m->m_ComponentManager.ComputeStateHash(outHash, quick);
}

bool CSimulation2::DumpDebugState(std::ostream& stream)
{
	stream << "sim turn: " << m->m_TurnNumber << std::endl;
	return m->m_ComponentManager.DumpDebugState(stream, true);
}

bool CSimulation2::SerializeState(std::ostream& stream)
{
	return m->m_ComponentManager.SerializeState(stream);
}

bool CSimulation2::DeserializeState(std::istream& stream)
{
	// TODO: need to make sure the required SYSTEM_ENTITY components get constructed
	return m->m_ComponentManager.DeserializeState(stream);
}

void CSimulation2::ActivateRejoinTest(int turn)
{
	if (m->m_RejoinTestTurn != -1)
		return;
	LOGMESSAGERENDER("Rejoin test will activate in %i turns", turn - m->m_TurnNumber);
	m->m_RejoinTestTurn = turn;
}

std::string CSimulation2::GenerateSchema()
{
	return m->m_ComponentManager.GenerateSchema();
}

static std::vector<std::string> GetJSONData(const VfsPath& path)
{
	VfsPaths pathnames;
	Status ret = vfs::GetPathnames(g_VFS, path, L"*.json", pathnames);
	if (ret != INFO::OK)
	{
		// Some error reading directory
		wchar_t error[200];
		LOGERROR("Error reading directory '%s': %s", path.string8(), utf8_from_wstring(StatusDescription(ret, error, ARRAY_SIZE(error))));
		return std::vector<std::string>();
	}

	std::vector<std::string> data;
	for (const VfsPath& p : pathnames)
	{
		// Load JSON file
		CVFSFile file;
		PSRETURN loadStatus = file.Load(g_VFS, p);
		if (loadStatus != PSRETURN_OK)
		{
			LOGERROR("GetJSONData: Failed to load file '%s': %s", p.string8(), GetErrorString(loadStatus));
			continue;
		}

		data.push_back(file.DecodeUTF8()); // assume it's UTF-8
	}

	return data;
}

std::vector<std::string> CSimulation2::GetRMSData()
{
	return GetJSONData(L"maps/random/");
}

std::vector<std::string> CSimulation2::GetVictoryConditiondData()
{
	return GetJSONData(L"simulation/data/settings/victory_conditions/");
}

static std::string ReadJSON(const VfsPath& path)
{
	if (!VfsFileExists(path))
	{
		LOGERROR("File '%s' does not exist", path.string8());
		return std::string();
	}

	// Load JSON file
	CVFSFile file;
	PSRETURN ret = file.Load(g_VFS, path);
	if (ret != PSRETURN_OK)
	{
		LOGERROR("Failed to load file '%s': %s", path.string8(), GetErrorString(ret));
		return std::string();
	}

	return file.DecodeUTF8(); // assume it's UTF-8
}

std::string CSimulation2::GetPlayerDefaults()
{
	return ReadJSON(L"simulation/data/settings/player_defaults.json");
}

std::string CSimulation2::GetMapSizes()
{
	return ReadJSON(L"simulation/data/settings/map_sizes.json");
}

std::string CSimulation2::GetAIData()
{
	const ScriptInterface& scriptInterface = GetScriptInterface();
	ScriptRequest rq(scriptInterface);
	JS::RootedValue aiData(rq.cx, ICmpAIManager::GetAIs(scriptInterface));

	// Build single JSON string with array of AI data
	JS::RootedValue ais(rq.cx);

	if (!Script::CreateObject(rq, &ais, "AIData", aiData))
		return std::string();

	return Script::StringifyJSON(rq, &ais);
}
