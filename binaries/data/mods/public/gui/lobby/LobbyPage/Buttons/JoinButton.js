/**
 * This class manages the button that enables the player to join a lobby game hosted by a remote player.
 */
class JoinButton
{
	constructor(dialog, gameList)
	{
		this.gameList = gameList;

		this.joinButton = Engine.GetGUIObjectByName("joinButton");
		this.joinButton.caption = this.Caption;
		this.joinButton.hidden = dialog;
		if (!dialog)
			this.joinButton.onPress = this.onPress.bind(this);

		gameList.gamesBox.onMouseLeftDoubleClickItem = this.onPress.bind(this);
		gameList.registerSelectionChangeHandler(this.onSelectedGameChange.bind(this, dialog));

		this.onSelectedGameChange(dialog);
	}

	onSelectedGameChange(dialog, selectedGame)
	{
		this.joinButton.hidden = dialog || !selectedGame;
	}

	/**
	 * Immediately rejoin and join game setups. Otherwise confirm late-observer join attempt.
	 */
	async onPress()
	{
		const game = this.gameList.selectedGame();
		if (!game)
			return;

		const rating = this.getRejoinRating(game);
		const playername = rating ? g_Nickname + " (" + rating + ")" : g_Nickname;

		if (!game.isCompatible)
		{
			const buttonIndex = await messageBox(
				400, 200,
				translate("Your active mods do not match the mods of this game.") + "\n\n" +
					comparedModsString(game.mods, Engine.GetEngineInfo().mods) + "\n\n" +
					translate("Do you want to switch to the mod selection page?"),
				translate("Incompatible mods"),
				[translate("No"), translate("Yes")]);

			if (buttonIndex === 0)
				return;

			Engine.StopXmppClient();
			Engine.SwitchGuiPage("page_modmod.xml", { "cancelbutton": true });
			return;
		}

		const stanza = game.stanza;
		if (stanza.state !== "init" && game.players.every(player => player.Name !== playername))
		{
			const buttonIndex = await messageBox(
				400, 200,
				translate("The game has already started. Do you want to join as observer?"),
				translate("Confirmation"),
				[translate("No"), translate("Yes")]);

			if (buttonIndex === 0)
				return;
		}

		if (this.joinButton.hidden)
			return;

		Engine.OpenChildPage("page_gamesetup_mp.xml", {
			"multiplayerGameType": "join",
			"name": g_Nickname,
			"rating": this.getRejoinRating(stanza),
			"hasPassword": !!stanza.hasPassword,
			"hostJID": stanza.hostJID
		});
	}

	/**
	 * Rejoin games with the original playername, even if the rating changed meanwhile.
	 */
	getRejoinRating(game)
	{
		for (const player of game.players)
		{
			const playerNickRating = splitRatingFromNick(player.Name);
			if (playerNickRating.nick == g_Nickname)
				return playerNickRating.rating;
		}
		return Engine.LobbyGetPlayerRating(g_Nickname);
	}
}

// Translation: Join the game currently selected in the list.
JoinButton.prototype.Caption = translate("Join Game");
