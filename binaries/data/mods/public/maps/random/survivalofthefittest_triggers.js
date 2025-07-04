/**
 * If set to true, it will print how many templates would be spawned if the players were not defeated.
 */
const dryRun = false;

/**
 * If enabled, prints the number of units to the command line output.
 */
const debugLog = false;

/**
 * Get the number of minutes to pass between spawning new treasures.
 */
var treasureTime = () => randFloat(3, 5);

/**
 * Get the time in minutes when the first wave of attackers will be spawned.
 */
var firstWaveTime = () => randFloat(4, 6);

/**
 * Maximum time in minutes between two consecutive waves.
 */
var maxWaveTime = 4;

/**
 * Get the next attacker wave delay.
 */
var waveTime = () => randFloat(0.5, 1) * maxWaveTime;

/**
 * Roughly the number of attackers on the first wave.
 */
var initialAttackers = 5;

/**
 * Increase the number of attackers exponentially, by this percent value per minute.
 */
var percentPerMinute = 1.05;

/**
 * Greatest amount of attackers that can be spawned.
 */
var totalAttackerLimit = 200;

/**
 * Least and greatest amount of siege engines per wave.
 */
var siegeFraction = () => randFloat(0.2, 0.5);

/**
 * Potentially / definitely spawn a gaia hero after this number of minutes.
 */
var heroTime = () => randFloat(20, 60);

/**
 * The following templates can't be built by any player.
 */
var disabledTemplates = (civ) => [
	// Economic structures
	"structures/" + civ + "/corral",
	"structures/" + civ + "/farmstead",
	"structures/" + civ + "/field",
	"structures/" + civ + "/storehouse",
	"structures/" + civ + "/rotarymill",
	"units/maur/support_elephant",

	// Expansions
	"structures/" + civ + "/civil_centre",
	"structures/" + civ + "/military_colony",

	// Walls
	"structures/" + civ + "/wallset_stone",
	"structures/rome/wallset_siege",
	"structures/wallset_palisade",

	// Shoreline
	"structures/" + civ + "/dock",
	"structures/brit/crannog",
	"structures/cart/super_dock",
	"structures/ptol/lighthouse"
];

/**
 * Spawn these treasures in regular intervals.
 */
var treasures = [
	"gaia/treasure/food_barrel",
	"gaia/treasure/food_bin",
	"gaia/treasure/food_crate",
	"gaia/treasure/food_jars",
	"gaia/treasure/metal",
	"gaia/treasure/stone",
	"gaia/treasure/wood",
	"gaia/treasure/wood",
	"gaia/treasure/wood"
];

/**
 * An object that maps from civ [f.e. "spart"] to an object
 * that has the keys "champions", "siege" and "heroes",
 * which is an array containing all these templates,
 * trainable from a building or not.
 */
var attackerUnitTemplates = {};

Trigger.prototype.InitSurvival = function()
{
	this.InitStartingUnits();
	this.LoadAttackerTemplates();
	this.SetDisableTemplates();
	this.PlaceTreasures();
	this.InitializeEnemyWaves();
};

Trigger.prototype.debugLog = function(txt)
{
	if (!debugLog)
		return;

	print("DEBUG [" + Math.round(TriggerHelper.GetMinutes()) + "]  " + txt + "\n");
};

Trigger.prototype.LoadAttackerTemplates = function()
{
	for (const civ of ["gaia", ...Object.keys(loadCivFiles(false))])
		attackerUnitTemplates[civ] = {
			"heroes": TriggerHelper.GetTemplateNamesByClasses("Hero", civ, undefined, true),
			"champions": TriggerHelper.GetTemplateNamesByClasses("Champion+!Elephant", civ, undefined, true),
			"siege": TriggerHelper.GetTemplateNamesByClasses("Siege Champion+Elephant", civ, "packed", undefined)
		};

	this.debugLog("Attacker templates:");
	this.debugLog(uneval(attackerUnitTemplates));
};

Trigger.prototype.SetDisableTemplates = function()
{
	for (let i = 1; i < TriggerHelper.GetNumberOfPlayers(); ++i)
		QueryPlayerIDInterface(i).SetDisabledTemplates(disabledTemplates(QueryPlayerIDInterface(i, IID_Identity).GetCiv()));
};

/**
 *  Remember civic centers and make women invincible.
 */
Trigger.prototype.InitStartingUnits = function()
{
	for (let playerID = 1; playerID < TriggerHelper.GetNumberOfPlayers(); ++playerID)
	{
		this.playerCivicCenter[playerID] = TriggerHelper.GetPlayerEntitiesByClass(playerID, "CivilCentre")[0];
		this.treasureFemale[playerID] = TriggerHelper.GetPlayerEntitiesByClass(playerID, "FemaleCitizen")[0];
		Engine.QueryInterface(this.treasureFemale[playerID], IID_Resistance).SetInvulnerability(true);
	}
};

