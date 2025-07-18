Trigger.prototype.InitRegicideGame = function(msg)
{
	const cmpEndGameManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_EndGameManager);
	const regicideGarrison = cmpEndGameManager.GetGameSettings().regicideGarrison;

	const playersCivs = [];
	for (let playerID = 1; playerID < TriggerHelper.GetNumberOfPlayers(); ++playerID)
		playersCivs[playerID] = QueryPlayerIDInterface(playerID, IID_Identity).GetCiv();

	// Get all hero templates of these civs
	const heroTemplates = {};
	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	for (const templateName of cmpTemplateManager.FindAllTemplates(false))
	{
		if (!templateName.startsWith("units/"))
			continue;

		const identity = cmpTemplateManager.GetTemplate(templateName).Identity;
		const classes = GetIdentityClasses(identity);

		if (classes.indexOf("Hero") == -1 ||
		    playersCivs.every(civ => civ != identity.Civ))
			continue;

		if (!heroTemplates[identity.Civ])
			heroTemplates[identity.Civ] = [];

		if (heroTemplates[identity.Civ].indexOf(templateName) == -1)
			heroTemplates[identity.Civ].push({
				"templateName": regicideGarrison ? templateName : "ungarrisonable|" + templateName,
				"classes": classes
			});
	}

	// Sort available spawn points by preference
	const spawnPreferences = ["CivilCentre", "Structure", "Ship"];
	const getSpawnPreference = entity => {

		const cmpIdentity = Engine.QueryInterface(entity, IID_Identity);
		if (!cmpIdentity)
			return -1;

		const classes = cmpIdentity.GetClassesList();

		for (const i in spawnPreferences)
			if (classes.indexOf(spawnPreferences[i]) != -1)
				return spawnPreferences.length - i;
		return 0;
	};

	// Attempt to spawn one hero per player
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	for (let playerID = 1; playerID < TriggerHelper.GetNumberOfPlayers(); ++playerID)
	{
		let spawnPoints = cmpRangeManager.GetEntitiesByPlayer(playerID).sort((entity1, entity2) =>
			getSpawnPreference(entity2) - getSpawnPreference(entity1));

		// Spawn the hero on land as close as possible
		if (!regicideGarrison && TriggerHelper.EntityMatchesClassList(spawnPoints[0], "Ship"))
		{
			const shipPosition = Engine.QueryInterface(spawnPoints[0], IID_Position).GetPosition2D();

			const distanceToShip = entity =>
				Engine.QueryInterface(entity, IID_Position).GetPosition2D().distanceToSquared(shipPosition);

			spawnPoints = TriggerHelper.GetLandSpawnPoints().sort((entity1, entity2) =>
				distanceToShip(entity1) - distanceToShip(entity2));
		}

		this.regicideHeroes[playerID] = this.SpawnRegicideHero(playerID, heroTemplates[playersCivs[playerID]], spawnPoints);
	}
};

/**
 * Spawn a random hero at one of the given locations (which are checked in order).
 * Garrison it if the location is a ship.
 *
 * @param spawnPoints - entity IDs at which to spawn
 */
Trigger.prototype.SpawnRegicideHero = function(playerID, heroTemplates, spawnPoints)
{
	for (const heroTemplate of shuffleArray(heroTemplates))
		for (const spawnPoint of spawnPoints)
		{
			const cmpPosition = Engine.QueryInterface(spawnPoint, IID_Position);
			if (!cmpPosition || !cmpPosition.IsInWorld())
				continue;

			// Consider nomad maps where units start on a ship
			const isShip = TriggerHelper.EntityMatchesClassList(spawnPoint, "Ship");
			if (isShip)
			{
				const cmpGarrisonHolder = Engine.QueryInterface(spawnPoint, IID_GarrisonHolder);
				if (cmpGarrisonHolder.IsFull() ||
				    !MatchesClassList(heroTemplate.classes, cmpGarrisonHolder.GetAllowedClasses()))
					continue;
			}

			let hero = TriggerHelper.SpawnUnits(spawnPoint, heroTemplate.templateName, 1, playerID);
			if (!hero.length)
				continue;

			hero = hero[0];

			if (isShip)
			{
				const cmpUnitAI = Engine.QueryInterface(hero, IID_UnitAI);
				cmpUnitAI.Garrison(spawnPoint);
			}

			return hero;
		}

	error("Couldn't spawn hero for player " + playerID);
	return undefined;
};

Trigger.prototype.RenameRegicideHero = function(data)
{
	const index = this.regicideHeroes.indexOf(data.entity);
	if (index != -1)
		this.regicideHeroes[index] = data.newentity;
};

Trigger.prototype.CheckRegicideDefeat = function(data)
{
	if (data.entity == this.regicideHeroes[data.from])
		TriggerHelper.DefeatPlayer(
			data.from,
			markForTranslation("%(player)s has been defeated (lost hero)."));
};

{
	const cmpTrigger = Engine.QueryInterface(SYSTEM_ENTITY, IID_Trigger);
	cmpTrigger.regicideHeroes = [];
	cmpTrigger.DoAfterDelay(0, "InitRegicideGame", {});
	cmpTrigger.RegisterTrigger("OnOwnershipChanged", "CheckRegicideDefeat", { "enabled": true });
	cmpTrigger.RegisterTrigger("OnEntityRenamed", "RenameRegicideHero", { "enabled": true });
}
