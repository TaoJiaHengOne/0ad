/**
 * This class is concerned with setting up the text input field and the send button.
 */
class ChatInput
{
	constructor(alignmentHelper)
	{
		this.selectedCommand = "";
		this.chatSubmitHandlers = [];
		this.chatSubmittedHandlers = [];

		this.chatInputCaption = Engine.GetGUIObjectByName("chatInputCaption");
		resizeGUIObjectToCaption(this.chatInputCaption, { "horizontal": "right" });

		this.chatInput = Engine.GetGUIObjectByName("chatInput");
		this.chatInput.onPress = this.submitChatInput.bind(this);
		this.chatInput.onTab = this.autoComplete.bind(this);

		alignmentHelper.setObject(this.chatInput, "left", this.chatInputCaption.size.right + this.InputMargin);

		this.sendChat = Engine.GetGUIObjectByName("sendChat");
		this.sendChat.onPress = this.submitChatInput.bind(this);

		registerHotkeyChangeHandler(this.onHotkeyChange.bind(this));
	}

	onHotkeyChange()
	{
		const tooltip = this.getInputHotkeyTooltip() + this.getOpenHotkeyTooltip();
		this.chatInput.tooltip = tooltip;
		this.sendChat.tooltip = tooltip;
	}

	getInputHotkeyTooltip()
	{
		return translateWithContext("chat input", "Type the message to send.") + "\n" +
			colorizeAutocompleteHotkey();
	}

	getOpenHotkeyTooltip()
	{
		return colorizeHotkey("\n" + translate("Press %(hotkey)s to open the public chat."), "chat") +
			colorizeHotkey(
				"\n" + (g_IsObserver ?
					translate("Press %(hotkey)s to open the observer chat.") :
					translate("Press %(hotkey)s to open the ally chat.")),
				"teamchat") +
			colorizeHotkey("\n" + translate("Press %(hotkey)s to open the previously selected private chat."), "privatechat");
	}

	/**
	 * The functions registered using this function will be called sequentially
	 * when the user submits chat, until one of them returns true.
	 */
	registerChatSubmitHandler(handler)
	{
		this.chatSubmitHandlers.push(handler);
	}

	/**
	 * The functions registered using this function will be called after the user submitted chat input.
	 */
	registerChatSubmittedHandler(handler)
	{
		this.chatSubmittedHandlers.push(handler);
	}

	/**
	 * Called each time the addressee dropdown changes selection.
	 */
	onSelectionChange(command)
	{
		this.selectedCommand = command;
	}

	autoComplete()
	{
		const playernames = [];
		for (const player in g_PlayerAssignments)
			playernames.push(g_PlayerAssignments[player].name);
		autoCompleteText(this.chatInput, playernames);
	}

	submitChatInput()
	{
		const text = this.chatInput.caption;
		if (text)
			this.chatSubmitHandlers.some(handler => handler(text, this.selectedCommand));

		for (const handler of this.chatSubmittedHandlers)
			handler();
	}
}

ChatInput.prototype.InputMargin = 5;
