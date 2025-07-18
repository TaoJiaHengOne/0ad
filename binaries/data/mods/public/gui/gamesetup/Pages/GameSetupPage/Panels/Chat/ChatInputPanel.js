class ChatInputPanel
{
	constructor(netMessages, chatInputAutocomplete)
	{
		this.chatInputAutocomplete = chatInputAutocomplete;

		this.chatSubmitButton = Engine.GetGUIObjectByName("chatSubmitButton");
		resizeGUIObjectToCaption(this.chatSubmitButton, { "horizontal": "left" }, { "horizontal": 8 });

		this.chatInput = Engine.GetGUIObjectByName("chatInput");
		this.chatSubmitButton.size.right = this.chatSubmitButton.size.left;

		this.chatInput = Engine.GetGUIObjectByName("chatInput");
		this.chatInput.tooltip = colorizeAutocompleteHotkey(this.Tooltip);
		this.chatInput.onPress = this.onPress.bind(this);
		this.chatInput.onTab = this.onTab.bind(this);
		this.chatInput.focus();

		this.chatSubmitButton = Engine.GetGUIObjectByName("chatSubmitButton");
		this.chatSubmitButton.onPress = this.onPress.bind(this);

		netMessages.registerNetMessageHandler("netstatus", this.onNetStatusMessage.bind(this));
	}

	onNetStatusMessage(message)
	{
		if (message.status == "disconnected")
		{
			reportDisconnect(message, true);
			this.chatInput.hidden = true;
			this.chatSubmitButton.hidden = true;
		}
	}

	onTab()
	{
		autoCompleteText(
			this.chatInput,
			this.chatInputAutocomplete.getAutocompleteEntries());
	}

	onPress()
	{
		if (!g_IsNetworked)
			return;

		const text = this.chatInput.caption;
		if (!text.length)
			return;

		this.chatInput.caption = "";

		if (!executeNetworkCommand(text))
			Engine.SendNetworkChat(text);

		this.chatInput.focus();
	}
}

ChatInputPanel.prototype.Tooltip =
	translate("Press %(hotkey)s to autocomplete player names or settings.");
