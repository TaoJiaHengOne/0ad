/**
 * This class is concerned with displaying the most recent chat messages on a screen overlay for some seconds.
 */
class ChatOverlay
{
	constructor()
	{
		/**
		 * Maximum number of lines to display simultaneously.
		 */
		this.chatLinesNumber = 20;

		/**
		 * Number of seconds after which chatmessages will disappear.
		 */
		this.chatTimeout = 30;

		/**
		 * Holds the timer-IDs used for hiding the chat after chatTimeout seconds.
		 */
		this.chatTimers = [];

		/**
		 * The currently displayed strings, limited by the given timeframe and limit above.
		 */
		this.chatMessages = [];

		this.chatText = Engine.GetGUIObjectByName("chatText");
		this.chatLines = Engine.GetGUIObjectByName("chatLines").children;
		this.chatLinesNumber = Math.min(this.chatLinesNumber, this.chatLines.length);
	}

	displayChatMessages()
	{
		for (let i = 0; i < this.chatLinesNumber; ++i)
		{
			const chatMessage = this.chatMessages[i];
			if (chatMessage && chatMessage.text)
			{
				// First scale line width to maximum size.
				const height = this.chatLines[i].size.bottom - this.chatLines[i].size.top;
				Object.assign(this.chatLines[i].size, {
					"top": i * height,
					"bottom": (i + 1) * height,
					"rright": 100
				});

				this.chatLines[i].caption = chatMessage.text;

				// Now read the actual text width and scale the line width accordingly.
				this.chatLines[i].size.rright = 0;
				this.chatLines[i].size.right = this.chatLines[i].size.left + this.chatLines[i].getTextSize().width;

				if (chatMessage.callback)
					this.chatLines[i].onPress = chatMessage.callback;

				if (chatMessage.tooltip)
					this.chatLines[i].tooltip = chatMessage.tooltip;
			}
			this.chatLines[i].hidden = !chatMessage || !chatMessage.text;
			this.chatLines[i].ghost = !chatMessage || !chatMessage.callback;
		}
	}

	/**
	 * Displays this message in the chat overlay and sets up the timer to remove it after a while.
	 */
	onChatMessage(msg, chatMessage)
	{
		this.chatMessages.push(chatMessage);
		this.chatTimers.push(setTimeout(this.removeOldChatMessage.bind(this), this.chatTimeout * 1000));

		if (this.chatMessages.length > this.chatLinesNumber)
			this.removeOldChatMessage();
		else
			this.displayChatMessages();
	}

	/**
	 * Empty all messages currently displayed in the chat overlay.
	 */
	clearChatMessages()
	{
		this.chatMessages = [];
		this.displayChatMessages();

		for (const timer of this.chatTimers)
			clearTimeout(timer);

		this.chatTimers = [];
	}

	/**
	 * Called when the timer has run out for the oldest chatmessage or when the message limit is reached.
	 */
	removeOldChatMessage()
	{
		clearTimeout(this.chatTimers[0]);
		this.chatTimers.shift();
		this.chatMessages.shift();
		this.displayChatMessages();
	}
}
