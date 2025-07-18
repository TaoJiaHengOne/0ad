/**
 * All tutorial messages received so far.
 */
var g_TutorialMessages = [];

/**
 * GUI tags applied to the most recent tutorial message.
 */
var g_TutorialNewMessageTags = { "color": "255 226 149" };

/**
 * The number of seconds we monitor to rate limit flares.
 */
var g_FlareRateLimitScope = 6;

/**
 * The maximum allowed number of flares within g_FlareRateLimitScope seconds.
 * This should be a bit larger than the number of flares that can be sent in theory by using the GUI.
 */
var g_FlareRateLimitMaximumFlares = 16;

/**
 * Contains the arrival timestamps the flares of the last g_FlareRateLimitScope seconds.
 */
var g_FlareRateLimitLastTimes = [];

/**
 * These handlers are called everytime a client joins or disconnects.
 */
var g_PlayerAssignmentsChangeHandlers = new Set();

/**
 * These handlers are called when the ceasefire time has run out.
 */
var g_CeasefireEndedHandlers = new Set();

/**
 * These handlers are fired when the match is networked and
 * the current client established the connection, authenticated,
 * finished the loading screen, starts or finished synchronizing after a rejoin.
 * The messages are constructed in NetClient.cpp.
 */
var g_NetworkStatusChangeHandlers = new Set();

/**
 * These handlers are triggered whenever a client finishes the loading screen.
 */
var g_ClientsLoadingHandlers = new Set();

/**
 * These handlers are fired if the server informed the players that the networked game is out of sync.
 */
var g_NetworkOutOfSyncHandlers = new Set();

/**
 * Handle all netmessage types that can occur.
 */
var g_NetMessageTypes = {
	"netstatus": msg => {
		handleNetStatusMessage(msg);
	},
	"netwarn": msg => {
		addNetworkWarning(msg);
	},
	"out-of-sync": msg => {
		for (const handler of g_NetworkOutOfSyncHandlers)
			handler(msg);
	},
	"players": msg => {
		handlePlayerAssignmentsMessage(msg);
	},
	"paused": msg => {
		g_PauseControl.setClientPauseState(msg.guid, msg.pause);
	},
	"clients-loading": msg => {
		for (const handler of g_ClientsLoadingHandlers)
			handler(msg.guids);
	},
	"rejoined": msg => {
		addChatMessage({
			"type": "rejoined",
			"guid": msg.guid
		});
	},
	"kicked": msg => {
		addChatMessage({
			"type": "kicked",
			"username": msg.username,
			"banned": msg.banned
		});
	},
	"chat": msg => {
		addChatMessage({
			"type": "message",
			"guid": msg.guid,
			"text": msg.text
		});
	},
	"flare": msg => {
		handleFlare(msg);
	},
	"gamesetup": msg => {}, // Needed for autostart
	"start": msg => {}
};

var g_PlayerStateMessages = {
	"won": translate("You have won!"),
	"defeated": translate("You have been defeated!")
};

var g_LastAttack;

/**
 * Defines how the GUI reacts to notifications that are sent by the simulation.
 * Don't open new pages (message boxes) here! Otherwise further notifications
 * handled in the same turn can't access the GUI objects anymore.
 */
