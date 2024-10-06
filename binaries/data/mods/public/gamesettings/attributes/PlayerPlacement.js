GameSettings.prototype.Attributes.PlayerPlacement = class PlayerPlacement extends GameSetting
{
	init()
	{
		this.available = undefined;
		this.value = undefined;
		this.settings.map.watch(() => this.onMapChange(), ["map"]);
	}

	toInitAttributes(attribs)
	{
		if (this.value !== undefined)
			attribs.settings.PlayerPlacement = this.value;
	}

	fromInitAttributes(attribs)
	{
		if (!!this.getLegacySetting(attribs, "PlayerPlacement"))
			this.value = this.getLegacySetting(attribs, "PlayerPlacement");
	}

	onMapChange()
	{
		if (!this.getMapSetting("PlayerPlacements"))
		{
			this.value = undefined;
			this.available = undefined;
			return;
		}
		// TODO: should probably validate that they fit one of the known schemes.
		this.available = this.getMapSetting("PlayerPlacements");
		this.value = "random";
	}

	setValue(val)
	{
		this.value = val;
	}

	pickRandomItems()
	{
		// If the map is random, we need to wait until it is selected.
		if (this.settings.map.map === "random" || this.value !== "random")
			return false;

		this.value = pickRandom(this.available);
		return true;
	}
};


GameSettings.prototype.Attributes.PlayerPlacement.prototype.StartingPositions = [
	{
		"Id": "radial",
		"Name": translateWithContext("player placement", "Circle"),
		"Description": translate("Allied players are grouped and placed with opposing players on one circle spanning the map.")
	},
	{
		"Id": "line",
		"Name": translateWithContext("player placement", "Line"),
		"Description": translate("Allied players are placed in a linear pattern."),
	},
	{
		"Id": "randomGroup",
		"Name": translateWithContext("player placement", "Random Group"),
		"Description": translate("Allied players are grouped, but otherwise placed randomly on the map."),
	},
	{
		"Id": "stronghold",
		"Name": translateWithContext("player placement", "Stronghold"),
		"Description": translate("Allied players are grouped in one random place of the map."),
	}
];
