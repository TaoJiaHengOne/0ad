/**
 * Called when the map has been loaded, but before the simulation has started.
 * Only called when a new game is started, not when loading a saved game.
 */
function PreInitGame()
{
	// We need to replace skirmish "default" entities with real ones.
	// This needs to happen before AI initialization (in InitGame).
	// And we need to flush destroyed entities otherwise the AI gets the wrong game state in
	// the beginning and a bunch of "destroy" messages on turn 0, which just shouldn't happen.
	Engine.BroadcastMessage(MT_SkirmishReplace, {});
	Engine.FlushDestroyedEntities();

	const numPlayers = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetNumPlayers();
	for (let i = 1; i < numPlayers; ++i) // ignore gaia
	{
		const cmpTechnologyManager = QueryPlayerIDInterface(i, IID_TechnologyManager);
		if (cmpTechnologyManager)
			cmpTechnologyManager.UpdateAutoResearch();
	}

	// Explore the map inside the players' territory borders
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	cmpRangeManager.ExploreTerritories();
}

function InitGame(settings)
{
	// No settings when loading a map in Atlas, so do nothing
	if (!settings)
	{
		// Map dependent initialisations of components (i.e. garrisoned units)
		Engine.BroadcastMessage(MT_InitGame, {});
		return;
	}

	if (settings.ExploreMap)
	{
		const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
		for (let i = 1; i < settings.PlayerData.length; ++i)
			cmpRangeManager.ExploreMap(i);
	}

	const cmpAIManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_AIManager);
	for (let i = 0; i < settings.PlayerData.length; ++i)
	{
		const cmpPlayer = QueryPlayerIDInterface(i);
		cmpPlayer.SetCheatsEnabled(!!settings.CheatsEnabled);

		if (settings.PlayerData[i])
		{
			if(settings.PlayerData[i].Removed)
			{
				cmpPlayer.Defeat(undefined);
				continue;
			}
			else if (settings.PlayerData[i].AI)
			{
				cmpAIManager.AddPlayer(settings.PlayerData[i].AI, i, +settings.PlayerData[i].AIDiff, settings.PlayerData[i].AIBehavior || "random");
				cmpPlayer.SetAI(true);
			}
		}

		if (settings.AllyView)
			Engine.QueryInterface(cmpPlayer.entity, IID_TechnologyManager)?.ResearchTechnology(Engine.QueryInterface(cmpPlayer.entity, IID_Diplomacy).template.SharedLosTech);
	}

	const cmpPopulationCapManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PopulationCapManager);
	cmpPopulationCapManager.SetPopulationCapType(settings.PopulationCapType);
	cmpPopulationCapManager.SetPopulationCap(settings.PopulationCap);

	// Update the grid with all entities created for the map init.
	Engine.QueryInterface(SYSTEM_ENTITY, IID_Pathfinder).UpdateGrid();

	// Map or player data (handicap...) dependent initialisations of components (i.e. garrisoned units).
	Engine.BroadcastMessage(MT_InitGame, {});

	cmpAIManager.TryLoadSharedComponent();
	cmpAIManager.RunGamestateInit();
}

Engine.RegisterGlobal("PreInitGame", PreInitGame);
Engine.RegisterGlobal("InitGame", InitGame);