var g_NotificationsTypes =
{
	"aichat": function(notification, player)
	{
		const message = {
			"type": "message",
			"text": notification.message,
			"guid": findGuidForPlayerID(player) || -1,
			"player": player,
			"translate": true
		};

		if (notification.translateParameters)
		{
			message.translateParameters = notification.translateParameters;
			message.parameters = notification.parameters;
			colorizePlayernameParameters(notification.parameters);
		}

		addChatMessage(message);
	},
	"defeat": function(notification, player)
	{
		playersFinished(notification.allies, notification.message, false);
	},
	"won": function(notification, player)
	{
		playersFinished(notification.allies, notification.message, true);
	},
	"diplomacy": function(notification, player)
	{
		updatePlayerData();
		g_DiplomacyColors.onDiplomacyChange();

		addChatMessage({
			"type": "diplomacy",
			"sourcePlayer": player,
			"targetPlayer": notification.targetPlayer,
			"status": notification.status
		});
	},
	"ceasefire-ended": function(notification, player)
	{
		updatePlayerData();
		for (const handler of g_CeasefireEndedHandlers)
			handler();
	},
	"tutorial": function(notification, player)
	{
		updateTutorial(notification);
	},
	"tribute": function(notification, player)
	{
		addChatMessage({
			"type": "tribute",
			"sourcePlayer": notification.donator,
			"targetPlayer": player,
			"amounts": notification.amounts
		});
	},
	"barter": function(notification, player)
	{
		addChatMessage({
			"type": "barter",
			"player": player,
			"amountGiven": notification.amountGiven,
			"amountGained": notification.amountGained,
			"resourceGiven": notification.resourceGiven,
			"resourceGained": notification.resourceGained
		});
	},
	"spy-response": function(notification, player)
	{
		g_DiplomacyDialog.onSpyResponse(notification, player);

		if (notification.entity && g_ViewedPlayer == player && (!g_IsObserver || g_FollowPlayer))
		{
			g_DiplomacyDialog.close();
			setCameraFollow(notification.entity);
		}
	},
	"attack": function(notification, player)
	{
		if (player != g_ViewedPlayer)
			return;

		// Focus camera on attacks
		if (g_FollowPlayer)
		{
			setCameraFollow(notification.target);

			g_Selection.reset();
			if (notification.target)
				g_Selection.addList([notification.target]);
		}

		g_LastAttack = { "target": notification.target, "position": notification.position };

		if (Engine.ConfigDB_GetValue("user", "gui.session.notifications.attack") !== "true")
			return;

		addChatMessage({
			"type": "attack",
			"player": player,
			"attacker": notification.attacker,
			"target": notification.target,
			"position": notification.position,
			"targetIsDomesticAnimal": notification.targetIsDomesticAnimal
		});
	},
	"phase": function(notification, player)
	{
		addChatMessage({
			"type": "phase",
			"player": player,
			"phaseName": notification.phaseName,
			"phaseState": notification.phaseState
		});
	},
	"dialog": function(notification, player)
	{
		if (player == Engine.GetPlayerID())
			openDialog(notification.dialogName, notification.data, player);
	},
	"playercommand": function(notification, player)
	{
		// For observers, focus the camera on units commanded by the selected player
		if (!g_FollowPlayer || player != g_ViewedPlayer)
			return;

		const cmd = notification.cmd;

		// Ignore rallypoint commands of trained animals
		const entState = cmd.entities && cmd.entities[0] && GetEntityState(cmd.entities[0]);
		if (g_ViewedPlayer != 0 &&
		    entState && entState.identity && entState.identity.classes &&
		    entState.identity.classes.indexOf("Animal") != -1)
			return;

		// Focus the structure to build.
		if (cmd.type == "repair")
		{
			const targetState = GetEntityState(cmd.target);
			if (targetState)
				Engine.CameraMoveTo(targetState.position.x, targetState.position.z);
		}
		else if (cmd.type == "delete-entities" && notification.position)
			Engine.CameraMoveTo(notification.position.x, notification.position.y);
		// Focus commanded entities, but don't lose previous focus when training units
		else if (cmd.type != "train" && cmd.type != "research" && entState)
			setCameraFollow(cmd.entities[0]);

		if (["walk", "attack-walk", "patrol"].indexOf(cmd.type) != -1)
			DrawTargetMarker(cmd);

		// Select units affected by that command
		let selection = [];
		if (cmd.entities)
			selection = cmd.entities;
		if (cmd.target)
			selection.push(cmd.target);

		// Allow gaia in selection when gathering
		g_Selection.reset();
		g_Selection.addList(selection, false, cmd.type == "gather");
	},
	"play-tracks": function(notification, player)
	{
		if (notification.lock)
		{
			global.music.storeTracks(notification.tracks.map(track => ({ "Type": "custom", "File": track })));
			global.music.setState(global.music.states.CUSTOM);
		}

		global.music.setLocked(notification.lock);
	}
};

function registerPlayerAssignmentsChangeHandler(handler)
{
	g_PlayerAssignmentsChangeHandlers.add(handler);
}

function registerCeasefireEndedHandler(handler)
{
	g_CeasefireEndedHandlers.add(handler);
}

function registerNetworkOutOfSyncHandler(handler)
{
	g_NetworkOutOfSyncHandlers.add(handler);
}

function registerNetworkStatusChangeHandler(handler)
{
	g_NetworkStatusChangeHandlers.add(handler);
}

function registerClientsLoadingHandler(handler)
{
	g_ClientsLoadingHandlers.add(handler);
}