Trigger.prototype.InitializeEnemyWaves = function()
{
	const time = firstWaveTime() * 60 * 1000;
	Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface).AddTimeNotification({
		"message": markForTranslation("The first wave will start in %(time)s!"),
		"translateMessage": true
	}, time);
	this.DoAfterDelay(time, "StartAnEnemyWave", {});
};

Trigger.prototype.StartAnEnemyWave = function()
{
	const currentMin = TriggerHelper.GetMinutes();
	const nextWaveTime = waveTime();
	const civ = pickRandom(Object.keys(attackerUnitTemplates));

	// Determine total attacker count of the current wave.
	// Exponential increase with time, capped to the limit and fluctuating proportionally with the current wavetime.
	const totalAttackers = Math.ceil(Math.min(totalAttackerLimit,
		initialAttackers * Math.pow(percentPerMinute, currentMin) * nextWaveTime / maxWaveTime));

	const siegeRatio = siegeFraction();

	this.debugLog("Spawning " + totalAttackers + " attackers, siege ratio " + siegeRatio.toFixed(2));

	const attackerCount = TriggerHelper.BalancedTemplateComposition(
		[
			{
				"templates": attackerUnitTemplates[civ].heroes,
				"count": currentMin > heroTime() && attackerUnitTemplates[civ].heroes.length ? 1 : 0
			},
			{
				"templates": attackerUnitTemplates[civ].siege,
				"frequency": siegeRatio
			},
			{
				"templates": attackerUnitTemplates[civ].champions,
				"frequency": 1 - siegeRatio
			}
		],
		totalAttackers);

	this.debugLog("Templates: " + uneval(attackerCount));

	// Spawn the templates
	let spawned = false;
	for (const point of this.GetTriggerPoints("A"))
	{
		if (dryRun)
		{
			spawned = true;
			break;
		}

		// Don't spawn attackers for defeated players and players that lost their cc after win
		const cmpPlayer = QueryOwnerInterface(point, IID_Player);
		if (!cmpPlayer)
			continue;

		const playerID = cmpPlayer.GetPlayerID();
		const civicCentre = this.playerCivicCenter[playerID];
		if (!civicCentre)
			continue;

		// Check if the cc is garrisoned in another building
		const targetPos = TriggerHelper.GetEntityPosition2D(civicCentre);
		if (!targetPos)
			continue;

		for (const templateName in attackerCount)
		{
			const isHero = attackerUnitTemplates[civ].heroes.indexOf(templateName) != -1;

			// Don't spawn gaia hero if the previous one is still alive
			if (this.gaiaHeroes[playerID] && isHero)
			{
				const cmpHealth = Engine.QueryInterface(this.gaiaHeroes[playerID], IID_Health);
				if (cmpHealth && cmpHealth.GetHitpoints() != 0)
				{
					this.debugLog("Not spawning hero for player " + playerID + " as the previous one is still alive");
					continue;
				}
			}

			if (dryRun)
				continue;

			const entities = TriggerHelper.SpawnUnits(point, templateName, attackerCount[templateName], 0);

			ProcessCommand(0, {
				"type": "attack-walk",
				"entities": entities,
				"x": targetPos.x,
				"z": targetPos.y,
				"targetClasses": undefined,
				"allowCapture": false,
				"queued": true
			});

			if (isHero)
				this.gaiaHeroes[playerID] = entities[0];
		}
		spawned = true;
	}

	if (!spawned)
		return;

	Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface).PushNotification({
		"message": markForTranslation("An enemy wave is attacking!"),
		"translateMessage": true
	});

	this.DoAfterDelay(nextWaveTime * 60 * 1000, "StartAnEnemyWave", {});
};

Trigger.prototype.PlaceTreasures = function()
{
	const triggerPoints = this.GetTriggerPoints(pickRandom(["B", "C", "D"]));
	for (const point of triggerPoints)
		TriggerHelper.SpawnUnits(point, pickRandom(treasures), 1, 0);

	this.DoAfterDelay(treasureTime() * 60 * 1000, "PlaceTreasures", {});
};

Trigger.prototype.OnOwnershipChanged = function(data)
{
	if (data.entity == this.playerCivicCenter[data.from])
	{
		this.playerCivicCenter[data.from] = undefined;

		TriggerHelper.DefeatPlayer(
			data.from,
			markForTranslation("%(player)s has been defeated (lost civic center)."));
	}
	else if (data.entity == this.treasureFemale[data.from])
	{
		this.treasureFemale[data.from] = undefined;
		Engine.DestroyEntity(data.entity);
	}
};


{
	const cmpTrigger = Engine.QueryInterface(SYSTEM_ENTITY, IID_Trigger);

	cmpTrigger.treasureFemale = [];
	cmpTrigger.playerCivicCenter = [];
	cmpTrigger.gaiaHeroes = [];

	cmpTrigger.RegisterTrigger("OnInitGame", "InitSurvival", { "enabled": true });
	cmpTrigger.RegisterTrigger("OnOwnershipChanged", "OnOwnershipChanged", { "enabled": true });
}
