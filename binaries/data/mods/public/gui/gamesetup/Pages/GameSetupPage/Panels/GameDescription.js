class GameDescription
{
	constructor(setupWindow, gameSettingTabs)
	{
		this.mapCache = setupWindow.controls.mapCache;

		this.gameDescription = Engine.GetGUIObjectByName("gameDescription");

		gameSettingTabs.registerTabsResizeHandler(this.onTabsResize.bind(this));
		this.registerWatchers();
		this.updateGameDescription();
	}

	registerWatchers()
	{
		const update = () => this.updateGameDescription();
		g_GameSettings.biome.watch(update, ["biome"]);
		g_GameSettings.ceasefire.watch(update, ["value"]);
		g_GameSettings.cheats.watch(update, ["enabled"]);
		g_GameSettings.disableTreasures.watch(update, ["enabled"]);
		g_GameSettings.lastManStanding.watch(update, ["enabled"]);
		g_GameSettings.lockedTeams.watch(update, ["enabled"]);
		g_GameSettings.map.watch(update, ["map", "type"]);
		g_GameSettings.mapExploration.watch(update, ["explored"]);
		g_GameSettings.mapExploration.watch(update, ["revealed"]);
		g_GameSettings.mapExploration.watch(update, ["allied"]);
		g_GameSettings.nomad.watch(update, ["enabled"]);
		g_GameSettings.population.watch(update, ["perPlayer", "cap", "capType"]);
		g_GameSettings.playerPlacement.watch(update, ["value"]);
		g_GameSettings.rating.watch(update, ["enabled"]);
		g_GameSettings.regicideGarrison.watch(update, ["enabled"]);
		g_GameSettings.relic.watch(update, ["count", "duration"]);
		g_GameSettings.startingResources.watch(update, ["perPlayer", "resources"]);
		g_GameSettings.triggerDifficulty.watch(update, ["value"]);
		g_GameSettings.victoryConditions.watch(update, ["active"]);
		g_GameSettings.wonder.watch(update, ["duration"]);
		g_GameSettings.mapSize.watch(update, ["size"]);
	}

	onTabsResize(settingsTabButtonsFrame)
	{
		this.gameDescription.size.top = settingsTabButtonsFrame.size.bottom + this.Margin;
	}

	updateGameDescription()
	{
		if (this.timer)
			clearTimeout(this.timer);
		// Update the description on the next GUI tick.
		// (multiple settings can change at once)
		const updateCaption = () => { this.gameDescription.caption = getGameDescription(g_GameSettings.toInitAttributes(), this.mapCache); };
		this.timer = setTimeout(updateCaption, 0);
	}
}

GameDescription.prototype.Margin = 3;
