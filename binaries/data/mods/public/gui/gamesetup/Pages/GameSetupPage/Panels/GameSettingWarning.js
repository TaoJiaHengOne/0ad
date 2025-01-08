class GameSettingWarning
{
	constructor(setupWindow, cancelButton)
	{
		if (!g_IsNetworked)
			return;

		this.bottomRightPanel = Engine.GetGUIObjectByName("bottomRightPanel");
		this.gameSettingWarning = Engine.GetGUIObjectByName("gameSettingWarning");
		this.savedGameLabel = Engine.GetGUIObjectByName("savedGameLabel");

		g_GameSettings.cheats.watch(() => this.onSettingsChange(), ["enabled"]);
		g_GameSettings.rating.watch(() => this.onSettingsChange(), ["enabled"]);
		this.onSettingsChange();
	}

	onSettingsChange()
	{
		const maxWidth = this.savedGameLabel.hidden ? 260 : 180;
		const marginRight = 8;

		let caption =
			g_GameSettings.cheats.enabled ?
				this.CheatsEnabled :
				g_GameSettings.rating.enabled ?
					this.RatingEnabled :
					"";

		this.gameSettingWarning.caption = caption;

		let labelWidth = Math.min(Engine.GetTextWidth(this.gameSettingWarning.font, this.gameSettingWarning.caption) + 10, maxWidth);

		const neighborElement = !this.savedGameLabel.hidden ? this.savedGameLabel.parent : this.bottomRightPanel;
		this.gameSettingWarning.parent.size = new GUISize(
			neighborElement.size.left - labelWidth - marginRight,
			this.gameSettingWarning.parent.size.top,
			neighborElement.size.left - marginRight,
			this.gameSettingWarning.parent.size.bottom,
			neighborElement.size.rleft,
			this.gameSettingWarning.parent.size.rtop,
			neighborElement.size.rright,
			this.gameSettingWarning.parent.size.rbottom
		)
		this.gameSettingWarning.hidden = !caption;
	}
}

GameSettingWarning.prototype.CheatsEnabled =
	translate("Cheats enabled.");

GameSettingWarning.prototype.RatingEnabled =
	translate("Rated game.");
