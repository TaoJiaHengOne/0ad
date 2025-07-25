// Contains standardized functions suitable for using in trigger scripts.
// Do not use them in any other simulation script.

var TriggerHelper = {};

TriggerHelper.GetPlayerIDFromEntity = function(ent)
{
	const cmpPlayer = Engine.QueryInterface(ent, IID_Player);
	if (cmpPlayer)
		return cmpPlayer.GetPlayerID();

	return -1;
};

TriggerHelper.IsInWorld = function(ent)
{
	const cmpPosition = Engine.QueryInterface(ent, IID_Position);
	return cmpPosition && cmpPosition.IsInWorld();
};

TriggerHelper.GetEntityPosition2D = function(ent)
{
	const cmpPosition = Engine.QueryInterface(ent, IID_Position);

	if (!cmpPosition || !cmpPosition.IsInWorld())
		return undefined;

	return cmpPosition.GetPosition2D();
};

TriggerHelper.GetOwner = function(ent)
{
	const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
	if (cmpOwnership)
		return cmpOwnership.GetOwner();

	return -1;
};

/**
 * This returns the mapsize in number of tiles, the value corresponding to map_sizes.json, also used by random map scripts.
 */
TriggerHelper.GetMapSizeTiles = function()
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_Terrain).GetTilesPerSide();
};

/**
 * This returns the mapsize in the the coordinate system used in the simulation/, especially cmpPosition.
 */
TriggerHelper.GetMapSizeTerrain = function()
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_Terrain).GetMapSize();
};

/**
 * Returns the elapsed ingame time in milliseconds since the gamestart.
 */
TriggerHelper.GetTime = function()
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer).GetTime();
};

/**
  * Returns the elpased ingame time in minutes since the gamestart. Useful for balancing.
  */
TriggerHelper.GetMinutes = function()
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer).GetTime() / 60 / 1000;
};

TriggerHelper.GetEntitiesByPlayer = function(playerID)
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetEntitiesByPlayer(playerID);
};

TriggerHelper.GetAllPlayersEntities = function()
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetNonGaiaEntities();
};

TriggerHelper.SetUnitStance = function(ent, stance)
{
	const cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
	if (cmpUnitAI)
		cmpUnitAI.SwitchToStance(stance);
};

TriggerHelper.SetUnitFormation = function(playerID, entities, formation)
{
	ProcessCommand(playerID, {
		"type": "formation",
		"entities": entities,
		"formation": formation
	});
};

/**
 * Creates a new unit taking into account possible 0 experience promotion.
 * @param {number} owner Player id of the owner of the new units.
 * @param {string} template Name of the template.
 * @returns the created entity id.
 */
TriggerHelper.AddUpgradeTemplate = function(owner, template)
{
	const upgradedTemplate = GetUpgradedTemplate(owner, template);
	if (upgradedTemplate !== template)
		warn(`tried to spawn template '${template}' but upgraded template '${upgradedTemplate}' will be spawned instead. You might want to create a template that is not affected by this promotion.`);

	return Engine.AddEntity(upgradedTemplate);
};

/**
 * Can be used to "force" a building/unit to spawn a group of entities.
 *
 * @param source Entity id of the point where they will be spawned from
 * @param template Name of the template
 * @param count Number of units to spawn
 * @param owner Player id of the owner of the new units. By default, the owner
 * of the source entity.
 */
TriggerHelper.SpawnUnits = function(source, template, count, owner)
{
	const entities = [];
	const cmpFootprint = Engine.QueryInterface(source, IID_Footprint);
	const cmpPosition = Engine.QueryInterface(source, IID_Position);

	if (!cmpPosition || !cmpPosition.IsInWorld())
	{
		error("tried to create entity from a source without position");
		return entities;
	}

	if (owner == null)
		owner = TriggerHelper.GetOwner(source);

	for (let i = 0; i < count; ++i)
	{
		const ent = this.AddUpgradeTemplate(owner, template);

		const cmpEntPosition = Engine.QueryInterface(ent, IID_Position);
		if (!cmpEntPosition)
		{
			Engine.DestroyEntity(ent);
			error("tried to create entity without position");
			continue;
		}

		const cmpEntOwnership = Engine.QueryInterface(ent, IID_Ownership);
		if (cmpEntOwnership)
			cmpEntOwnership.SetOwner(owner);

		entities.push(ent);

		let pos;
		if (cmpFootprint)
			pos = cmpFootprint.PickSpawnPoint(ent);

		// TODO this can happen if the player build on the place
		// where our trigger point is
		// We should probably warn the trigger maker in some way,
		// but not interrupt the game for the player
		if (!pos || pos.y < 0)
			pos = cmpPosition.GetPosition();

		cmpEntPosition.JumpTo(pos.x, pos.z);
	}

	return entities;
};

