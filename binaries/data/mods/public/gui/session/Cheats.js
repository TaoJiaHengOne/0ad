/**
 * The purpose of this class is to run a given user input against the cheats database on request
 * and perform that cheat if an according cheat was found.
 */
class Cheats
{
	constructor()
	{
		this.cheats = {};
		for (const fileName of Engine.ListDirectoryFiles(this.Directory, "*.json", false))
		{
			const cheat = Engine.ReadJSONFile(fileName);
			if (this.cheats[cheat.Name])
				warn("Cheat name '" + cheat.Name + "' is already present");
			else
				this.cheats[cheat.Name] = cheat.Data;
		}
	}

	/**
	 * Reads userinput from the chat and sends a simulation command in case it is a known cheat.
	 * @returns {boolean} - True if a cheat was executed.
	 */
	executeCheat(text)
	{
		const player = Engine.GetPlayerID();
		if (!controlsPlayer(player) ||
		    !g_Players[player].cheatsEnabled)
			return false;

		// Find the cheat code that is a prefix of the user input
		const cheatCode = Object.keys(this.cheats).find(code => text.indexOf(code) == 0);
		if (!cheatCode)
			return false;

		const cheat = this.cheats[cheatCode];

		let parameter = text.substr(cheatCode.length + 1);

		if (cheat.DefaultParameter && parameter === "")
			parameter = cheat.DefaultParameter;

		if (parameter !== "" && cheat.isNumeric)
		{
			parameter = +parameter;
			if (isNaN(parameter))
			{
				addChatMessage({
					"type": "message",
					"guid": -1,
					"player": player,
					"text": this.NoNumericMessage,
					"translate": true,
					"translateParameters": [],
					"parameters": {}
				});
				return false;
			}
		}

		Engine.PostNetworkCommand({
			"type": "cheat",
			"action": cheat.Action,
			"text": cheat.Type,
			"player": Engine.GetPlayerID(),
			"parameter": parameter,
			"templates": cheat.Templates,
			"selected": g_Selection.toList()
		});

		return true;
	}
}

Cheats.prototype.Directory = "simulation/data/cheats/";

Cheats.prototype.NoNumericMessage =
	markForTranslation("This cheat requires a numeric parameter.");
