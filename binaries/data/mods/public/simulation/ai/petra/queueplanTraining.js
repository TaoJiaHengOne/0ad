PETRA.TrainingPlan = function(gameState, type, metadata, number = 1, maxMerge = 5)
{
	if (!PETRA.QueuePlan.call(this, gameState, type, metadata))
	{
		API3.warn(" Plan training " + type + " canceled");
		return false;
	}

	// Refine the estimated cost and add pop cost
	const trainers = this.getBestTrainers(gameState);
	const trainer = trainers ? trainers[0] : undefined;
	this.cost = new API3.Resources(this.template.cost(trainer), +this.template._template.Cost.Population);

	this.category = "unit";
	this.number = number;
	this.maxMerge = maxMerge;

	return true;
};

PETRA.TrainingPlan.prototype = Object.create(PETRA.QueuePlan.prototype);

PETRA.TrainingPlan.prototype.canStart = function(gameState)
{
	this.trainers = this.getBestTrainers(gameState);
	if (!this.trainers)
		return false;
	this.cost = new API3.Resources(this.template.cost(this.trainers[0]), +this.template._template.Cost.Population);
	return true;
};

PETRA.TrainingPlan.prototype.getBestTrainers = function(gameState)
{
	if (this.metadata && this.metadata.trainer)
	{
		const trainer = gameState.getEntityById(this.metadata.trainer);
		if (trainer)
			return [trainer];
	}

	let allTrainers = gameState.findTrainers(this.type);
	if (this.metadata && this.metadata.sea)
		allTrainers = allTrainers.filter(API3.Filters.byMetadata(PlayerID, "sea", this.metadata.sea));
	if (this.metadata && this.metadata.base)
		allTrainers = allTrainers.filter(API3.Filters.byMetadata(PlayerID, "base", this.metadata.base));
	if (!allTrainers || !allTrainers.hasEntities())
		return undefined;

	// Keep only trainers with smallest cost
	let costMin = Math.min();
	let trainers;
	for (const ent of allTrainers.values())
	{
		const cost = this.template.costSum(ent);
		if (cost === costMin)
			trainers.push(ent);
		else if (cost < costMin)
		{
			costMin = cost;
			trainers = [ent];
		}
	}
	return trainers;
};

PETRA.TrainingPlan.prototype.start = function(gameState)
{
	if (this.metadata && this.metadata.trainer)
	{
		const metadata = {};
		for (const key in this.metadata)
			if (key !== "trainer")
				metadata[key] = this.metadata[key];
		this.metadata = metadata;
	}

	if (this.trainers.length > 1)
	{
		let wantedIndex;
		if (this.metadata && this.metadata.index)
			wantedIndex = this.metadata.index;
		const workerUnit = this.metadata && this.metadata.role && this.metadata.role === PETRA.Worker.ROLE_WORKER;
		const supportUnit = this.template.hasClass("Support");
		this.trainers.sort(function(a, b) {
			// Prefer training buildings with short queues
			let aa = a.trainingQueueTime();
			let bb = b.trainingQueueTime();
			// Give priority to support units in the cc
			if (a.hasClass("Civic") && !supportUnit)
				aa += 10;
			if (b.hasClass("Civic") && !supportUnit)
				bb += 10;
			// And support units should not be too near to dangerous place
			if (supportUnit)
			{
				if (gameState.ai.HQ.isNearInvadingArmy(a.position()))
					aa += 50;
				if (gameState.ai.HQ.isNearInvadingArmy(b.position()))
					bb += 50;
			}
			// Give also priority to buildings with the right accessibility
			const aBase = a.getMetadata(PlayerID, "base");
			const bBase = b.getMetadata(PlayerID, "base");
			if (wantedIndex)
			{
				if (!aBase || gameState.ai.HQ.getBaseByID(aBase).accessIndex != wantedIndex)
					aa += 30;
				if (!bBase || gameState.ai.HQ.getBaseByID(bBase).accessIndex != wantedIndex)
					bb += 30;
			}
			// Then, if workers, small preference for bases with less workers
			if (workerUnit && aBase && bBase && aBase != bBase)
			{
				const apop = gameState.ai.HQ.getBaseByID(aBase).workers.length;
				const bpop = gameState.ai.HQ.getBaseByID(bBase).workers.length;
				if (apop > bpop)
					aa++;
				else if (bpop > apop)
					bb++;
			}
			return aa - bb;
		});
	}

	if (this.metadata && this.metadata.base !== undefined && this.metadata.base === 0)
		this.metadata.base = this.trainers[0].getMetadata(PlayerID, "base");
	this.trainers[0].train(gameState.getPlayerCiv(), this.type, this.number, this.metadata);

	this.onStart(gameState);
};

PETRA.TrainingPlan.prototype.addItem = function(amount = 1)
{
	this.number += amount;
};

PETRA.TrainingPlan.prototype.Serialize = function()
{
	return {
		"category": this.category,
		"type": this.type,
		"ID": this.ID,
		"metadata": this.metadata,
		"cost": this.cost.Serialize(),
		"number": this.number,
		"maxMerge": this.maxMerge
	};
};

PETRA.TrainingPlan.prototype.Deserialize = function(gameState, data)
{
	for (const key in data)
		this[key] = data[key];

	this.cost = new API3.Resources();
	this.cost.Deserialize(data.cost);
};
