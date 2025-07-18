/**
 * This class stores and triggers the event handlers for the GUI messages constructed by the XmppClient.
 */
class XmppMessages
{
	constructor()
	{
		this.xmppMessageHandlers = {};
		for (const type in this.MessageTypes)
		{
			this.xmppMessageHandlers[type] = {};
			for (const level of this.MessageTypes[type])
				this.xmppMessageHandlers[type][level] = new Set();
		}

		this.messageBatchProcessedHandlers = new Set();
		this.playerListUpdateHandlers = new Set();

		Engine.GetGUIObjectByName("lobbyPage").onTick = this.onTick.bind(this);
	}

	registerXmppMessageHandler(type, level, handler)
	{
		this.xmppMessageHandlers[type][level].add(handler);
	}

	unregisterXmppMessageHandler(type, level, handler)
	{
		this.xmppMessageHandlers[type][level].delete(handler);
	}

	registerMessageBatchProcessedHandler(handler)
	{
		this.messageBatchProcessedHandlers.add(handler);
	}

	unregisterMessageBatchProcessedHandler(handler)
	{
		this.messageBatchProcessedHandlers.delete(handler);
	}

	registerPlayerListUpdateHandler(handler)
	{
		this.playerListUpdateHandlers.add(handler);
	}

	unregisterPlayerListUpdateHandler(handler)
	{
		this.playerListUpdateHandlers.delete(handler);
	}

	onTick()
	{
		this.handleMessages(Engine.LobbyGuiPollNewMessages);

		if (Engine.LobbyGuiPollHasPlayerListUpdate())
			for (const handler of this.playerListUpdateHandlers)
				handler();
	}

	processHistoricMessages()
	{
		this.handleMessages(Engine.LobbyGuiPollHistoricMessages);
	}

	handleMessages(getMessages)
	{
		const messages = getMessages();
		if (!messages)
			return;

		for (const message of messages)
		{
			if (!this.xmppMessageHandlers[message.type])
				error("Unrecognized message type: " + message.type);
			else if (!this.xmppMessageHandlers[message.type][message.level])
				error("Unrecognized message level: " + message.level);
			else
				for (const handler of this.xmppMessageHandlers[message.type][message.level])
					handler(message);
		}

		for (const handler of this.messageBatchProcessedHandlers)
			handler();
	}
}

/**
 * Processing of notifications sent by XmppClient.cpp.
 */
XmppMessages.prototype.MessageTypes = {
	"system": [
		"registered",
		"connected",
		"disconnected",
		"error"
	],
	"chat": [
		"subject",
		"join",
		"leave",
		"role",
		"nick",
		"kicked",
		"banned",
		"room-message",
		"private-message",
		"headline"
	],
	"game": [
		"gamelist",
		"profile",
		"leaderboard",
		"ratinglist"
	]
};
