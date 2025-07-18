/**
 * System component to store the victory conditions and their settings and
 * check for allied victory / last-man-standing.
 */
function EndGameManager() {}

EndGameManager.prototype.Schema =
	"<a:component type='system'/><empty/>";

EndGameManager.prototype.Init = function()
{
	// Contains settings specific to the victory condition,
	// for example wonder victory duration.
	this.gameSettings = {};

	// Allied victory means allied players can win if victory conditions are met for each of them
	// False for a "last man standing" game
	this.alliedVictory = true;

	// Don't do any checks before the diplomacies were set for each player
	// or when marking a player as won.
	this.skipAlliedVictoryCheck = true;

	this.lastManStandingMessage = undefined;

	this.endlessGame = false;
};

EndGameManager.prototype.GetGameSettings = function()
{
	return this.gameSettings;
};

EndGameManager.prototype.GetVictoryConditions = function()
{
	return this.gameSettings.victoryConditions;
};

EndGameManager.prototype.SetGameSettings = function(newSettings = {})
{
	this.gameSettings = newSettings;
	this.skipAlliedVictoryCheck = false;
	this.endlessGame = !this.gameSettings.victoryConditions.length;

	Engine.BroadcastMessage(MT_VictoryConditionsChanged, {});
};

/**
 * Sets the given player (and the allies if allied victory is enabled) as a winner.
 *
 * @param {number} playerID - The player that should win.
 * @param {function} victoryReason - Function that maps from number to plural string, for example
 *   n => markForPluralTranslation(
 *       "%(lastPlayer)s has won (game mode).",
 *       "%(players)s and %(lastPlayer)s have won (game mode).",
 *       n));
 */
EndGameManager.prototype.MarkPlayerAndAlliesAsWon = function(playerID, victoryString, defeatString)
{
	const cmpPlayer = QueryPlayerIDInterface(playerID);
	if (!cmpPlayer.IsActive())
	{
		warn("Can't mark player " + playerID + " as won, since the state is " + cmpPlayer.GetState());
		return;
	}

	let winningPlayers = [playerID];
	if (this.alliedVictory)
		winningPlayers = QueryPlayerIDInterface(playerID, IID_Diplomacy).GetMutualAllies(playerID).filter(
			player => QueryPlayerIDInterface(player).IsActive());

	this.MarkPlayersAsWon(winningPlayers, victoryString, defeatString);
};

/**
 * Sets the given players as won and others as defeated.
 *
 * @param {array} winningPlayers - The players that should win.
 * @param {function} victoryReason - Function that maps from number to plural string, for example
 *   n => markForPluralTranslation(
 *       "%(lastPlayer)s has won (game mode).",
 *       "%(players)s and %(lastPlayer)s have won (game mode).",
 *       n));
 */
EndGameManager.prototype.MarkPlayersAsWon = function(winningPlayers, victoryString, defeatString)
{
	this.skipAlliedVictoryCheck = true;
	for (const playerID of winningPlayers)
	{
		const cmpPlayer = QueryPlayerIDInterface(playerID);
		if (!cmpPlayer.IsActive())
		{
			warn("Can't mark player " + playerID + " as won, since the state is " + cmpPlayer.GetState());
			continue;
		}
		cmpPlayer.Win(undefined);
	}

	const defeatedPlayers = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetActivePlayers().filter(
		playerID => winningPlayers.indexOf(playerID) == -1);

	for (const playerID of defeatedPlayers)
		QueryPlayerIDInterface(playerID).Defeat(undefined);

	const cmpGUIInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
	cmpGUIInterface.PushNotification({
		"type": "won",
		"players": [winningPlayers[0]],
		"allies": winningPlayers,
		"message": victoryString(winningPlayers.length)
	});

	if (defeatedPlayers.length)
		cmpGUIInterface.PushNotification({
			"type": "defeat",
			"players": [defeatedPlayers[0]],
			"allies": defeatedPlayers,
			"message": defeatString(defeatedPlayers.length)
		});

	this.skipAlliedVictoryCheck = false;
};

EndGameManager.prototype.SetAlliedVictory = function(flag)
{
	this.alliedVictory = flag;
};

EndGameManager.prototype.GetAlliedVictory = function()
{
	return this.alliedVictory;
};

EndGameManager.prototype.AlliedVictoryCheck = function()
{
	if (this.skipAlliedVictoryCheck || this.endlessGame)
		return;

	const cmpGuiInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
	cmpGuiInterface.DeleteTimeNotification(this.lastManStandingMessage);

	// Proceed if only allies are remaining
	const allies = [];
	const numPlayers = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetNumPlayers();
	for (let playerID = 1; playerID < numPlayers; ++playerID)
	{
		const cmpPlayer = QueryPlayerIDInterface(playerID);
		if (!cmpPlayer.IsActive())
			continue;

		if (allies.length && !QueryPlayerIDInterface(playerID, IID_Diplomacy).IsMutualAlly(allies[0]))
			return;

		allies.push(playerID);
	}

	if (!allies.length)
		return;

	if (this.alliedVictory || allies.length == 1)
	{
		for (const playerID of allies)
			QueryPlayerIDInterface(playerID)?.Win(undefined);

		cmpGuiInterface.PushNotification({
			"type": "won",
			"players": [allies[0]],
			"allies": allies,
			"message": markForPluralTranslation(
				"%(lastPlayer)s has won (last player alive).",
				"%(players)s and %(lastPlayer)s have won (last players alive).",
				allies.length)
		});
	}
	else
		this.lastManStandingMessage = cmpGuiInterface.AddTimeNotification({
			"message": markForTranslation("Last remaining player wins."),
			"translateMessage": true,
		}, 12 * 60 * 60 * 1000); // 12 hours
};

EndGameManager.prototype.OnInitGame = function(msg)
{
	this.AlliedVictoryCheck();
};

EndGameManager.prototype.OnGlobalDiplomacyChanged = function(msg)
{
	this.AlliedVictoryCheck();
};

EndGameManager.prototype.OnGlobalPlayerDefeated = function(msg)
{
	this.AlliedVictoryCheck();
};

Engine.RegisterSystemComponentType(IID_EndGameManager, "EndGameManager", EndGameManager);
