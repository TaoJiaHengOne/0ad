/**
 * This class is extended in subclasses.
 * Each subclass represents one button in the session menu.
 * All subclasses  store the button member so that mods can change it easily.
 */
class MenuButtons
{
}

MenuButtons.prototype.Manual = class
{
	constructor(button, pauseControl)
	{
		this.button = button;
		this.button.caption = translate(translate("Manual"));
		this.pauseControl = pauseControl;
	}

	async onPress()
	{
		closeOpenDialogs();
		this.pauseControl.implicitPause();
		await Engine.OpenChildPage("page_manual.xml");
		resumeGame();
	}
};

MenuButtons.prototype.Chat = class
{
	constructor(button, pauseControl, playerViewControl, chat)
	{
		this.button = button;
		this.button.caption = translate("Chat");
		this.chat = chat;
		registerHotkeyChangeHandler(this.rebuild.bind(this));
	}

	rebuild()
	{
		this.button.tooltip = this.chat.getOpenHotkeyTooltip().trim();
	}

	onPress()
	{
		this.chat.openPage();
	}
};

MenuButtons.prototype.Save = class
{
	constructor(button, pauseControl)
	{
		this.button = button;
		this.button.caption = translate("Save");
		this.pauseControl = pauseControl;
	}

	async onPress()
	{
		closeOpenDialogs();
		this.pauseControl.implicitPause();

		await Engine.OpenChildPage(
			"page_loadgame.xml",
			{
				"savedGameData": getSavedGameData(),
				"campaignRun": g_CampaignSession ? g_CampaignSession.run.filename : null
			});
		resumeGame();
	}
};

MenuButtons.prototype.Summary = class
{
	constructor(button, pauseControl)
	{
		this.button = button;
		this.button.caption = translate("Summary");
		this.button.hotkey = "summary";
		// TODO: Atlas should pass g_InitAttributes.settings
		this.button.enabled = !Engine.IsAtlasRunning();

		this.pauseControl = pauseControl;
		this.selectedData = undefined;

		registerHotkeyChangeHandler(this.rebuild.bind(this));
	}

	rebuild()
	{
		this.button.tooltip = sprintf(translate("Press %(hotkey)s to open the summary screen."), {
			"hotkey": colorizeHotkey("%(hotkey)s", this.button.hotkey),
		});
	}

	async onPress()
	{
		if (Engine.IsAtlasRunning())
			return;

		closeOpenDialogs();
		this.pauseControl.implicitPause();
		// Allows players to see their own summary.
		// If they have shared ally vision researched, they are able to see the summary of there allies too.
		const simState = Engine.GuiInterfaceCall("GetExtendedSimulationState");
		const data = await Engine.OpenChildPage(
			"page_summary.xml",
			{
				"sim": {
					"mapSettings": g_InitAttributes.settings,
					"playerStates": simState.players.filter((state, player) =>
						g_IsObserver || g_ViewedPlayer == 0 || player == 0 || player == g_ViewedPlayer ||
						simState.players[g_ViewedPlayer].hasSharedLos && g_Players[player].isMutualAlly[g_ViewedPlayer]),
					"timeElapsed": simState.timeElapsed
				},
				"gui": {
					"dialog": true,
					"isInGame": true,
					"summarySelection": this.summarySelection
				},
			});

		this.summarySelection = data.summarySelection;
		this.pauseControl.implicitResume();
	}
};

MenuButtons.prototype.Lobby = class
{
	constructor(button)
	{
		this.button = button;
		this.button.caption = translate("Lobby");
		this.button.hotkey = "lobby";
		this.button.enabled = Engine.HasXmppClient();

		registerHotkeyChangeHandler(this.rebuild.bind(this));
	}

	rebuild()
	{
		this.button.tooltip = sprintf(translate("Press %(hotkey)s to open the multiplayer lobby page without leaving the game."), {
			"hotkey": colorizeHotkey("%(hotkey)s", this.button.hotkey),
		});
	}

	onPress()
	{
		if (!Engine.HasXmppClient())
			return;
		closeOpenDialogs();
		Engine.OpenChildPage("page_lobby.xml", { "dialog": true });
	}
};

MenuButtons.prototype.Options = class
{
	constructor(button, pauseControl)
	{
		this.button = button;
		this.button.caption = translate("Options");
		this.pauseControl = pauseControl;
	}

	async onPress()
	{
		closeOpenDialogs();
		this.pauseControl.implicitPause();

		fireConfigChangeHandlers(await Engine.OpenChildPage("page_options.xml"));
		resumeGame();
	}
};

MenuButtons.prototype.Hotkeys = class
{
	constructor(button, pauseControl)
	{
		this.button = button;
		this.button.caption = translate("Hotkeys");
		this.pauseControl = pauseControl;
	}

	async onPress()
	{
		closeOpenDialogs();
		this.pauseControl.implicitPause();

		await Engine.OpenChildPage("hotkeys/page_hotkeys.xml");
		resumeGame();
	}
};

MenuButtons.prototype.Pause = class
{
	constructor(button, pauseControl, playerViewControl)
	{
		this.button = button;
		this.button.hotkey = "pause";
		this.pauseControl = pauseControl;

		registerPlayersInitHandler(this.rebuild.bind(this));
		registerPlayersFinishedHandler(this.rebuild.bind(this));
		playerViewControl.registerPlayerIDChangeHandler(this.rebuild.bind(this));
		pauseControl.registerPauseHandler(this.rebuild.bind(this));
		registerHotkeyChangeHandler(this.rebuild.bind(this));
		registerNetworkStatusChangeHandler(this.rebuild.bind(this));
	}

	rebuild()
	{
		this.button.enabled = this.pauseControl.canPause(true);
		this.button.caption = this.pauseControl.explicitPause ? translate("Resume") : translate("Pause");
		this.button.tooltip = sprintf(translate("Press %(hotkey)s to pause or resume the game."), {
			"hotkey": colorizeHotkey("%(hotkey)s", this.button.hotkey),
		});
	}

	onPress()
	{
		this.pauseControl.setPaused(!g_PauseControl.explicitPause, true);
	}
};

MenuButtons.prototype.Resign = class
{
	constructor(button, pauseControl, playerViewControl)
	{
		this.button = button;
		this.button.caption = translate("Resign");
		this.pauseControl = pauseControl;

		registerPlayersInitHandler(this.rebuild.bind(this));
		registerPlayersFinishedHandler(this.rebuild.bind(this));
		playerViewControl.registerPlayerIDChangeHandler(this.rebuild.bind(this));
	}

	rebuild()
	{
		this.button.enabled = !g_IsObserver;
	}

	onPress()
	{
		(new ResignConfirmation()).display();
	}
};

MenuButtons.prototype.Exit = class
{
	constructor(button, pauseControl)
	{
		this.button = button;
		this.button.caption = translate("Exit");
		this.button.enabled = !Engine.IsAtlasRunning();
		this.pauseControl = pauseControl;
	}

	onPress()
	{
		for (const name in QuitConfirmationMenu.prototype)
		{
			const quitConfirmation = new QuitConfirmationMenu.prototype[name]();
			if (quitConfirmation.enabled())
				quitConfirmation.display();
		}
	}
};
