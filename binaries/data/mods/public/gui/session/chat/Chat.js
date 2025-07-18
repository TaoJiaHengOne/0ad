/**
 * This class is only concerned with owning the helper classes and linking them.
 * The class is not dealing with specific GUI objects and doesn't provide own handlers.
 */
class Chat
{
	constructor(playerViewControl, cheats)
	{
		this.ChatWindow = new ChatWindow();
		this.ChatOverlay = new ChatOverlay();

		this.ChatHistory = new ChatHistory();
		this.ChatHistory.registerSelectionChangeHandler(this.ChatWindow.onSelectionChange.bind(this.ChatWindow));

		const alignmentHelper = new AlignmentHelper("max");

		this.ChatInput = new ChatInput(alignmentHelper);
		this.ChatInput.registerChatSubmitHandler(executeNetworkCommand);
		this.ChatInput.registerChatSubmitHandler(cheats.executeCheat.bind(cheats));
		this.ChatInput.registerChatSubmitHandler(this.submitChat.bind(this));
		this.ChatInput.registerChatSubmittedHandler(this.closePage.bind(this));

		this.ChatAddressees = new ChatAddressees(alignmentHelper);
		this.ChatAddressees.registerSelectionChangeHandler(this.ChatInput.onSelectionChange.bind(this.ChatInput));
		this.ChatAddressees.registerSelectionChangeHandler(this.ChatWindow.onSelectionChange.bind(this.ChatWindow));

		this.ChatMessageHandler = new ChatMessageHandler();
		this.ChatMessageHandler.registerMessageFormatClass(ChatMessageFormatNetwork);
		this.ChatMessageHandler.registerMessageFormatClass(ChatMessageFormatSimulation);
		this.ChatMessageFormatPlayer = new ChatMessageFormatPlayer();
		this.ChatMessageFormatPlayer.registerAddresseeTypes(this.ChatAddressees.AddresseeTypes);
		this.ChatMessageHandler.registerMessageFormat("message", this.ChatMessageFormatPlayer);
		this.ChatMessageHandler.registerMessageHandler(this.ChatOverlay.onChatMessage.bind(this.ChatOverlay));
		this.ChatMessageHandler.registerMessageHandler(this.ChatHistory.onChatMessage.bind(this.ChatHistory));
		this.ChatMessageHandler.registerMessageHandler(() => {
			if (this.ChatWindow.isOpen() && this.ChatWindow.isExtended())
				this.ChatHistory.displayChatHistory();
		});

		const updater = this.onUpdatePlayers.bind(this);
		registerPlayersFinishedHandler(updater);
		registerPlayerAssignmentsChangeHandler(updater);
		playerViewControl.registerViewedPlayerChangeHandler(updater);

		Engine.SetGlobalHotkey("chat", "Press", this.openPage.bind(this));
		Engine.SetGlobalHotkey("privatechat", "Press", this.openPage.bind(this));
		Engine.SetGlobalHotkey("teamchat", "Press", () => { this.openPage(g_IsObserver ? "/observers" : "/allies"); });
	}

	/**
	 * Called by the owner whenever g_PlayerAssignments or g_Players changed.
	 */
	onUpdatePlayers()
	{
		this.ChatAddressees.onUpdatePlayers();
	}

	openPage(command = "")
	{
		if (g_Disconnected)
			return;

		closeOpenDialogs();

		this.ChatAddressees.select(command);
		this.ChatHistory.displayChatHistory();
		this.ChatWindow.openPage(command);
	}

	closePage()
	{
		this.ChatWindow.closePage();
	}

	getOpenHotkeyTooltip()
	{
		return this.ChatInput.getOpenHotkeyTooltip();
	}

	/**
	 * Send the given chat message to the addressees.
	 */
	submitChat(text, command = "")
	{
		if (command.startsWith("/msg "))
			Engine.SetGlobalHotkey("privatechat", "Press", () => { this.openPage(command); });

		const msg = command ? command + " " + text : text;

		if (Engine.HasNetClient())
			Engine.SendNetworkChat(msg, this.getReceiverGUIDs(text, command));
		else
			this.ChatMessageHandler.handleMessage({
				"type": "message",
				"guid": "local",
				"text": msg
			});
	}

	getReceiverGUIDs(text, command)
	{
		const senderGUID = Engine.GetPlayerGUID();

		if (command.startsWith("/msg "))
		{
			const receiverGUID =
				this.ChatMessageFormatPlayer.matchUsername(
					command.substr("/msg ".length));

			if (!receiverGUID)
			{
				warn("Unknown chat addressee: " + text);
				return [];
			}

			return [senderGUID, receiverGUID];
		}

		const isAddressee = this.ChatAddressees.AddresseeTypes.find(
			type => type.command === command)?.isAddressee;

		if (!isAddressee)
		{
			warn("Unknown chat command " + command);
			return [];
		}

		const senderID = Engine.GetPlayerID();
		return Object.keys(g_PlayerAssignments).filter(potentialReceiverGUID => {
			return potentialReceiverGUID === senderGUID ||
				isAddressee(senderID, g_PlayerAssignments[potentialReceiverGUID].player);
		});
	}
}