/**
 * Can be used to spawn garrisoned units inside a building/ship.
 *
 * @param entity Entity id of the garrison holder in which units will be garrisoned
 * @param template Name of the template
 * @param count Number of units to spawn
 * @param owner Player id of the owner of the new units. By default, the owner
 * of the garrisonholder entity.
 */
TriggerHelper.SpawnGarrisonedUnits = function(entity, template, count, owner)
{
	const entities = [];

	const cmpGarrisonHolder = Engine.QueryInterface(entity, IID_GarrisonHolder);
	if (!cmpGarrisonHolder)
	{
		error("tried to create garrisoned entities inside a non-garrisonholder");
		return entities;
	}

	if (owner == null)
		owner = TriggerHelper.GetOwner(entity);

	for (let i = 0; i < count; ++i)
	{
		const ent = this.AddUpgradeTemplate(owner, template);

		const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
		if (cmpOwnership)
			cmpOwnership.SetOwner(owner);

		const cmpGarrisonable = Engine.QueryInterface(ent, IID_Garrisonable);
		if (cmpGarrisonable && cmpGarrisonable.Garrison(entity))
			entities.push(ent);
		else
			error("failed to garrison entity " + ent + " (" + template + ") inside " + entity);
	}

	return entities;
};

/**
 * Can be used to spawn turreted units on top of a building/ship.
 *
 * @param entity Entity id of the turret holder on which units will be turreted
 * @param template Name of the template
 * @param count Number of units to spawn
 * @param owner Player id of the owner of the new units. By default, the owner
 * of the turretholder entity.
 */
TriggerHelper.SpawnTurretedUnits = function(entity, template, count, owner)
{
	const entities = [];

	const cmpTurretHolder = Engine.QueryInterface(entity, IID_TurretHolder);
	if (!cmpTurretHolder)
	{
		error("tried to create turreted entities inside a non-turretholder");
		return entities;
	}

	if (owner == null)
		owner = TriggerHelper.GetOwner(entity);

	for (let i = 0; i < count; ++i)
	{
		const ent = this.AddUpgradeTemplate(owner, template);

		const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
		if (cmpOwnership)
			cmpOwnership.SetOwner(owner);

		const cmpTurretable = Engine.QueryInterface(ent, IID_Turretable);
		if (cmpTurretable && cmpTurretable.OccupyTurret(entity))
			entities.push(ent);
		else
			error("failed to turret entity " + ent + " (" + template + ") inside " + entity);
	}

	return entities;
};

/**
 * Spawn units from all trigger points with this reference
 * If player is defined, only spaw units from the trigger points
 * that belong to that player
 * @param ref Trigger point reference name to spawn units from
 * @param template Template name
 * @param count Number of spawned entities per Trigger point
 * @param owner Owner of the spawned units. Default: the owner of the origins
 * @return A list of new entities per origin like
 * {originId1: [entId1, entId2], originId2: [entId3, entId4], ...}
 */
TriggerHelper.SpawnUnitsFromTriggerPoints = function(ref, template, count, owner = null)
{
	const cmpTrigger = Engine.QueryInterface(SYSTEM_ENTITY, IID_Trigger);
	const triggerPoints = cmpTrigger.GetTriggerPoints(ref);

	const entities = {};
	for (const point of triggerPoints)
		entities[point] = TriggerHelper.SpawnUnits(point, template, count, owner);

	return entities;
};

/**
 * Returns the resource type that can be gathered from an entity
 */
TriggerHelper.GetResourceType = function(entity)
{
	const cmpResourceSupply = Engine.QueryInterface(entity, IID_ResourceSupply);
	if (!cmpResourceSupply)
		return undefined;

	return cmpResourceSupply.GetType();
};

