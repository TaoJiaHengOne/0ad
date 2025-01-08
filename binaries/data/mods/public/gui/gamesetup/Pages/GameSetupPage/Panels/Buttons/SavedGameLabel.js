class SavedGameLabel
{
	constructor(isSavedGame)
	{
		const maxWidth = 140;
		const marginRight = 8;

		const bottomRightPanel = Engine.GetGUIObjectByName("bottomRightPanel");
		const savedGameLabel = Engine.GetGUIObjectByName("savedGameLabel");
		let labelWidth = Math.min(Engine.GetTextWidth(savedGameLabel.font, savedGameLabel.caption) + 10, maxWidth);

		if (isSavedGame) {
			savedGameLabel.parent.size = new GUISize(
				bottomRightPanel.size.left - labelWidth - marginRight,
				savedGameLabel.parent.size.top,
				bottomRightPanel.size.left - marginRight,
				savedGameLabel.parent.size.bottom,
				bottomRightPanel.size.rleft,
				savedGameLabel.parent.size.rtop,
				bottomRightPanel.size.rleft,
				savedGameLabel.parent.size.rbottom
			)
			savedGameLabel.hidden = false;
		}
	}
}
