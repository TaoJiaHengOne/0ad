/**
 * This class stores the handlers for all GUI objects in the lobby page,
 * (excluding other pages in the same context such as leaderboard and profile page).
 */
class LobbyPage
{
	constructor(closePageCallback, dialog, xmppMessages, leaderboardPage, profilePage)
	{
		Engine.ProfileStart("Create LobbyPage");
		const mapCache = new MapCache();
		const buddyButton = new BuddyButton(xmppMessages);
		const accountSettingsButton = Engine.GetGUIObjectByName("accountSettingsButton");
		accountSettingsButton.onPress = AccountSettingsPage.openPage.bind(null, xmppMessages);
		const gameList = new GameList(xmppMessages, buddyButton, mapCache);
		const playerList = new PlayerList(xmppMessages, buddyButton, gameList);

		this.lobbyPage = {
			"buttons": {
				"buddyButton": buddyButton,
				"accountSettingsButton": accountSettingsButton,
				"joinButton": new JoinButton(dialog, gameList),
				"hostButton": new HostButton(dialog, xmppMessages,
					Engine.GetGUIObjectByName("hostButton"), false),
				"hostSavedGameButton": new HostButton(dialog, xmppMessages,
					Engine.GetGUIObjectByName("hostSavedGameButton"), true),
				"leaderboardButton": new LeaderboardButton(xmppMessages, leaderboardPage),
				"profileButton": new ProfileButton(xmppMessages, profilePage),
				"quitButton": new QuitButton(closePageCallback, dialog, leaderboardPage,
					profilePage)
			},
			"panels": {
				"chatPanel": new ChatPanel(xmppMessages),
				"gameDetails": new GameDetails(dialog, gameList, mapCache),
				"gameList": gameList,
				"playerList": playerList,
				"profilePanel": new ProfilePanel(xmppMessages, playerList, leaderboardPage),
				"subject": new Subject(dialog, xmppMessages, gameList)
			},
			"eventHandlers": {
				"announcementHandler": new AnnouncementHandler(xmppMessages),
				"connectionHandler": new ConnectionHandler(xmppMessages),
			}
		};

		if (dialog)
			this.setDialogStyle();
		Engine.ProfileStop();
	}

	setDialogStyle()
	{
		const lobbyPage = Engine.GetGUIObjectByName("lobbyPage");
		lobbyPage.sprite = "ModernDialog";
		lobbyPage.size = {
			"left": this.WindowMargin,
			"top": this.WindowMargin,
			"right": -this.WindowMargin,
			"bottom": -this.WindowMargin,
			"rleft": 0,
			"rtop": 0,
			"rright": 100,
			"rbottom": 100
		};

		const lobbyPageTitle = Engine.GetGUIObjectByName("lobbyPageTitle");
		lobbyPageTitle.size.top -= this.WindowMargin / 2;
		lobbyPageTitle.size.bottom -= this.WindowMargin / 2;

		Engine.GetGUIObjectByName("lobbyPanels").size.top -= this.WindowMargin / 2;
	}
}

LobbyPage.prototype.WindowMargin = 40;