/**
 * The given player will win the game.
 * If it's not a last man standing game, then allies will win too and others will be defeated.
 *
 * @param {number} playerID - The player who will win.
 * @param {function} victoryReason - Function that maps from number to plural string, for example
 *     n => markForPluralTranslation(
 *         "%(lastPlayer)s has won (game mode).",
 *         "%(players)s and %(lastPlayer)s have won (game mode).",
 *         n));
 * It's a function since we don't know in advance how many players will have won.
 */
TriggerHelper.SetPlayerWon = function(playerID, victoryReason, defeatReason)
{
	const cmpEndGameManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_EndGameManager);
	cmpEndGameManager.MarkPlayerAndAlliesAsWon(playerID, victoryReason, defeatReason);
};

/**
 * Defeats a single player.
 *
 * @param {number} - ID of that player.
 * @param {string} - String to be shown in chat, for example
 *                   markForTranslation("%(player)s has been defeated (objective).")
 */
TriggerHelper.DefeatPlayer = function(playerID, defeatReason)
{
	const cmpPlayer = QueryPlayerIDInterface(playerID);
	if (cmpPlayer)
		cmpPlayer.SetState("defeated", defeatReason);
};

/**
 * Returns the number of players including gaia and defeated ones.
 */
TriggerHelper.GetNumberOfPlayers = function()
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetNumPlayers();
};

/**
 * A function to determine if an entity matches specific classes.
 * See globalscripts/Templates.js for details of MatchesClassList.
 *
 * @param entity - ID of the entity that we want to check for classes.
 * @param classes - List of the classes we are checking if the entity matches.
 */
TriggerHelper.EntityMatchesClassList = function(entity, classes)
{
	const cmpIdentity = Engine.QueryInterface(entity, IID_Identity);
	return cmpIdentity && MatchesClassList(cmpIdentity.GetClassesList(), classes);
};

TriggerHelper.MatchEntitiesByClass = function(entities, classes)
{
	return entities.filter(ent => TriggerHelper.EntityMatchesClassList(ent, classes));
};

TriggerHelper.GetPlayerEntitiesByClass = function(playerID, classes)
{
	return TriggerHelper.MatchEntitiesByClass(TriggerHelper.GetEntitiesByPlayer(playerID), classes);
};

TriggerHelper.GetAllPlayersEntitiesByClass = function(classes)
{
	return TriggerHelper.MatchEntitiesByClass(TriggerHelper.GetAllPlayersEntities(), classes);
};

/**
 * Return valid gaia-owned spawn points on land in neutral territory.
 * If there are none, use those available in player-owned territory.
 */
TriggerHelper.GetLandSpawnPoints = function()
{
	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	const cmpWaterManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_WaterManager);
	const cmpTerritoryManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TerritoryManager);
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);

	const neutralSpawnPoints = [];
	const nonNeutralSpawnPoints = [];

	for (const ent of cmpRangeManager.GetEntitiesByPlayer(0))
	{
		const cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
		const cmpPosition = Engine.QueryInterface(ent, IID_Position);
		if (!cmpIdentity || !cmpPosition || !cmpPosition.IsInWorld())
			continue;

		const templateName = cmpTemplateManager.GetCurrentTemplateName(ent);
		if (!templateName)
			continue;

		const template = cmpTemplateManager.GetTemplate(templateName);
		if (!template || template.UnitMotionFlying)
			continue;

		const pos = cmpPosition.GetPosition();
		if (pos.y <= cmpWaterManager.GetWaterLevel(pos.x, pos.z))
			continue;

		if (cmpTerritoryManager.GetOwner(pos.x, pos.z) == 0)
			neutralSpawnPoints.push(ent);
		else
			nonNeutralSpawnPoints.push(ent);
	}

	return neutralSpawnPoints.length ? neutralSpawnPoints : nonNeutralSpawnPoints;
};

TriggerHelper.HasDealtWithTech = function(playerID, techName)
{
	const cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);
	const playerEnt = cmpPlayerManager.GetPlayerByID(playerID);
	const cmpTechnologyManager = Engine.QueryInterface(playerEnt, IID_TechnologyManager);
	return cmpTechnologyManager && (cmpTechnologyManager.IsTechnologyQueued(techName) ||
	                                cmpTechnologyManager.IsTechnologyResearched(techName));
};

