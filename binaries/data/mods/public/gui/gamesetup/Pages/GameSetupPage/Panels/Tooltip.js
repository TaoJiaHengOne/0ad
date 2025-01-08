class Tooltip
{
	constructor()
	{
		this.onScreenTooltip = Engine.GetGUIObjectByName("onscreenToolTip");
		this.gameSettingWarning = Engine.GetGUIObjectByName("gameSettingWarning");
		this.bottomRightPanel = Engine.GetGUIObjectByName("bottomRightPanel");

		g_GameSettings.cheats.watch(() => this.onSettingsChange(), ["enabled"]);
		g_GameSettings.rating.watch(() => this.onSettingsChange(), ["enabled"]);
		this.onSettingsChange();
	}

	onSettingsChange()
	{
		const marginRight = 8;
		const neighborElement = !g_IsNetworked ? this.bottomRightPanel : this.gameSettingWarning.parent;

		this.onScreenTooltip.parent.size = new GUISize(
			this.onScreenTooltip.parent.size.left,
			this.onScreenTooltip.parent.size.top,
			neighborElement.size.left - marginRight,
			this.onScreenTooltip.parent.size.bottom,
			this.onScreenTooltip.size.rleft,
			this.onScreenTooltip.parent.size.rtop,
			neighborElement.size.rleft,
			this.onScreenTooltip.parent.size.rbottom
		)
	}
}