function findGuidForPlayerID(playerID)
{
	return Object.keys(g_PlayerAssignments).find(guid => g_PlayerAssignments[guid].player == playerID);
}

/**
 * Processes all pending notifications sent from the GUIInterface simulation component.
 */
function handleNotifications()
{
	for (const notification of Engine.GuiInterfaceCall("GetNotifications"))
	{
		if (!notification.players || !notification.type || !g_NotificationsTypes[notification.type])
		{
			error("Invalid GUI notification: " + uneval(notification));
			continue;
		}

		for (const player of notification.players)
			g_NotificationsTypes[notification.type](notification, player);
	}
}

function focusAttack(attack)
{
	if (!attack)
		return;

	const entState = attack.target && GetEntityState(attack.target);
	if (entState && hasClass(entState, "Unit"))
		setCameraFollow(attack.target);
	else
	{
		const position = attack.position;
		Engine.SetCameraTarget(position.x, position.y, position.z);
	}
}

function toggleTutorial()
{
	const tutorialPanel = Engine.GetGUIObjectByName("tutorialPanel");
	tutorialPanel.hidden = !tutorialPanel.hidden || !Engine.GetGUIObjectByName("tutorialText").caption;
}

/**
 * Updates the tutorial panel when a new goal.
 */
function updateTutorial(notification)
{
	// Show the tutorial panel if not yet done
	Engine.GetGUIObjectByName("tutorialPanel").hidden = false;

	if (notification.warning)
	{
		Engine.GetGUIObjectByName("tutorialWarning").caption = coloredText(translate(notification.warning), "orange");
		return;
	}

	const notificationText =
		notification.instructions.reduce((instructions, item) =>
			instructions + (typeof item == "string" ? translate(item) : colorizeHotkey(translate(item.text), item.hotkey)),
		"");

	Engine.GetGUIObjectByName("tutorialText").caption = g_TutorialMessages.concat(setStringTags(notificationText, g_TutorialNewMessageTags)).join("\n");
	g_TutorialMessages.push(notificationText);

	if (notification.readyButton)
	{
		Engine.GetGUIObjectByName("tutorialReady").hidden = false;
		if (notification.leave)
		{
			Engine.GetGUIObjectByName("tutorialWarning").caption = translate("Click to quit this tutorial.");
			Engine.GetGUIObjectByName("tutorialReady").caption = translate("Quit");
			Engine.GetGUIObjectByName("tutorialReady").onPress = () => { endGame(true); };
		}
		else
			Engine.GetGUIObjectByName("tutorialWarning").caption = translate("Click when ready.");
	}
	else
	{
		Engine.GetGUIObjectByName("tutorialWarning").caption = translate("Follow the instructions.");
		Engine.GetGUIObjectByName("tutorialReady").hidden = true;
	}
}

/**
 * Process every CNetMessage (see NetMessage.h, NetMessages.h) sent by the CNetServer.
 * Saves the received object to mainlog.html.
 */
function handleNetMessages()
{
	while (true)
	{
		const msg = Engine.PollNetworkClient();
		if (!msg)
			return;

		log("Net message: " + uneval(msg));

		if (g_NetMessageTypes[msg.type])
			g_NetMessageTypes[msg.type](msg);
		else
			error("Unrecognized net message type '" + msg.type + "'");
	}
}

function handleNetStatusMessage(message)
{
	if (g_Disconnected)
		return;

	g_IsNetworkedActive = message.status == "active";

	if (message.status == "disconnected")
	{
		g_Disconnected = true;
		updateCinemaPath();
		closeOpenDialogs();
	}

	for (const handler of g_NetworkStatusChangeHandlers)
		handler(message);
}

function handlePlayerAssignmentsMessage(message)
{
	for (const guid in g_PlayerAssignments)
		if (!message.newAssignments[guid])
			onClientLeave(guid);

	const joins = Object.keys(message.newAssignments).filter(guid => !g_PlayerAssignments[guid]);

	g_PlayerAssignments = message.newAssignments;

	joins.forEach(guid => {
		onClientJoin(guid);
	});

	for (const handler of g_PlayerAssignmentsChangeHandlers)
		handler();

	// TODO: use subscription instead
	updateGUIObjects();
}

