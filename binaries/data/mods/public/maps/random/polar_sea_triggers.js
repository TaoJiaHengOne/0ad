const debugLog = false;

var attackerTemplate = "gaia/fauna_wolf_arctic_violent";

var minWaveSize = 1;
var maxWaveSize = 3;

var firstWaveTime = 5;
var minWaveTime = 2;
var maxWaveTime = 4;

/**
 * Attackers will focus the targetCount closest units that have the targetClasses type.
 */
var targetClasses = "Organic+!Domestic";
var targetCount = 3;

Trigger.prototype.DisableTechnologies = function()
{
	for (let i = 1; i < TriggerHelper.GetNumberOfPlayers(); ++i)
		QueryPlayerIDInterface(i).SetDisabledTechnologies([
			"gather_lumbering_ironaxes",
			"gather_lumbering_sharpaxes",
			"gather_lumbering_strongeraxes",
			"gather_wicker_baskets"
		]);
};

Trigger.prototype.SpawnWolvesAndAttack = function()
{
	const waveSize = Math.round(Math.random() * (maxWaveSize - minWaveSize) + minWaveSize);
	const attackers = TriggerHelper.SpawnUnitsFromTriggerPoints("A", attackerTemplate, waveSize, 0);

	if (debugLog)
		print("Spawned " + waveSize + " " + attackerTemplate + " at " + Object.keys(attackers).length + " points\n");

	let allTargets;

	const players = new Array(TriggerHelper.GetNumberOfPlayers()).fill(0).map((v, i) => i + 1);

	for (const spawnPoint in attackers)
	{
		// TriggerHelper.SpawnUnits is guaranteed to spawn
		const firstAttacker = attackers[spawnPoint][0];
		if (!firstAttacker)
			continue;

		const attackerPos = TriggerHelper.GetEntityPosition2D(firstAttacker);
		if (!attackerPos)
			continue;

		// The returned entities are sorted by RangeManager already
		// Only consider units implementing Health since wolves deal damage.
		const targets = PositionHelper.EntitiesNearPoint(attackerPos, 200, players, IID_Health).filter(ent => {
			const cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
			return cmpIdentity && MatchesClassList(cmpIdentity.GetClassesList(), targetClasses);
		});

		let goodTargets = targets.slice(0, targetCount);

		// Look through all targets if there aren't enough nearby ones
		if (goodTargets.length < targetCount)
		{
			if (!allTargets)
				allTargets = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetNonGaiaEntities().filter(ent => {
					const cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
					return cmpIdentity && MatchesClassList(cmpIdentity.GetClassesList(), targetClasses);
				});

			const getDistance = target => {
				const targetPos = TriggerHelper.GetEntityPosition2D(target);
				return targetPos ? attackerPos.distanceToSquared(targetPos) : Infinity;
			};

			goodTargets = [];
			const goodDists = [];
			for (const target of allTargets)
			{
				const dist = getDistance(target);
				let i = goodDists.findIndex(element => dist < element);
				if (i != -1 || goodTargets.length < targetCount)
				{
					if (i == -1)
						i = goodTargets.length;
					goodTargets.splice(i, 0, target);
					goodDists.splice(i, 0, dist);
					if (goodTargets.length > targetCount)
					{
						goodTargets.pop();
						goodDists.pop();
					}
				}
			}
		}

		for (const target of goodTargets)
			ProcessCommand(0, {
				"type": "attack",
				"entities": attackers[spawnPoint],
				"target": target,
				"queued": true
			});
	}

	this.DoAfterDelay((Math.random() * (maxWaveTime - minWaveTime) + minWaveTime) * 60 * 1000, "SpawnWolvesAndAttack", {});
};

{
	const cmpTrigger = Engine.QueryInterface(SYSTEM_ENTITY, IID_Trigger);
	cmpTrigger.RegisterTrigger("OnInitGame", "DisableTechnologies", { "enabled": true });
	cmpTrigger.DoAfterDelay(firstWaveTime * 60 * 1000, "SpawnWolvesAndAttack", {});
}