/**
 * Returns all names of templates that match the given identity classes, constrainted to an optional civ.
 *
 * @param {string} classes - See MatchesClassList for the accepted formats, for example "Class1 Class2+!Class3".
 * @param [string] civ - Optionally only retrieve templates of the given civ. Can be left undefined.
 * @param [string] packedState - When retrieving siege engines filter for the "packed" or "unpacked" state
 * @param [string] rank - If given, only return templates that have no or the given rank. For example "Elite".
 * @param [boolean] excludeBarracksVariants - Optionally exclude templates whose name ends with "_barracks"
 */
TriggerHelper.GetTemplateNamesByClasses = function(classes, civ, packedState, rank, excludeBarracksVariants)
{
	const templateNames = [];
	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	for (const templateName of cmpTemplateManager.FindAllTemplates(false))
	{
		if (templateName.startsWith("campaigns/army_"))
			continue;

		if (excludeBarracksVariants && templateName.endsWith("_barracks"))
			continue;

		const template = cmpTemplateManager.GetTemplate(templateName);

		if (civ && (!template.Identity || template.Identity.Civ != civ))
			continue;

		if (!MatchesClassList(GetIdentityClasses(template.Identity), classes))
			continue;

		if (rank && template.Identity.Rank && template.Identity.Rank != rank)
			continue;

		if (packedState && template.Pack && packedState != template.Pack.State)
			continue;

		templateNames.push(templateName);
	}

	return templateNames;
};
/**
 * Composes a random set of the given templates of the given total size.
 *
 * @param {string[]} templateNames - for example ["brit_infantry_javelineer_b", "brit_cavalry_swordsman_e"]
 * @param {number} totalCount - total amount of templates, in this  example 12
 * @returns an object where the keys are template names and values are amounts,
 *          for example { "brit_infantry_javelineer_b": 4, "brit_cavalry_swordsman_e": 8 }
 */
TriggerHelper.RandomTemplateComposition = function(templateNames, totalCount)
{
	const frequencies = templateNames.map(() => randFloat(0, 1));
	const frequencySum = frequencies.reduce((sum, frequency) => sum + frequency, 0);

	let remainder = totalCount;
	const templateCounts = {};

	for (let i = 0; i < templateNames.length; ++i)
	{
		const count = i == templateNames.length - 1 ? remainder : Math.min(remainder, Math.round(frequencies[i] / frequencySum * totalCount));
		if (!count)
			continue;

		templateCounts[templateNames[i]] = count;
		remainder -= count;
	}

	return templateCounts;
};

/**
 * Composes a random set of the given templates so that the sum of templates matches totalCount.
 * For each template array that has a count item, it choses exactly that number of templates at random.
 * The remaining template arrays are chosen depending on the given frequency.
 * If a unique_entities array is given, it will only select the template if none of the given entityIDs
 * already have that entity (useful to let heroes remain unique).
 *
 * @param {Object[]} templateBalancing - for example
 *     [
 *        { "templates": ["template1", "template2"], "frequency": 2 },
 *        { "templates": ["template3"], "frequency": 1 },
 *        { "templates": ["hero1", "hero2"], "unique_entities": [380, 495], "count": 1 }
 *     ]
 * @param {number} totalCount - total amount of templates, for example 5.
 *
 * @returns an object where the keys are template names and values are amounts,
 *    for example { "template1": 1, "template2": 3, "template3": 2, "hero1": 1 }
 */
