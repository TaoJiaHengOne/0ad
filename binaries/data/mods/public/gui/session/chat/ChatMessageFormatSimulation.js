/**
 * These classes construct a chat message from simulation events initiated from the GuiInterface PushNotification method.
 */
class ChatMessageFormatSimulation
{
}

ChatMessageFormatSimulation.attack = class
{
	parse(msg)
	{
		if (msg.player != g_ViewedPlayer)
			return "";

		const message = msg.targetIsDomesticAnimal ?
			translate("%(icon)sYour livestock has been attacked by %(attacker)s!") :
			translate("%(icon)sYou have been attacked by %(attacker)s!");

		return {
			"text": sprintf(message, {
				"icon": '[icon="icon_focusattacked"]',
				"attacker": colorizePlayernameByID(msg.attacker)
			}),
			"callback": ((target, position) => function() {
				focusAttack({ "target": target, "position": position });
			})(msg.target, msg.position),
			"tooltip": translate("Click to focus on the attacked unit.")
		};
	}
};

ChatMessageFormatSimulation.barter = class
{
	parse(msg)
	{
		if (!g_IsObserver || Engine.ConfigDB_GetValue("user", "gui.session.notifications.barter") != "true")
			return "";

		const amountGiven = {};
		amountGiven[msg.resourceGiven] = msg.amountGiven;

		const amountGained = {};
		amountGained[msg.resourceGained] = msg.amountGained;

		return {
			"text": sprintf(translate("%(player)s bartered %(amountGiven)s for %(amountGained)s."), {
				"player": colorizePlayernameByID(msg.player),
				"amountGiven": getLocalizedResourceAmounts(amountGiven),
				"amountGained": getLocalizedResourceAmounts(amountGained)
			})
		};
	}
};

ChatMessageFormatSimulation.diplomacy = class
{
	parse(msg)
	{
		let messageType;

		if (g_IsObserver)
			messageType = "observer";
		else if (Engine.GetPlayerID() == msg.sourcePlayer)
			messageType = "active";
		else if (Engine.GetPlayerID() == msg.targetPlayer)
			messageType = "passive";
		else
			return "";

		return {
			"text": sprintf(translate(this.strings[messageType][msg.status]), {
				"player": colorizePlayernameByID(messageType == "active" ? msg.targetPlayer : msg.sourcePlayer),
				"player2": colorizePlayernameByID(messageType == "active" ? msg.sourcePlayer : msg.targetPlayer)
			})
		};
	}
};

ChatMessageFormatSimulation.diplomacy.prototype.strings = {
	"active": {
		"ally": markForTranslation("You are now allied with %(player)s."),
		"enemy": markForTranslation("You are now at war with %(player)s."),
		"neutral": markForTranslation("You are now neutral with %(player)s.")
	},
	"passive": {
		"ally": markForTranslation("%(player)s is now allied with you."),
		"enemy": markForTranslation("%(player)s is now at war with you."),
		"neutral": markForTranslation("%(player)s is now neutral with you.")
	},
	"observer": {
		"ally": markForTranslation("%(player)s is now allied with %(player2)s."),
		"enemy": markForTranslation("%(player)s is now at war with %(player2)s."),
		"neutral": markForTranslation("%(player)s is now neutral with %(player2)s.")
	}
};

ChatMessageFormatSimulation.phase = class
{
	parse(msg)
	{
		const notifyPhase = Engine.ConfigDB_GetValue("user", "gui.session.notifications.phase");
		if (notifyPhase == "none" || msg.player != g_ViewedPlayer && !g_IsObserver && !g_Players[msg.player].isMutualAlly[g_ViewedPlayer])
			return "";

		let message = "";
		if (notifyPhase == "all")
		{
			if (msg.phaseState == "started")
				message = translate("%(player)s is advancing to the %(phaseName)s.");
			else if (msg.phaseState == "aborted")
				message = translate("The %(phaseName)s of %(player)s has been aborted.");
		}
		if (msg.phaseState == "completed")
			message = translate("%(player)s has reached the %(phaseName)s.");

		return {
			"text": sprintf(message, {
				"player": colorizePlayernameByID(msg.player),
				"phaseName": getEntityNames(GetTechnologyData(msg.phaseName, g_Players[msg.player].civ))
			})
		};
	}
};

ChatMessageFormatSimulation.playerstate = class
{
	parse(msg)
	{
		if (!msg.message.pluralMessage)
			return {
				"text": sprintf(translate(msg.message), {
					"player": colorizePlayernameByID(msg.players[0])
				})
			};

		const mPlayers = msg.players.map(playerID => colorizePlayernameByID(playerID));
		const lastPlayer = mPlayers.pop();

		return {
			"text": sprintf(translatePlural(msg.message.message, msg.message.pluralMessage, msg.message.pluralCount), {
				// Translation: This comma is used for separating first to penultimate elements in an enumeration.
				"players": mPlayers.join(translate(", ")),
				"lastPlayer": lastPlayer
			})
		};
	}
};

/**
 * Optionally show all tributes sent in observer mode and tributes sent between allied players.
 * Otherwise, only show tributes sent directly to us, and tributes that we send.
 */
ChatMessageFormatSimulation.tribute = class
{
	parse(msg)
	{
		let message = "";
		if (msg.targetPlayer == Engine.GetPlayerID())
			message = translate("%(player)s has sent you %(amounts)s.");
		else if (msg.sourcePlayer == Engine.GetPlayerID())
			message = translate("You have sent %(player2)s %(amounts)s.");
		else if (Engine.ConfigDB_GetValue("user", "gui.session.notifications.tribute") == "true" &&
		        (g_IsObserver || g_InitAttributes.settings.LockTeams &&
		           g_Players[msg.sourcePlayer].isMutualAlly[Engine.GetPlayerID()] &&
		           g_Players[msg.targetPlayer].isMutualAlly[Engine.GetPlayerID()]))
			message = translate("%(player)s has sent %(player2)s %(amounts)s.");

		return {
			"text": sprintf(message, {
				"player": colorizePlayernameByID(msg.sourcePlayer),
				"player2": colorizePlayernameByID(msg.targetPlayer),
				"amounts": getLocalizedResourceAmounts(msg.amounts)
			})
		};
	}
};

ChatMessageFormatSimulation.flare = class
{
	// We don't need to check whether the player is supposed to see the flare as this function is only ever called in that case.
	parse(msg)
	{
		switch (Engine.ConfigDB_GetValue("user", "gui.session.notifications.flare"))
		{
		case "never":
			return "";

		case "observer":
			if (!g_IsObserver)
				return "";
			break;
		default:
			break;
		}

		return {
			"text": sprintf(translate("%(icon)s%(player)s has sent a flare."), {
				"icon": "[icon=\"icon_focusflare\" displace=\"0 1\"]",
				"player": colorizePlayernameByGUID(msg.guid)
			}),
			"callback": ((position) => function() {
				Engine.CameraMoveTo(position.x, position.z);
			})(msg.position),
			"tooltip": translate("Click to focus on the flare's location.")
		};
	}
};
