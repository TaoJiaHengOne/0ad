Engine.IncludeModule("common-api");

var PETRA = {};

PETRA.PetraBot = function(settings)
{
	API3.BaseAI.call(this, settings);

	this.playedTurn = 0;
	this.elapsedTime = 0;

	this.uniqueIDs = {
		"armies": 1,	// starts at 1 to allow easier tests on armies ID existence
		"bases": 1,	// base manager ID starts at one because "0" means "no base" on the map
		"plans": 0,	// training/building/research plans
		"transports": 1	// transport plans start at 1 because 0 might be used as none
	};

	this.Config = new PETRA.Config(settings.difficulty, settings.behavior);

	this.savedEvents = {};
};

PETRA.PetraBot.prototype = Object.create(API3.BaseAI.prototype);

PETRA.PetraBot.prototype.CustomInit = function(gameState)
{
	if (this.isDeserialized)
	{
		// WARNING: the deserializations should not modify the metadatas infos inside their init functions
		this.turn = this.data.turn;
		this.playedTurn = this.data.playedTurn;
		this.elapsedTime = this.data.elapsedTime;
		this.savedEvents = this.data.savedEvents;
		for (const key in this.savedEvents)
		{
			for (const i in this.savedEvents[key])
			{
				if (!this.savedEvents[key][i].entityObj)
					continue;
				const evt = this.savedEvents[key][i];
				const evtmod = {};
				for (const keyevt in evt)
				{
					evtmod[keyevt] = evt[keyevt];
					evtmod.entityObj = new API3.Entity(gameState.sharedScript, evt.entityObj);
					this.savedEvents[key][i] = evtmod;
				}
			}
		}

		this.Config.Deserialize(this.data.config);

		this.queueManager = new PETRA.QueueManager(this.Config, {});
		this.queueManager.Deserialize(gameState, this.data.queueManager);
		this.queues = this.queueManager.queues;

		this.HQ = new PETRA.HQ(this.Config);
		this.HQ.init(gameState, this.queues);
		this.HQ.Deserialize(gameState, this.data.HQ);

		this.uniqueIDs = this.data.uniqueIDs;
		this.isDeserialized = false;
		this.data = undefined;

		// initialisation needed after the completion of the deserialization
		this.HQ.postinit(gameState);
	}
	else
	{
		this.Config.setConfig(gameState);

		// this.queues can only be modified by the queue manager or things will go awry.
		this.queues = {};
		for (const i in this.Config.priorities)
			this.queues[i] = new PETRA.Queue();

		this.queueManager = new PETRA.QueueManager(this.Config, this.queues);

		this.HQ = new PETRA.HQ(this.Config);

		this.HQ.init(gameState, this.queues);

		// Analyze our starting position and set a strategy
		this.HQ.gameAnalysis(gameState);
	}
};

PETRA.PetraBot.prototype.OnUpdate = function(sharedScript)
{
	if (this.gameFinished || this.gameState.playerData.state == "defeated")
		return;

	for (const i in this.events)
	{
		if (i == "AIMetadata")   // not used inside petra
			continue;
		if(this.savedEvents[i] !== undefined)
			this.savedEvents[i] = this.savedEvents[i].concat(this.events[i]);
		else
			this.savedEvents[i] = this.events[i];
	}

	// Run the update every n turns, offset depending on player ID to balance the load
	this.elapsedTime = this.gameState.getTimeElapsed() / 1000;
	if (!this.playedTurn || (this.turn + this.player) % 8 == 5)
	{
		Engine.ProfileStart("PetraBot bot (player " + this.player +")");

		this.playedTurn++;

		if (this.gameState.getOwnEntities().length === 0)
		{
			Engine.ProfileStop();
			return; // With no entities to control the AI cannot do anything
		}

		this.HQ.update(this.gameState, this.queues, this.savedEvents);

		this.queueManager.update(this.gameState);

		for (const i in this.savedEvents)
			this.savedEvents[i] = [];

		Engine.ProfileStop();
	}

	this.turn++;
};

PETRA.PetraBot.prototype.Serialize = function()
{
	const savedEvents = {};
	for (const key in this.savedEvents)
	{
		savedEvents[key] = this.savedEvents[key].slice();
		for (const i in savedEvents[key])
		{
			if (!savedEvents[key][i] || !savedEvents[key][i].entityObj)
				continue;
			const evt = savedEvents[key][i];
			const evtmod = {};
			for (const keyevt in evt)
				evtmod[keyevt] = evt[keyevt];
			evtmod.entityObj = evt.entityObj._entity;
			savedEvents[key][i] = evtmod;
		}
	}

	return {
		"uniqueIDs": this.uniqueIDs,
		"turn": this.turn,
		"playedTurn": this.playedTurn,
		"elapsedTime": this.elapsedTime,
		"savedEvents": savedEvents,
		"config": this.Config.Serialize(),
		"queueManager": this.queueManager.Serialize(),
		"HQ": this.HQ.Serialize()
	};
};

PETRA.PetraBot.prototype.Deserialize = function(data, sharedScript)
{
	this.isDeserialized = true;
	this.data = data;
};
