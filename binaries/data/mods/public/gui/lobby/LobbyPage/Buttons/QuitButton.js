/**
 * This class manages the button that allows the player to close the lobby page.
 */
class QuitButton
{
	constructor(closePageCallback, dialog, leaderboardPage, profilePage)
	{
		this.closePageCallback = closePageCallback;
		const closeDialog = this.closeDialog.bind(this);
		const returnToMainMenu = this.returnToMainMenu.bind(this);
		const onPress = dialog ? closeDialog : returnToMainMenu;

		const leaveButton = Engine.GetGUIObjectByName("leaveButton");
		leaveButton.onPress = onPress;
		leaveButton.caption = dialog ?
			translateWithContext("previous page", "Back") :
			translateWithContext("previous page", "Main Menu");

		if (dialog)
		{
			Engine.SetGlobalHotkey("lobby", "Press", onPress);
			Engine.SetGlobalHotkey("cancel", "Press", onPress);

			const cancelHotkey = Engine.SetGlobalHotkey.bind(Engine, "cancel", "Press", onPress);
			leaderboardPage.registerClosePageHandler(cancelHotkey);
			profilePage.registerClosePageHandler(cancelHotkey);
		}
	}

	closeDialog()
	{
		Engine.LobbySetPlayerPresence("playing");
		this.closePageCallback();
	}

	returnToMainMenu()
	{
		Engine.StopXmppClient();
		Engine.SwitchGuiPage("page_pregame.xml");
	}
}
