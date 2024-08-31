class SavedGameLabel
{
	constructor(isSavedGame)
	{
		if (isSavedGame)
			Engine.GetGUIObjectByName("savedGameLabel").hidden = false;
	}
}
