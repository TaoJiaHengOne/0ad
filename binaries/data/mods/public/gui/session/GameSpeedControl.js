/**
 * This class controls the game speed.
 * The control is only available in single-player and replay mode.
 * Fast forwarding is enabled if and only if there is no human player assigned.
 */
class GameSpeedControl
{
	constructor(playerViewControl)
	{
		this.gameSpeed = Engine.GetGUIObjectByName("gameSpeed");
		this.gameSpeed.onSelectionChange = this.onSelectionChange.bind(this);

		registerPlayersInitHandler(this.rebuild.bind(this));
		registerPlayersFinishedHandler(this.rebuild.bind(this));
		playerViewControl.registerPlayerIDChangeHandler(this.rebuild.bind(this));
	}

	rebuild()
	{
		const player = g_Players[Engine.GetPlayerID()];

		const gameSpeeds = prepareForDropdown(g_Settings.GameSpeeds.filter(speed =>
			!speed.FastForward || !player || player.state != "active"));

		this.gameSpeed.list = gameSpeeds.Title;
		this.gameSpeed.list_data = gameSpeeds.Speed;

		const simRate = Engine.GetSimRate();

		// If the game speed is something like 0.100001 from the game setup, set it to 0.1
		const gameSpeedIdx = gameSpeeds.Speed.indexOf(+simRate.toFixed(2));
		this.gameSpeed.selected = gameSpeedIdx != -1 ? gameSpeedIdx : gameSpeeds.Default;
	}

	toggle()
	{
		this.gameSpeed.hidden = !this.gameSpeed.hidden;
	}

	onSelectionChange()
	{
		if (!g_IsNetworked)
			Engine.SetSimRate(+this.gameSpeed.list_data[this.gameSpeed.selected]);
	}
}