TriggerHelper.BalancedTemplateComposition = function(templateBalancing, totalCount)
{
	// Remove all unavailable unique templates (heroes) and empty template arrays
	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	const templateBalancingFiltered = [];
	for (const templateBalance of templateBalancing)
	{
		const templateBalanceNew = clone(templateBalance);

		if (templateBalanceNew.unique_entities)
			templateBalanceNew.templates = templateBalanceNew.templates.filter(templateName =>
				templateBalanceNew.unique_entities.every(ent => templateName != cmpTemplateManager.GetCurrentTemplateName(ent)));

		if (templateBalanceNew.templates.length)
			templateBalancingFiltered.push(templateBalanceNew);
	}

	// Helper function to add randomized templates to the result
	let remainder = totalCount;
	const results = {};
	const addTemplates = (templateNames, count) => {
		const templateCounts = TriggerHelper.RandomTemplateComposition(templateNames, count);
		for (const templateName in templateCounts)
		{
			if (!results[templateName])
				results[templateName] = 0;

			results[templateName] += templateCounts[templateName];
			remainder -= templateCounts[templateName];
		}
	};

	// Add template groups with fixed counts
	for (const templateBalance of templateBalancingFiltered)
		if (templateBalance.count)
			addTemplates(templateBalance.templates, Math.min(remainder, templateBalance.count));

	// Add template groups with frequency weights
	const templateBalancingFrequencies = templateBalancingFiltered.filter(templateBalance => !!templateBalance.frequency);
	const templateBalancingFrequencySum = templateBalancingFrequencies.reduce((sum, templateBalance) => sum + templateBalance.frequency, 0);
	for (let i = 0; i < templateBalancingFrequencies.length; ++i)
		addTemplates(
			templateBalancingFrequencies[i].templates,
			i == templateBalancingFrequencies.length - 1 ?
				remainder :
				Math.min(remainder, Math.round(templateBalancingFrequencies[i].frequency / templateBalancingFrequencySum * totalCount)));

	if (remainder != 0)
		warn("Could not choose as many templates as intended, remaining " + remainder + ", chosen: " + uneval(results));

	return results;
};

/**
 * This will spawn random compositions of entities of the given templates at all garrisonholders of the given targetClass of the given player.
 * The garrisonholder will be filled to capacityPercent.
 * Returns an object where keys are entityIDs of the affected garrisonholders and the properties are template compositions, see RandomTemplateComposition.
 */
TriggerHelper.SpawnAndGarrisonAtClasses = function(playerID, classes, templates, capacityPercent)
{
	const results = {};

	for (const entGarrisonHolder of Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetEntitiesByPlayer(playerID))
	{
		const cmpGarrisonHolder = Engine.QueryInterface(entGarrisonHolder, IID_GarrisonHolder);
		if (!cmpGarrisonHolder)
			continue;

		const cmpIdentity = Engine.QueryInterface(entGarrisonHolder, IID_Identity);
		if (!cmpIdentity || !MatchesClassList(cmpIdentity.GetClassesList(), classes))
			continue;

		// TODO: account for already garrisoned entities and garrison size.
		results[entGarrisonHolder] = this.RandomTemplateComposition(templates, Math.floor(cmpGarrisonHolder.GetCapacity() * capacityPercent));

		for (const template in results[entGarrisonHolder])
			TriggerHelper.SpawnGarrisonedUnits(entGarrisonHolder, template, results[entGarrisonHolder][template], playerID);
	}

	return results;
};

/**
 * This will spawn random compositions of entities of the given templates in all turretholders of the given targetClass of the given player.
 * The turretholder will be filled to capacityPercent.
 * Returns an object where keys are entityIDs of the affected turretholders and the properties are template compositions, see RandomTemplateComposition.
 */
TriggerHelper.SpawnAndTurretAtClasses = function(playerID, classes, templates, capacityPercent)
{
	const results = {};

	for (const entTurretHolder of Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetEntitiesByPlayer(playerID))
	{
		const cmpTurretHolder = Engine.QueryInterface(entTurretHolder, IID_TurretHolder);
		if (!cmpTurretHolder)
			continue;

		const cmpIdentity = Engine.QueryInterface(entTurretHolder, IID_Identity);
		if (!cmpIdentity || !MatchesClassList(cmpIdentity.GetClassesList(), classes))
			continue;

		results[entTurretHolder] = this.RandomTemplateComposition(templates, Math.max(Math.floor(cmpTurretHolder.GetTurretPoints().length * capacityPercent - cmpTurretHolder.GetEntities().length), 0));

		for (const template in results[entTurretHolder])
			TriggerHelper.SpawnTurretedUnits(entTurretHolder, template, results[entTurretHolder][template], playerID);
	}

	return results;
};

Engine.RegisterGlobal("TriggerHelper", TriggerHelper);