function onClientJoin(guid)
{
	const playerID = g_PlayerAssignments[guid].player;

	if (g_Players[playerID])
	{
		g_Players[playerID].guid = guid;
		g_Players[playerID].name = g_PlayerAssignments[guid].name;
		g_Players[playerID].offline = false;
	}

	addChatMessage({
		"type": "connect",
		"guid": guid
	});
}

function onClientLeave(guid)
{
	g_PauseControl.setClientPauseState(guid, false);

	for (const id in g_Players)
		if (g_Players[id].guid == guid)
			g_Players[id].offline = true;

	addChatMessage({
		"type": "disconnect",
		"guid": guid
	});
}

function addChatMessage(msg)
{
	g_Chat.ChatMessageHandler.handleMessage(msg);
}

function clearChatMessages()
{
	g_Chat.ChatOverlay.clearChatMessages();
}

function handleFlare(data)
{
	const playerID = g_PlayerAssignments[data.guid].player;
	const shouldSeeFlare = g_IsObserver || g_Players[playerID]?.isMutualAlly[Engine.GetPlayerID()];

	// Don't display for the player that sent the flare because they will see it immediately.
	if (!shouldSeeFlare || data.guid == Engine.GetPlayerGUID())
		return;

	const now = Date.now();
	if (g_FlareRateLimitLastTimes.length)
	{
		g_FlareRateLimitLastTimes = g_FlareRateLimitLastTimes.filter(t => now - t < g_FlareRateLimitScope * 1000);
		if (g_FlareRateLimitLastTimes.length >= g_FlareRateLimitMaximumFlares)
		{
			warn("Received too many flares. Dropping a flare request by '" + g_PlayerAssignments[data.guid].name + "'.");
			return;
		}
	}
	g_FlareRateLimitLastTimes.push(now);

	renderAndPlayFlare(data.position, data.guid);
}

/**
 * This function is used for AIs, whose names don't exist in g_PlayerAssignments.
 */
function colorizePlayernameByID(playerID)
{
	const username = g_Players[playerID] && escapeText(g_Players[playerID].name);
	return colorizePlayernameHelper(username, playerID);
}

function colorizePlayernameByGUID(guid)
{
	const username = g_PlayerAssignments[guid] ? g_PlayerAssignments[guid].name : "";
	const playerID = g_PlayerAssignments[guid] ? g_PlayerAssignments[guid].player : -1;
	return colorizePlayernameHelper(username, playerID);
}

function colorizePlayernameHelper(username, playerID)
{
	const playerColor = playerID > -1 ? g_DiplomacyColors.getPlayerColor(playerID) : "white";
	return coloredText(username || translate("Unknown Player"), playerColor);
}

/**
 * Insert the colorized playername to chat messages sent by the AI and time notifications.
 */
function colorizePlayernameParameters(parameters)
{
	for (const param in parameters)
		if (param.startsWith("_player_"))
			parameters[param] = colorizePlayernameByID(parameters[param]);
}

/**
 * Custom dialog response handling, usable by trigger maps.
 */
function sendDialogAnswer(guiObject, dialogName)
{
	Engine.GetGUIObjectByName(dialogName + "-dialog").hidden = true;

	Engine.PostNetworkCommand({
		"type": "dialog-answer",
		"dialog": dialogName,
		"answer": guiObject.name.split("-").pop(),
	});

	resumeGame();
}

/**
 * Custom dialog opening, usable by trigger maps.
 */
function openDialog(dialogName, data, player)
{
	const dialog = Engine.GetGUIObjectByName(dialogName + "-dialog");
	if (!dialog)
	{
		warn("messages.js: Unknown dialog with name " + dialogName);
		return;
	}
	dialog.hidden = false;

	for (const objName in data)
	{
		const obj = Engine.GetGUIObjectByName(dialogName + "-dialog-" + objName);
		if (!obj)
		{
			warn("messages.js: Key '" + objName + "' not found in '" + dialogName + "' dialog.");
			continue;
		}

		for (const key in data[objName])
		{
			const n = data[objName][key];
			if (typeof n == "object" && n.message)
			{
				let message = n.message;
				if (n.translateMessage)
					message = translate(message);
				const parameters = n.parameters || {};
				if (n.translateParameters)
					translateObjectKeys(parameters, n.translateParameters);
				obj[key] = sprintf(message, parameters);
			}
			else
				obj[key] = n;
		}
	}

	g_PauseControl.implicitPause();
}
