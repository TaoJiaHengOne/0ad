function GuiInterface() {}

GuiInterface.prototype.Schema =
	"<a:component type='system'/><empty/>";

GuiInterface.prototype.WHITE = { "r": 1, "g": 1, "b": 1 };

GuiInterface.prototype.Serialize = function()
{
	// This component isn't network-synchronized for the biggest part,
	// so most of the attributes shouldn't be serialized.
	// Return an object with a small selection of deterministic data.
	return {
		"timeNotifications": this.timeNotifications,
		"timeNotificationID": this.timeNotificationID
	};
};

GuiInterface.prototype.Deserialize = function(data)
{
	this.Init();
	this.timeNotifications = data.timeNotifications;
	this.timeNotificationID = data.timeNotificationID;
};

GuiInterface.prototype.Init = function()
{
	this.placementEntity = undefined; // = undefined or [templateName, entityID]
	this.placementWallEntities = undefined;
	this.placementWallLastAngle = 0;
	this.notifications = [];
	this.renamedEntities = [];
	this.miragedEntities = [];
	this.timeNotificationID = 1;
	this.timeNotifications = [];
	this.entsRallyPointsDisplayed = [];
	this.entsWithAuraAndStatusBars = new Set();
	this.enabledVisualRangeOverlayTypes = {};
	this.templateModified = {};
	this.selectionDirty = {};
	this.obstructionSnap = new ObstructionSnap();
};

/*
 * All of the functions defined below are called via Engine.GuiInterfaceCall(name, arg)
 * from GUI scripts, and executed here with arguments (player, arg).
 *
 * CAUTION: The input to the functions in this module is not network-synchronised, so it
 * mustn't affect the simulation state (i.e. the data that is serialised and can affect
 * the behaviour of the rest of the simulation) else it'll cause out-of-sync errors.
 */

/**
 * Returns global information about the current game state.
 * This is used by the GUI and also by AI scripts.
 */
GuiInterface.prototype.GetSimulationState = function()
{
	const ret = {
		"players": []
	};

	const cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);
	const numPlayers = cmpPlayerManager.GetNumPlayers();
	for (let i = 0; i < numPlayers; ++i)
	{
		const playerEnt = cmpPlayerManager.GetPlayerByID(i);
		const cmpPlayer = Engine.QueryInterface(playerEnt, IID_Player);
		const cmpPlayerEntityLimits = Engine.QueryInterface(playerEnt, IID_EntityLimits);
		const cmpIdentity = Engine.QueryInterface(playerEnt, IID_Identity);
		const cmpDiplomacy = Engine.QueryInterface(playerEnt, IID_Diplomacy);

		// Work out which phase we are in.
		let phase = "";
		const cmpTechnologyManager = Engine.QueryInterface(playerEnt, IID_TechnologyManager);
		if (cmpTechnologyManager)
		{
			if (cmpTechnologyManager.IsTechnologyResearched("phase_city"))
				phase = "city";
			else if (cmpTechnologyManager.IsTechnologyResearched("phase_town"))
				phase = "town";
			else if (cmpTechnologyManager.IsTechnologyResearched("phase_village"))
				phase = "village";
		}

		const allies = [];
		const mutualAllies = [];
		const neutrals = [];
		const enemies = [];

		for (let j = 0; j < numPlayers; ++j)
		{
			allies[j] = cmpDiplomacy.IsAlly(j);
			mutualAllies[j] = cmpDiplomacy.IsMutualAlly(j);
			neutrals[j] = cmpDiplomacy.IsNeutral(j);
			enemies[j] = cmpDiplomacy.IsEnemy(j);
		}

		ret.players.push({
			"name": cmpIdentity.GetName(),
			"civ": cmpIdentity.GetCiv(),
			"color": cmpPlayer.GetColor(),
			"entity": cmpPlayer.entity,
			"controlsAll": cmpPlayer.CanControlAllUnits(),
			"popCount": cmpPlayer.GetPopulationCount(),
			"popLimit": cmpPlayer.GetPopulationLimit(),
			"popMax": cmpPlayer.GetMaxPopulation(),
			"panelEntities": cmpPlayer.GetPanelEntities(),
			"resourceCounts": cmpPlayer.GetResourceCounts(),
			"resourceGatherers": cmpPlayer.GetResourceGatherers(),
			"trainingBlocked": cmpPlayer.IsTrainingBlocked(),
			"state": cmpPlayer.GetState(),
			"team": cmpDiplomacy.GetTeam(),
			"teamLocked": cmpDiplomacy.IsTeamLocked(),
			"cheatsEnabled": cmpPlayer.GetCheatsEnabled(),
			"disabledTemplates": cmpPlayer.GetDisabledTemplates(),
			"disabledTechnologies": cmpPlayer.GetDisabledTechnologies(),
			"hasSharedDropsites": cmpDiplomacy.HasSharedDropsites(),
			"hasSharedLos": cmpDiplomacy.HasSharedLos(),
			"spyCostMultiplier": cmpPlayer.GetSpyCostMultiplier(),
			"phase": phase,
			"isAlly": allies,
			"isMutualAlly": mutualAllies,
			"isNeutral": neutrals,
			"isEnemy": enemies,
			"entityLimits": cmpPlayerEntityLimits ? cmpPlayerEntityLimits.GetLimits() : null,
			"entityCounts": cmpPlayerEntityLimits ? cmpPlayerEntityLimits.GetCounts() : null,
			"matchEntityCounts": cmpPlayerEntityLimits ? cmpPlayerEntityLimits.GetMatchCounts() : null,
			"entityLimitChangers": cmpPlayerEntityLimits ? cmpPlayerEntityLimits.GetLimitChangers() : null,
			"researchQueued": cmpTechnologyManager ? cmpTechnologyManager.GetQueuedResearch() : null,
			"researchedTechs": cmpTechnologyManager ? cmpTechnologyManager.GetResearchedTechs() : null,
			"classCounts": cmpTechnologyManager ? cmpTechnologyManager.GetClassCounts() : null,
			"typeCountsByClass": cmpTechnologyManager ? cmpTechnologyManager.GetTypeCountsByClass() : null,
			"canBarter": cmpPlayer.CanBarter(),
			"barterPrices": Engine.QueryInterface(SYSTEM_ENTITY, IID_Barter).GetPrices(cmpPlayer)
		});
	}

	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	if (cmpRangeManager)
		ret.circularMap = cmpRangeManager.GetLosCircular();

	const cmpTerrain = Engine.QueryInterface(SYSTEM_ENTITY, IID_Terrain);
	if (cmpTerrain)
		ret.mapSize = cmpTerrain.GetMapSize();

	const cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	ret.timeElapsed = cmpTimer.GetTime();

	const cmpCeasefireManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_CeasefireManager);
	if (cmpCeasefireManager)
	{
		ret.ceasefireActive = cmpCeasefireManager.IsCeasefireActive();
		ret.ceasefireTimeRemaining = ret.ceasefireActive ? cmpCeasefireManager.GetCeasefireStartedTime() + cmpCeasefireManager.GetCeasefireTime() - ret.timeElapsed : 0;
	}

	const cmpCinemaManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_CinemaManager);
	if (cmpCinemaManager)
		ret.cinemaPlaying = cmpCinemaManager.IsPlaying();

	const cmpEndGameManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_EndGameManager);
	ret.victoryConditions = cmpEndGameManager.GetVictoryConditions();
	ret.alliedVictory = cmpEndGameManager.GetAlliedVictory();

	const cmpPopulationCapManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PopulationCapManager);
	ret.populationCapType = cmpPopulationCapManager.GetPopulationCapType();
	ret.populationCap = cmpPopulationCapManager.GetPopulationCap();

	for (let i = 0; i < numPlayers; ++i)
	{
		const cmpPlayerStatisticsTracker = QueryPlayerIDInterface(i, IID_StatisticsTracker);
		if (cmpPlayerStatisticsTracker)
			ret.players[i].statistics = cmpPlayerStatisticsTracker.GetBasicStatistics();
	}

	return ret;
};

/**
 * Returns global information about the current game state, plus statistics.
 * This is used by the GUI at the end of a game, in the summary screen.
 * Note: Amongst statistics, the team exploration map percentage is computed from
 * scratch, so the extended simulation state should not be requested too often.
 */
GuiInterface.prototype.GetExtendedSimulationState = function()
{
	const ret = this.GetSimulationState();

	const numPlayers = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetNumPlayers();
	for (let i = 0; i < numPlayers; ++i)
	{
		const cmpPlayerStatisticsTracker = QueryPlayerIDInterface(i, IID_StatisticsTracker);
		if (cmpPlayerStatisticsTracker)
			ret.players[i].sequences = cmpPlayerStatisticsTracker.GetSequences();
	}

	return ret;
};

/**
 * Returns the gamesettings that were chosen at the time the match started.
 */
GuiInterface.prototype.GetInitAttributes = function()
{
	return InitAttributes;
};

/**
 * This data will be stored in the replay metadata file after a match has been finished recording.
 */
GuiInterface.prototype.GetReplayMetadata = function()
{
	const extendedSimState = this.GetExtendedSimulationState();
	return {
		"timeElapsed": extendedSimState.timeElapsed,
		"playerStates": extendedSimState.players,
		"mapSettings": InitAttributes.settings
	};
};

/**
 * Called when the game ends if the current game is part of a campaign run.
 */
GuiInterface.prototype.GetCampaignGameEndData = function(player)
{
	const cmpTrigger = Engine.QueryInterface(SYSTEM_ENTITY, IID_Trigger);
	if (Trigger.prototype.OnCampaignGameEnd)
		return Trigger.prototype.OnCampaignGameEnd();
	return {};
};

GuiInterface.prototype.GetRenamedEntities = function(player)
{
	if (this.miragedEntities[player])
		return this.renamedEntities.concat(this.miragedEntities[player]);

	return this.renamedEntities;
};

GuiInterface.prototype.ClearRenamedEntities = function()
{
	this.renamedEntities = [];
	this.miragedEntities = [];
};

GuiInterface.prototype.AddMiragedEntity = function(player, entity, mirage)
{
	if (!this.miragedEntities[player])
		this.miragedEntities[player] = [];

	this.miragedEntities[player].push({ "entity": entity, "newentity": mirage });
};

/**
 * Get common entity info, often used in the gui.
 */
GuiInterface.prototype.GetEntityState = function(player, ent)
{
	if (!ent)
		return null;

	// All units must have a template; if not then it's a nonexistent entity id.
	const template = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager).GetCurrentTemplateName(ent);
	if (!template)
		return null;

	const ret = {
		"id": ent,
		"player": INVALID_PLAYER,
		"template": template
	};

	const cmpAuras = Engine.QueryInterface(ent, IID_Auras);
	if (cmpAuras)
		ret.auras = cmpAuras.GetDescriptions();

	const cmpMirage = Engine.QueryInterface(ent, IID_Mirage);
	if (cmpMirage)
		ret.mirage = true;

	const cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
	if (cmpIdentity)
		ret.identity = {
			"rank": cmpIdentity.GetRank(),
			"rankTechName": cmpIdentity.GetRankTechName(),
			"classes": cmpIdentity.GetClassesList(),
			"selectionGroupName": cmpIdentity.GetSelectionGroupName(),
			"canDelete": !cmpIdentity.IsUndeletable(),
			"controllable": cmpIdentity.IsControllable()
		};

	const cmpFormation = Engine.QueryInterface(ent, IID_Formation);
	if (cmpFormation)
		ret.formation = {
			"members": cmpFormation.GetMembers()
		};

	const cmpPosition = Engine.QueryInterface(ent, IID_Position);
	if (cmpPosition && cmpPosition.IsInWorld())
		ret.position = cmpPosition.GetPosition();

	const cmpHealth = QueryMiragedInterface(ent, IID_Health);
	if (cmpHealth)
	{
		ret.hitpoints = cmpHealth.GetHitpoints();
		ret.maxHitpoints = cmpHealth.GetMaxHitpoints();
		ret.needsRepair = cmpHealth.IsRepairable() && cmpHealth.IsInjured();
		ret.needsHeal = !cmpHealth.IsUnhealable();
	}

	const cmpCapturable = QueryMiragedInterface(ent, IID_Capturable);
	if (cmpCapturable)
	{
		ret.capturePoints = cmpCapturable.GetCapturePoints();
		ret.maxCapturePoints = cmpCapturable.GetMaxCapturePoints();
	}

	const cmpBuilder = Engine.QueryInterface(ent, IID_Builder);
	if (cmpBuilder)
		ret.builder = true;

	const cmpMarket = QueryMiragedInterface(ent, IID_Market);
	if (cmpMarket)
		ret.market = {
			"land": cmpMarket.HasType("land"),
			"naval": cmpMarket.HasType("naval")
		};

	const cmpPack = Engine.QueryInterface(ent, IID_Pack);
	if (cmpPack)
		ret.pack = {
			"packed": cmpPack.IsPacked(),
			"progress": cmpPack.GetProgress()
		};

	const cmpPopulation = Engine.QueryInterface(ent, IID_Population);
	if (cmpPopulation)
		ret.population = {
			"bonus": cmpPopulation.GetPopBonus()
		};

	const cmpUpgrade = Engine.QueryInterface(ent, IID_Upgrade);
	if (cmpUpgrade)
		ret.upgrade = {
			"upgrades": cmpUpgrade.GetUpgrades(),
			"progress": cmpUpgrade.GetProgress(),
			"template": cmpUpgrade.GetUpgradingTo(),
			"isUpgrading": cmpUpgrade.IsUpgrading()
		};

	const cmpResearcher = Engine.QueryInterface(ent, IID_Researcher);
	if (cmpResearcher)
		ret.researcher = {
			"technologies": cmpResearcher.GetTechnologiesList(),
			"techCostMultiplier": cmpResearcher.GetTechCostMultiplier()
		};

	const cmpStatusEffects = Engine.QueryInterface(ent, IID_StatusEffectsReceiver);
	if (cmpStatusEffects)
		ret.statusEffects = cmpStatusEffects.GetActiveStatuses();

	const cmpProductionQueue = Engine.QueryInterface(ent, IID_ProductionQueue);
	if (cmpProductionQueue)
		ret.production = {
			"queue": cmpProductionQueue.GetQueue(),
			"autoqueue": cmpProductionQueue.IsAutoQueueing()
		};

	const cmpTrainer = Engine.QueryInterface(ent, IID_Trainer);
	if (cmpTrainer)
		ret.trainer = {
			"entities": cmpTrainer.GetEntitiesList()
		};

	const cmpTrader = Engine.QueryInterface(ent, IID_Trader);
	if (cmpTrader)
		ret.trader = {
			"goods": cmpTrader.GetGoods()
		};

	const cmpFoundation = QueryMiragedInterface(ent, IID_Foundation);
	if (cmpFoundation)
		ret.foundation = {
			"numBuilders": cmpFoundation.GetNumBuilders(),
			"buildTime": cmpFoundation.GetBuildTime()
		};

	const cmpRepairable = QueryMiragedInterface(ent, IID_Repairable);
	if (cmpRepairable)
		ret.repairable = {
			"numBuilders": cmpRepairable.GetNumBuilders(),
			"buildTime": cmpRepairable.GetBuildTime()
		};

	const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
	if (cmpOwnership)
		ret.player = cmpOwnership.GetOwner();

	const cmpRallyPoint = Engine.QueryInterface(ent, IID_RallyPoint);
	if (cmpRallyPoint)
		ret.rallyPoint = { "position": cmpRallyPoint.GetPositions()[0] }; // undefined or {x,z} object

	const cmpGarrisonHolder = Engine.QueryInterface(ent, IID_GarrisonHolder);
	if (cmpGarrisonHolder)
		ret.garrisonHolder = {
			"entities": cmpGarrisonHolder.GetEntities(),
			"buffHeal": cmpGarrisonHolder.GetHealRate(),
			"allowedClasses": cmpGarrisonHolder.GetAllowedClasses(),
			"capacity": cmpGarrisonHolder.GetCapacity(),
			"occupiedSlots": cmpGarrisonHolder.OccupiedSlots()
		};

	const cmpTurretHolder = Engine.QueryInterface(ent, IID_TurretHolder);
	if (cmpTurretHolder)
		ret.turretHolder = {
			"turretPoints": cmpTurretHolder.GetTurretPoints()
		};

	const cmpTurretable = Engine.QueryInterface(ent, IID_Turretable);
	if (cmpTurretable)
		ret.turretable = {
			"ejectable": cmpTurretable.IsEjectable(),
			"holder": cmpTurretable.HolderID()
		};

	const cmpGarrisonable = Engine.QueryInterface(ent, IID_Garrisonable);
	if (cmpGarrisonable)
		ret.garrisonable = {
			"holder": cmpGarrisonable.HolderID(),
			"size": cmpGarrisonable.UnitSize()
		};

	const cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
	if (cmpUnitAI)
		ret.unitAI = {
			"state": cmpUnitAI.GetCurrentState(),
			"orders": cmpUnitAI.GetOrders(),
			"hasWorkOrders": cmpUnitAI.HasWorkOrders(),
			"canGuard": cmpUnitAI.CanGuard(),
			"isGuarding": cmpUnitAI.IsGuardOf(),
			"canPatrol": cmpUnitAI.CanPatrol(),
			"selectableStances": cmpUnitAI.GetSelectableStances(),
			"isIdle": cmpUnitAI.IsIdle(),
			"formations": cmpUnitAI.GetFormationsList(),
			"formation": cmpUnitAI.GetFormationController()
		};

	const cmpGuard = Engine.QueryInterface(ent, IID_Guard);
	if (cmpGuard)
		ret.guard = {
			"entities": cmpGuard.GetEntities()
		};

	const cmpResourceGatherer = Engine.QueryInterface(ent, IID_ResourceGatherer);
	if (cmpResourceGatherer)
	{
		ret.resourceCarrying = cmpResourceGatherer.GetCarryingStatus();
		ret.resourceGatherRates = cmpResourceGatherer.GetGatherRates();
	}

	const cmpGate = Engine.QueryInterface(ent, IID_Gate);
	if (cmpGate)
		ret.gate = {
			"locked": cmpGate.IsLocked()
		};

	const cmpAlertRaiser = Engine.QueryInterface(ent, IID_AlertRaiser);
	if (cmpAlertRaiser)
		ret.alertRaiser = {
			"classes": cmpAlertRaiser.GetTargetClasses()
		};

	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	ret.visibility = cmpRangeManager.GetLosVisibility(ent, player);

	const cmpAttack = Engine.QueryInterface(ent, IID_Attack);
	if (cmpAttack)
	{
		const types = cmpAttack.GetAttackTypes();
		if (types.length)
			ret.attack = {};

		for (const type of types)
		{
			ret.attack[type] = {};

			Object.assign(ret.attack[type], cmpAttack.GetAttackEffectsData(type));

			ret.attack[type].attackName = cmpAttack.GetAttackName(type);

			ret.attack[type].splash = cmpAttack.GetSplashData(type);
			if (ret.attack[type].splash)
				Object.assign(ret.attack[type].splash, cmpAttack.GetAttackEffectsData(type, true));

			const range = cmpAttack.GetRange(type);
			ret.attack[type].minRange = range.min;
			ret.attack[type].maxRange = range.max;
			ret.attack[type].yOrigin = cmpAttack.GetAttackYOrigin(type);

			const timers = cmpAttack.GetTimers(type);
			ret.attack[type].prepareTime = timers.prepare;
			ret.attack[type].repeatTime = timers.repeat;

			if (type != "Ranged")
			{
				ret.attack[type].elevationAdaptedRange = ret.attack.maxRange;
				continue;
			}

			if (cmpPosition && cmpPosition.IsInWorld())
				// For units, take the range in front of it, no spread, so angle = 0,
				// else, take the average elevation around it: angle = 2 * pi.
				ret.attack[type].elevationAdaptedRange = cmpRangeManager.GetElevationAdaptedRange(cmpPosition.GetPosition(), cmpPosition.GetRotation(), range.max, ret.attack[type].yOrigin, cmpUnitAI ? 0 : 2 * Math.PI);
			else
				// Not in world, set a default?
				ret.attack[type].elevationAdaptedRange = ret.attack.maxRange;
		}
	}

	const cmpResistance = QueryMiragedInterface(ent, IID_Resistance);
	if (cmpResistance)
		ret.resistance = cmpResistance.GetResistanceOfForm(cmpFoundation ? "Foundation" : "Entity");

	const cmpBuildingAI = Engine.QueryInterface(ent, IID_BuildingAI);
	if (cmpBuildingAI)
		ret.buildingAI = {
			"defaultArrowCount": cmpBuildingAI.GetDefaultArrowCount(),
			"maxArrowCount": cmpBuildingAI.GetMaxArrowCount(),
			"garrisonArrowMultiplier": cmpBuildingAI.GetGarrisonArrowMultiplier(),
			"garrisonArrowClasses": cmpBuildingAI.GetGarrisonArrowClasses(),
			"arrowCount": cmpBuildingAI.GetArrowCount()
		};

	if (cmpPosition && cmpPosition.GetTurretParent() != INVALID_ENTITY)
		ret.turretParent = cmpPosition.GetTurretParent();

	const cmpResourceSupply = QueryMiragedInterface(ent, IID_ResourceSupply);
	if (cmpResourceSupply)
		ret.resourceSupply = {
			"isInfinite": cmpResourceSupply.IsInfinite(),
			"max": cmpResourceSupply.GetMaxAmount(),
			"amount": cmpResourceSupply.GetCurrentAmount(),
			"type": cmpResourceSupply.GetType(),
			"killBeforeGather": cmpResourceSupply.GetKillBeforeGather(),
			"maxGatherers": cmpResourceSupply.GetMaxGatherers(),
			"numGatherers": cmpResourceSupply.GetNumGatherers()
		};

	const cmpResourceDropsite = Engine.QueryInterface(ent, IID_ResourceDropsite);
	if (cmpResourceDropsite)
		ret.resourceDropsite = {
			"types": cmpResourceDropsite.GetTypes(),
			"sharable": cmpResourceDropsite.IsSharable(),
			"shared": cmpResourceDropsite.IsShared()
		};

	const cmpPromotion = Engine.QueryInterface(ent, IID_Promotion);
	if (cmpPromotion)
		ret.promotion = {
			"curr": cmpPromotion.GetCurrentXp(),
			"req": cmpPromotion.GetRequiredXp()
		};

	if (!cmpFoundation && cmpIdentity && cmpIdentity.HasClass("Barter"))
		ret.isBarterMarket = true;

	const cmpHeal = Engine.QueryInterface(ent, IID_Heal);
	if (cmpHeal)
		ret.heal = {
			"health": cmpHeal.GetHealth(),
			"range": cmpHeal.GetRange().max,
			"interval": cmpHeal.GetInterval(),
			"unhealableClasses": cmpHeal.GetUnhealableClasses(),
			"healableClasses": cmpHeal.GetHealableClasses()
		};

	const cmpLoot = Engine.QueryInterface(ent, IID_Loot);
	if (cmpLoot)
	{
		ret.loot = cmpLoot.GetResources();
		ret.loot.xp = cmpLoot.GetXp();
	}

	const cmpResourceTrickle = Engine.QueryInterface(ent, IID_ResourceTrickle);
	if (cmpResourceTrickle)
		ret.resourceTrickle = {
			"interval": cmpResourceTrickle.GetInterval(),
			"rates": cmpResourceTrickle.GetRates()
		};

	const cmpTreasure = Engine.QueryInterface(ent, IID_Treasure);
	if (cmpTreasure)
		ret.treasure = {
			"collectTime": cmpTreasure.CollectionTime(),
			"resources": cmpTreasure.Resources()
		};

	const cmpTreasureCollector = Engine.QueryInterface(ent, IID_TreasureCollector);
	if (cmpTreasureCollector)
		ret.treasureCollector = true;

	const cmpUnitMotion = Engine.QueryInterface(ent, IID_UnitMotion);
	if (cmpUnitMotion)
		ret.speed = {
			"walk": cmpUnitMotion.GetWalkSpeed(),
			"run": cmpUnitMotion.GetWalkSpeed() * cmpUnitMotion.GetRunMultiplier(),
			"acceleration": cmpUnitMotion.GetAcceleration()
		};

	const cmpUpkeep = Engine.QueryInterface(ent, IID_Upkeep);
	if (cmpUpkeep)
		ret.upkeep = {
			"interval": cmpUpkeep.GetInterval(),
			"rates": cmpUpkeep.GetRates()
		};

	return ret;
};

GuiInterface.prototype.GetMultipleEntityStates = function(player, ents)
{
	return ents.map(ent => ({ "entId": ent, "state": this.GetEntityState(player, ent) }));
};

GuiInterface.prototype.GetAverageRangeForBuildings = function(player, cmd)
{
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	const cmpTerrain = Engine.QueryInterface(SYSTEM_ENTITY, IID_Terrain);

	const rot = { "x": 0, "y": 0, "z": 0 };
	const pos = {
		"x": cmd.x,
		"y": cmpTerrain.GetGroundLevel(cmd.x, cmd.z),
		"z": cmd.z
	};

	const yOrigin = cmd.yOrigin || 0;
	const range = cmd.range;

	return cmpRangeManager.GetElevationAdaptedRange(pos, rot, range, yOrigin, 2 * Math.PI);
};

GuiInterface.prototype.GetTemplateData = function(player, data)
{
	const templateName = data.templateName;
	const owner = data.player !== undefined ? data.player : player;
	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	const template = cmpTemplateManager.GetTemplate(templateName);

	if (!template)
		return null;

	const aurasTemplate = {};

	if (!template.Auras)
		return GetTemplateDataHelper(template, owner, aurasTemplate, Resources);

	const auraNames = template.Auras._string.split(/\s+/);

	for (const name of auraNames)
	{
		const auraTemplate = AuraTemplates.Get(name);
		if (!auraTemplate)
			error("Template " + templateName + " has undefined aura " + name);
		else
			aurasTemplate[name] = auraTemplate;
	}

	return GetTemplateDataHelper(template, owner, aurasTemplate, Resources);
};

GuiInterface.prototype.AreRequirementsMet = function(player, data)
{
	return !data.requirements || RequirementsHelper.AreRequirementsMet(data.requirements,
		data.player !== undefined ? data.player : player);
};

/**
 * Checks whether the requirements for this technology have been met.
 */
GuiInterface.prototype.CheckTechnologyRequirements = function(player, data)
{
	const cmpTechnologyManager = QueryPlayerIDInterface(data.player !== undefined ? data.player : player, IID_TechnologyManager);
	if (!cmpTechnologyManager)
		return false;

	return cmpTechnologyManager.CanResearch(data.tech);
};

/**
 * Returns technologies that are being actively researched, along with
 * which entity is researching them and how far along the research is.
 */
GuiInterface.prototype.GetStartedResearch = function(player)
{
	return QueryPlayerIDInterface(player, IID_TechnologyManager)?.GetBasicInfoOfStartedTechs() || {};
};

/**
 * Returns the battle state of the player.
 */
GuiInterface.prototype.GetBattleState = function(player)
{
	const cmpBattleDetection = QueryPlayerIDInterface(player, IID_BattleDetection);
	if (!cmpBattleDetection)
		return false;

	return cmpBattleDetection.GetState();
};

/**
 * Returns a list of ongoing attacks against the player.
 */
GuiInterface.prototype.GetIncomingAttacks = function(player)
{
	const cmpAttackDetection = QueryPlayerIDInterface(player, IID_AttackDetection);
	if (!cmpAttackDetection)
		return [];

	return cmpAttackDetection.GetIncomingAttacks();
};

/**
 * Used to show a red square over GUI elements you can't yet afford.
 */
GuiInterface.prototype.GetNeededResources = function(player, data)
{
	const cmpPlayer = QueryPlayerIDInterface(data.player !== undefined ? data.player : player);
	return cmpPlayer ? cmpPlayer.GetNeededResources(data.cost) : {};
};

/**
 * State of the templateData (player dependent): true when some template values have been modified
 * and need to be reloaded by the gui.
 */
GuiInterface.prototype.OnTemplateModification = function(msg)
{
	this.templateModified[msg.player] = true;
	this.selectionDirty[msg.player] = true;
};

GuiInterface.prototype.IsTemplateModified = function(player)
{
	return this.templateModified[player] || false;
};

GuiInterface.prototype.ResetTemplateModified = function()
{
	this.templateModified = {};
};

/**
 * Some changes may require an update to the selection panel,
 * which is cached for efficiency. Inform the GUI it needs reloading.
 */
GuiInterface.prototype.OnDisabledTemplatesChanged = function(msg)
{
	this.selectionDirty[msg.player] = true;
};

GuiInterface.prototype.OnDisabledTechnologiesChanged = function(msg)
{
	this.selectionDirty[msg.player] = true;
};

GuiInterface.prototype.SetSelectionDirty = function(player)
{
	this.selectionDirty[player] = true;
};

GuiInterface.prototype.IsSelectionDirty = function(player)
{
	return this.selectionDirty[player] || false;
};

GuiInterface.prototype.ResetSelectionDirty = function()
{
	this.selectionDirty = {};
};

/**
 * Add a timed notification.
 * Warning: timed notifacations are serialised
 * (to also display them on saved games or after a rejoin)
 * so they should allways be added and deleted in a deterministic way.
 */
GuiInterface.prototype.AddTimeNotification = function(notification, duration = 10000)
{
	const cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	notification.endTime = duration + cmpTimer.GetTime();
	notification.id = ++this.timeNotificationID;

	// Let all players and observers receive the notification by default.
	if (!notification.players)
	{
		notification.players = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetAllPlayers();
		notification.players[0] = -1;
	}

	this.timeNotifications.push(notification);
	this.timeNotifications.sort((n1, n2) => n2.endTime - n1.endTime);

	cmpTimer.SetTimeout(this.entity, IID_GuiInterface, "DeleteTimeNotification", duration, this.timeNotificationID);

	return this.timeNotificationID;
};

GuiInterface.prototype.DeleteTimeNotification = function(notificationID)
{
	this.timeNotifications = this.timeNotifications.filter(n => n.id != notificationID);
};

GuiInterface.prototype.GetTimeNotifications = function(player)
{
	const time = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer).GetTime();
	// Filter on players and time, since the delete timer might be executed with a delay.
	return this.timeNotifications.filter(n => n.players.indexOf(player) != -1 && n.endTime > time);
};

GuiInterface.prototype.PushNotification = function(notification)
{
	if (!notification.type || notification.type == "text")
		this.AddTimeNotification(notification);
	else
		this.notifications.push(notification);
};

GuiInterface.prototype.GetNotifications = function()
{
	const n = this.notifications;
	this.notifications = [];
	return n;
};

GuiInterface.prototype.GetAvailableFormations = function(player, wantedPlayer)
{
	const cmpPlayer = QueryPlayerIDInterface(wantedPlayer);
	if (!cmpPlayer)
		return [];

	return cmpPlayer.GetFormations();
};

GuiInterface.prototype.GetFormationRequirements = function(player, data)
{
	return GetFormationRequirements(data.formationTemplate);
};

GuiInterface.prototype.CanMoveEntsIntoFormation = function(player, data)
{
	return CanMoveEntsIntoFormation(data.ents, data.formationTemplate);
};

GuiInterface.prototype.GetFormationInfoFromTemplate = function(player, data)
{
	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	const template = cmpTemplateManager.GetTemplate(data.templateName);

	if (!template || !template.Formation)
		return {};

	return {
		"name": template.Identity.GenericName,
		"tooltip": template.Identity.Tooltip || "",
		"disabledTooltip": template.Formation.DisabledTooltip || "",
		"icon": template.Identity.Icon
	};
};

GuiInterface.prototype.IsFormationSelected = function(player, data)
{
	return data.ents.some(ent => {
		const cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
		return cmpUnitAI && cmpUnitAI.GetFormationTemplate() == data.formationTemplate;
	});
};

GuiInterface.prototype.IsStanceSelected = function(player, data)
{
	for (const ent of data.ents)
	{
		const cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
		if (cmpUnitAI && cmpUnitAI.GetStanceName() == data.stance)
			return true;
	}
	return false;
};

GuiInterface.prototype.GetAllBuildableEntities = function(player, cmd)
{
	const buildableEnts = [];
	for (const ent of cmd.entities)
	{
		const cmpBuilder = Engine.QueryInterface(ent, IID_Builder);
		if (!cmpBuilder)
			continue;

		for (const building of cmpBuilder.GetEntitiesList())
			if (buildableEnts.indexOf(building) == -1)
				buildableEnts.push(building);
	}
	return buildableEnts;
};

GuiInterface.prototype.UpdateDisplayedPlayerColors = function(player, data)
{
	const updateEntityColor = (iids, entities) => {
		for (const ent of entities)
			for (const iid of iids)
			{
				const cmp = Engine.QueryInterface(ent, iid);
				if (cmp)
					cmp.UpdateColor();
			}
	};

	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	const numPlayers = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetNumPlayers();
	for (let i = 1; i < numPlayers; ++i)
	{
		const cmpDiplomacy = QueryPlayerIDInterface(i, IID_Diplomacy);
		if (!cmpDiplomacy)
			continue;

		QueryPlayerIDInterface(i, IID_Player).SetDisplayDiplomacyColor(data.displayDiplomacyColors);
		if (data.displayDiplomacyColors)
			cmpDiplomacy.SetDiplomacyColor(data.displayedPlayerColors[i]);

		updateEntityColor(data.showAllStatusBars && (i == player || player == -1) ?
			[IID_Minimap, IID_RangeOverlayRenderer, IID_RallyPointRenderer, IID_StatusBars] :
			[IID_Minimap, IID_RangeOverlayRenderer, IID_RallyPointRenderer],
		cmpRangeManager.GetEntitiesByPlayer(i));
	}
	updateEntityColor([IID_Selectable, IID_StatusBars], data.selected);
	Engine.QueryInterface(SYSTEM_ENTITY, IID_TerritoryManager).UpdateColors();
};

GuiInterface.prototype.SetSelectionHighlight = function(player, cmd)
{
	// Cache of owner -> color map
	const playerColors = {};

	for (const ent of cmd.entities)
	{
		const cmpSelectable = Engine.QueryInterface(ent, IID_Selectable);
		if (!cmpSelectable)
			continue;

		// Find the entity's owner's color.
		let owner = INVALID_PLAYER;
		const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
		if (cmpOwnership)
			owner = cmpOwnership.GetOwner();

		let color = playerColors[owner];
		if (!color)
		{
			color = QueryPlayerIDInterface(owner, IID_Player)?.GetDisplayedColor() || this.WHITE;
			playerColors[owner] = color;
		}

		cmpSelectable.SetSelectionHighlight({ "r": color.r, "g": color.g, "b": color.b, "a": cmd.alpha }, cmd.selected);

		const cmpRangeOverlayManager = Engine.QueryInterface(ent, IID_RangeOverlayManager);
		if (!cmpRangeOverlayManager || player != owner && player != INVALID_PLAYER)
			continue;

		cmpRangeOverlayManager.SetEnabled(cmd.selected, this.enabledVisualRangeOverlayTypes, false);
	}
};

GuiInterface.prototype.EnableVisualRangeOverlayType = function(player, data)
{
	this.enabledVisualRangeOverlayTypes[data.type] = data.enabled;
};

GuiInterface.prototype.GetEntitiesWithStatusBars = function()
{
	return Array.from(this.entsWithAuraAndStatusBars);
};

GuiInterface.prototype.SetStatusBars = function(player, cmd)
{
	const affectedEnts = new Set();
	for (const ent of cmd.entities)
	{
		const cmpStatusBars = Engine.QueryInterface(ent, IID_StatusBars);
		if (!cmpStatusBars)
			continue;
		cmpStatusBars.SetEnabled(cmd.enabled, cmd.showRank, cmd.showExperience);

		const cmpAuras = Engine.QueryInterface(ent, IID_Auras);
		if (!cmpAuras)
			continue;

		for (const name of cmpAuras.GetAuraNames())
		{
			if (!cmpAuras.GetOverlayIcon(name))
				continue;
			for (const e of cmpAuras.GetAffectedEntities(name))
				affectedEnts.add(e);
			if (cmd.enabled)
				this.entsWithAuraAndStatusBars.add(ent);
			else
				this.entsWithAuraAndStatusBars.delete(ent);
		}
	}

	for (const ent of affectedEnts)
	{
		const cmpStatusBars = Engine.QueryInterface(ent, IID_StatusBars);
		if (cmpStatusBars)
			cmpStatusBars.RegenerateSprites();
	}
};

GuiInterface.prototype.SetRangeOverlays = function(player, cmd)
{
	for (const ent of cmd.entities)
	{
		const cmpRangeOverlayManager = Engine.QueryInterface(ent, IID_RangeOverlayManager);
		if (cmpRangeOverlayManager)
			cmpRangeOverlayManager.SetEnabled(cmd.enabled, this.enabledVisualRangeOverlayTypes, true);
	}
};

GuiInterface.prototype.GetPlayerEntities = function(player)
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetEntitiesByPlayer(player);
};

GuiInterface.prototype.GetNonGaiaEntities = function()
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetNonGaiaEntities();
};

/**
 * Displays the rally points of a given list of entities (carried in cmd.entities).
 *
 * The 'cmd' object may carry its own x/z coordinate pair indicating the location where the rally point should
 * be rendered, in order to support instantaneously rendering a rally point marker at a specified location
 * instead of incurring a delay while PostNetworkCommand processes the set-rallypoint command (see input.js).
 * If cmd doesn't carry a custom location, then the position to render the marker at will be read from the
 * RallyPoint component.
 */
GuiInterface.prototype.DisplayRallyPoint = function(player, cmd)
{
	const cmpPlayer = QueryPlayerIDInterface(player);

	// If there are some rally points already displayed, first hide them.
	for (const ent of this.entsRallyPointsDisplayed)
	{
		const cmpRallyPointRenderer = Engine.QueryInterface(ent, IID_RallyPointRenderer);
		if (cmpRallyPointRenderer)
			cmpRallyPointRenderer.SetDisplayed(false);
	}

	this.entsRallyPointsDisplayed = [];

	// Show the rally points for the passed entities.
	for (const ent of cmd.entities)
	{
		const cmpRallyPointRenderer = Engine.QueryInterface(ent, IID_RallyPointRenderer);
		if (!cmpRallyPointRenderer)
			continue;

		// Entity must have a rally point component to display a rally point marker
		// (regardless of whether cmd specifies a custom location).
		const cmpRallyPoint = Engine.QueryInterface(ent, IID_RallyPoint);
		if (!cmpRallyPoint)
			continue;

		// Verify the owner.
		const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
		if (!(cmpPlayer && cmpPlayer.CanControlAllUnits()))
			if (!cmpOwnership || cmpOwnership.GetOwner() != player)
				continue;

		// If the command was passed an explicit position, use that and
		// override the real rally point position; otherwise use the real position.
		let pos;
		if (cmd.x && cmd.z)
			pos = cmd;
		else
			// May return undefined if no rally point is set.
			pos = cmpRallyPoint.GetPositions()[0];

		if (pos)
		{
			// Only update the position if we changed it (cmd.queued is set).
			// Note that Add-/SetPosition take a CFixedVector2D which has X/Y components, not X/Z.
			if ("queued" in cmd)
			{
				if (cmd.queued == true)
					cmpRallyPointRenderer.AddPosition(new Vector2D(pos.x, pos.z));
				else
					cmpRallyPointRenderer.SetPosition(new Vector2D(pos.x, pos.z));
			}
			else if (!cmpRallyPointRenderer.IsSet())
				// Rebuild the renderer when not set (when reading saved game or in case of building update).
				for (const posi of cmpRallyPoint.GetPositions())
					cmpRallyPointRenderer.AddPosition(new Vector2D(posi.x, posi.z));

			cmpRallyPointRenderer.SetDisplayed(true);

			// Remember which entities have their rally points displayed so we can hide them again.
			this.entsRallyPointsDisplayed.push(ent);
		}
	}
};

GuiInterface.prototype.AddTargetMarker = function(player, cmd)
{
	const ent = Engine.AddLocalEntity(cmd.template);
	if (!ent)
		return;
	const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
	if (cmpOwnership)
		cmpOwnership.SetOwner(cmd.owner);
	const cmpPosition = Engine.QueryInterface(ent, IID_Position);
	cmpPosition.JumpTo(cmd.x, cmd.z);
};

/**
 * Display the building placement preview.
 * cmd.template is the name of the entity template, or "" to disable the preview.
 * cmd.x, cmd.z, cmd.angle give the location.
 *
 * Returns result object from CheckPlacement:
 * 	{
 *		"success":             true iff the placement is valid, else false
 *		"message":             message to display in UI for invalid placement, else ""
 *		"parameters":          parameters to use in the message
 *		"translateMessage":    localisation info
 *		"translateParameters": localisation info
 *		"pluralMessage":       we might return a plural translation instead (optional)
 *		"pluralCount":         localisation info (optional)
 *  }
 */
GuiInterface.prototype.SetBuildingPlacementPreview = function(player, cmd)
{
	let result = {
		"success": false,
		"message": "",
		"parameters": {},
		"translateMessage": false,
		"translateParameters": []
	};

	if (!this.placementEntity || this.placementEntity[0] != cmd.template)
	{
		if (this.placementEntity)
			Engine.DestroyEntity(this.placementEntity[1]);

		if (cmd.template == "")
			this.placementEntity = undefined;
		else
			this.placementEntity = [cmd.template, Engine.AddLocalEntity("preview|" + cmd.template)];
	}

	if (this.placementEntity)
	{
		const ent = this.placementEntity[1];

		const pos = Engine.QueryInterface(ent, IID_Position);
		if (pos)
		{
			pos.JumpTo(cmd.x, cmd.z);
			pos.SetYRotation(cmd.angle);
		}

		const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
		cmpOwnership.SetOwner(player);

		const cmpBuildRestrictions = Engine.QueryInterface(ent, IID_BuildRestrictions);
		if (!cmpBuildRestrictions)
			error("cmpBuildRestrictions not defined");
		else
			result = cmpBuildRestrictions.CheckPlacement();

		const cmpRangeOverlayManager = Engine.QueryInterface(ent, IID_RangeOverlayManager);
		if (cmpRangeOverlayManager)
			cmpRangeOverlayManager.SetEnabled(true, this.enabledVisualRangeOverlayTypes);

		// Set it to a red shade if this is an invalid location.
		const cmpVisual = Engine.QueryInterface(ent, IID_Visual);
		if (cmpVisual)
		{
			if (cmd.actorSeed !== undefined)
				cmpVisual.SetActorSeed(cmd.actorSeed);

			if (!result.success)
				cmpVisual.SetShadingColor(1.4, 0.4, 0.4, 1);
			else
				cmpVisual.SetShadingColor(1, 1, 1, 1);
		}
	}

	return result;
};

/**
 * Previews the placement of a wall between cmd.start and cmd.end, or just the starting piece of a wall if cmd.end is not
 * specified. Returns an object with information about the list of entities that need to be newly constructed to complete
 * at least a part of the wall, or false if there are entities required to build at least part of the wall but none of
 * them can be validly constructed.
 *
 * It's important to distinguish between three lists of entities that are at play here, because they may be subsets of one
 * another depending on things like snapping and whether some of the entities inside them can be validly positioned.
 * We have:
 *    - The list of entities that previews the wall. This list is usually equal to the entities required to construct the
 *      entire wall. However, if there is snapping to an incomplete tower (i.e. a foundation), it includes extra entities
 *      to preview the completed tower on top of its foundation.
 *
 *    - The list of entities that need to be newly constructed to build the entire wall. This list is regardless of whether
 *      any of them can be validly positioned. The emphasishere here is on 'newly'; this list does not include any existing
 *      towers at either side of the wall that we snapped to. Or, more generally; it does not include any _entities_ that we
 *      snapped to; we might still snap to e.g. terrain, in which case the towers on either end will still need to be newly
 *      constructed.
 *
 *    - The list of entities that need to be newly constructed to build at least a part of the wall. This list is the same
 *      as the one above, except that it is truncated at the first entity that cannot be validly positioned. This happens
 *      e.g. if the player tries to build a wall straight through an obstruction. Note that any entities that can be validly
 *      constructed but come after said first invalid entity are also truncated away.
 *
 * With this in mind, this method will return false if the second list is not empty, but the third one is. That is, if there
 * were entities that are needed to build the wall, but none of them can be validly constructed. False is also returned in
 * case of unexpected errors (typically missing components), and when clearing the preview by passing an empty wallset
 * argument (see below). Otherwise, it will return an object with the following information:
 *
 * result: {
 *   'startSnappedEnt':   ID of the entity that we snapped to at the starting side of the wall. Currently only supports towers.
 *   'endSnappedEnt':     ID of the entity that we snapped to at the (possibly truncated) ending side of the wall. Note that this
 *                        can only be set if no truncation of the second list occurs; if we snapped to an entity at the ending side
 *                        but the wall construction was truncated before we could reach it, it won't be set here. Currently only
 *                        supports towers.
 *   'pieces':            Array with the following data for each of the entities in the third list:
 *    [{
 *       'template':      Template name of the entity.
 *       'x':             X coordinate of the entity's position.
 *       'z':             Z coordinate of the entity's position.
 *       'angle':         Rotation around the Y axis of the entity (in radians).
 *     },
 *     ...]
 *   'cost': {            The total cost required for constructing all the pieces as listed above.
 *     'food': ...,
 *     'wood': ...,
 *     'stone': ...,
 *     'metal': ...,
 *     'population': ...,
 *   }
 * }
 *
 * @param cmd.wallSet Object holding the set of wall piece template names. Set to an empty value to clear the preview.
 * @param cmd.start Starting point of the wall segment being created.
 * @param cmd.end (Optional) Ending point of the wall segment being created. If not defined, it is understood that only
 *                 the starting point of the wall is available at this time (e.g. while the player is still in the process
 *                 of picking a starting point), and that therefore only the first entity in the wall (a tower) should be
 *                 previewed.
 * @param cmd.snapEntities List of candidate entities to snap the start and ending positions to.
 */
GuiInterface.prototype.SetWallPlacementPreview = function(player, cmd)
{
	const wallSet = cmd.wallSet;

	// Did the start position snap to anything?
	// If we snapped, was it to an entity? If yes, hold that entity's ID.
	const start = {
		"pos": cmd.start,
		"angle": 0,
		"snapped": false,
		"snappedEnt": INVALID_ENTITY
	};

	// Did the end position snap to anything?
	// If we snapped, was it to an entity? If yes, hold that entity's ID.
	const end = {
		"pos": cmd.end,
		"angle": 0,
		"snapped": false,
		"snappedEnt": INVALID_ENTITY
	};

	// --------------------------------------------------------------------------------
	// Do some entity cache management and check for snapping.

	if (!this.placementWallEntities)
		this.placementWallEntities = {};

	if (!wallSet)
	{
		// We're clearing the preview, clear the entity cache and bail.
		for (const tpl in this.placementWallEntities)
		{
			for (const ent of this.placementWallEntities[tpl].entities)
				Engine.DestroyEntity(ent);

			this.placementWallEntities[tpl].numUsed = 0;
			this.placementWallEntities[tpl].entities = [];
			// Keep template data around.
		}

		return false;
	}

	for (const tpl in this.placementWallEntities)
	{
		for (const ent of this.placementWallEntities[tpl].entities)
		{
			const pos = Engine.QueryInterface(ent, IID_Position);
			if (pos)
				pos.MoveOutOfWorld();
		}

		this.placementWallEntities[tpl].numUsed = 0;
	}

	// Create cache entries for templates we haven't seen before.
	for (const type in wallSet.templates)
	{
		if (type == "curves")
			continue;

		const tpl = wallSet.templates[type];
		if (!(tpl in this.placementWallEntities))
		{
			this.placementWallEntities[tpl] = {
				"numUsed": 0,
				"entities": [],
				"templateData": this.GetTemplateData(player, { "templateName": tpl }),
			};

			if (!this.placementWallEntities[tpl].templateData.wallPiece)
			{
				error("[SetWallPlacementPreview] No WallPiece component found for wall set template '" + tpl + "'");
				return false;
			}
		}
	}

	// Prevent division by zero errors further on if the start and end positions are the same.
	if (end.pos && (start.pos.x === end.pos.x && start.pos.z === end.pos.z))
		end.pos = undefined;

	// See if we need to snap the start and/or end coordinates to any of our list of snap entities. Note that, despite the list
	// of snapping candidate entities, it might still snap to e.g. terrain features. Use the "ent" key in the returned snapping
	// data to determine whether it snapped to an entity (if any), and to which one (see GetFoundationSnapData).
	if (cmd.snapEntities)
	{
		// Value of 0.5 was determined through trial and error.
		const snapRadius = this.placementWallEntities[wallSet.templates.tower].templateData.wallPiece.length * 0.5;
		const startSnapData = this.GetFoundationSnapData(player, {
			"x": start.pos.x,
			"z": start.pos.z,
			"template": wallSet.templates.tower,
			"snapEntities": cmd.snapEntities,
			"snapRadius": snapRadius,
		});

		if (startSnapData)
		{
			start.pos.x = startSnapData.x;
			start.pos.z = startSnapData.z;
			start.angle = startSnapData.angle;
			start.snapped = true;

			if (startSnapData.ent)
				start.snappedEnt = startSnapData.ent;
		}

		if (end.pos)
		{
			const endSnapData = this.GetFoundationSnapData(player, {
				"x": end.pos.x,
				"z": end.pos.z,
				"template": wallSet.templates.tower,
				"snapEntities": cmd.snapEntities,
				"snapRadius": snapRadius,
			});

			if (endSnapData)
			{
				end.pos.x = endSnapData.x;
				end.pos.z = endSnapData.z;
				end.angle = endSnapData.angle;
				end.snapped = true;

				if (endSnapData.ent)
					end.snappedEnt = endSnapData.ent;
			}
		}
	}

	// Clear the single-building preview entity (we'll be rolling our own).
	this.SetBuildingPlacementPreview(player, { "template": "" });

	// --------------------------------------------------------------------------------
	// Calculate wall placement and position preview entities.

	const result = {
		"pieces": [],
		"cost": { "population": 0, "time": 0 }
	};
	for (const res of Resources.GetCodes())
		result.cost[res] = 0;

	let previewEntities = [];
	if (end.pos)
		// See helpers/Walls.js.
		previewEntities = GetWallPlacement(this.placementWallEntities, wallSet, start, end);

	// For wall placement, we may (and usually do) need to have wall pieces overlap each other more than would
	// otherwise be allowed by their obstruction shapes. However, during this preview phase, this is not so much of
	// an issue, because all preview entities have their obstruction components deactivated, meaning that their
	// obstruction shapes do not register in the simulation and hence cannot affect it. This implies that the preview
	// entities cannot be found to obstruct each other, which largely solves the issue of overlap between wall pieces.

	// Note that they will still be obstructed by existing shapes in the simulation (that have the BLOCK_FOUNDATION
	// flag set), which is what we want. The only exception to this is when snapping to existing towers (or
	// foundations thereof); the wall segments that connect up to these will be found to be obstructed by the
	// existing tower/foundation, and be shaded red to indicate that they cannot be placed there. To prevent this,
	// we manually set the control group of the outermost wall pieces equal to those of the snapped-to towers, so
	// that they are free from mutual obstruction (per definition of obstruction control groups). This is done by
	// assigning them an extra "controlGroup" field, which we'll then set during the placement loop below.

	// Additionally, in the situation that we're snapping to merely a foundation of a tower instead of a fully
	// constructed one, we'll need an extra preview entity for the starting tower, which also must not be obstructed
	// by the foundation it snaps to.

	if (start.snappedEnt && start.snappedEnt != INVALID_ENTITY)
	{
		const startEntObstruction = Engine.QueryInterface(start.snappedEnt, IID_Obstruction);
		if (previewEntities.length && startEntObstruction)
			previewEntities[0].controlGroups = [startEntObstruction.GetControlGroup()];

		// If we're snapping to merely a foundation, add an extra preview tower and also set it to the same control group.
		const startEntState = this.GetEntityState(player, start.snappedEnt);
		if (startEntState.foundation)
		{
			const cmpPosition = Engine.QueryInterface(start.snappedEnt, IID_Position);
			if (cmpPosition)
				previewEntities.unshift({
					"template": wallSet.templates.tower,
					"pos": start.pos,
					"angle": cmpPosition.GetRotation().y,
					"controlGroups": [startEntObstruction ? startEntObstruction.GetControlGroup() : undefined],
					"excludeFromResult": true // Preview only, must not appear in the result.
				});
		}
	}
	else
	{
		// Didn't snap to an existing entity, add the starting tower manually. To prevent odd-looking rotation jumps
		// when shift-clicking to build a wall, reuse the placement angle that was last seen on a validly positioned
		// wall piece.

		// To illustrate the last point, consider what happens if we used some constant instead, say, 0. Issuing the
		// build command for a wall is asynchronous, so when the preview updates after shift-clicking, the wall piece
		// foundations are not registered yet in the simulation. This means they cannot possibly be picked in the list
		// of candidate entities for snapping. In the next preview update, we therefore hit this case, and would rotate
		// the preview to 0 radians. Then, after one or two simulation updates or so, the foundations register and
		// onSimulationUpdate in session.js updates the preview again. It first grabs a new list of snapping candidates,
		// which this time does include the new foundations; so we snap to the entity, and rotate the preview back to
		// the foundation's angle.

		// The result is a noticeable rotation to 0 and back, which is undesirable. So, for a split second there until
		// the simulation updates, we fake it by reusing the last angle and hope the player doesn't notice.
		previewEntities.unshift({
			"template": wallSet.templates.tower,
			"pos": start.pos,
			"angle": previewEntities.length ? previewEntities[0].angle : this.placementWallLastAngle
		});
	}

	if (end.pos)
	{
		// Analogous to the starting side case above.
		if (end.snappedEnt && end.snappedEnt != INVALID_ENTITY)
		{
			const endEntObstruction = Engine.QueryInterface(end.snappedEnt, IID_Obstruction);

			// Note that it's possible for the last entity in previewEntities to be the same as the first, i.e. the
			// same wall piece snapping to both a starting and an ending tower. And it might be more common than you would
			// expect; the allowed overlap between wall segments and towers facilitates this to some degree. To deal with
			// the possibility of dual initial control groups, we use a '.controlGroups' array rather than a single
			// '.controlGroup' property. Note that this array can only ever have 0, 1 or 2 elements (checked at a later time).
			if (previewEntities.length > 0 && endEntObstruction)
			{
				previewEntities[previewEntities.length - 1].controlGroups = previewEntities[previewEntities.length - 1].controlGroups || [];
				previewEntities[previewEntities.length - 1].controlGroups.push(endEntObstruction.GetControlGroup());
			}

			// If we're snapping to a foundation, add an extra preview tower and also set it to the same control group.
			const endEntState = this.GetEntityState(player, end.snappedEnt);
			if (endEntState.foundation)
			{
				const cmpPosition = Engine.QueryInterface(end.snappedEnt, IID_Position);
				if (cmpPosition)
					previewEntities.push({
						"template": wallSet.templates.tower,
						"pos": end.pos,
						"angle": cmpPosition.GetRotation().y,
						"controlGroups": [endEntObstruction ? endEntObstruction.GetControlGroup() : undefined],
						"excludeFromResult": true
					});
			}
		}
		else
			previewEntities.push({
				"template": wallSet.templates.tower,
				"pos": end.pos,
				"angle": previewEntities.length ? previewEntities[previewEntities.length - 1].angle : this.placementWallLastAngle
			});
	}

	const cmpTerrain = Engine.QueryInterface(SYSTEM_ENTITY, IID_Terrain);
	if (!cmpTerrain)
	{
		error("[SetWallPlacementPreview] System Terrain component not found");
		return false;
	}

	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	if (!cmpRangeManager)
	{
		error("[SetWallPlacementPreview] System RangeManager component not found");
		return false;
	}

	// Loop through the preview entities, and construct the subset of them that need to be, and can be, validly constructed
	// to build at least a part of the wall (meaning that the subset is truncated after the first entity that needs to be,
	// but cannot validly be, constructed). See method-level documentation for more details.

	let allPiecesValid = true;
	// Number of entities that are required to build the entire wall, regardless of validity.
	let numRequiredPieces = 0;

	for (let i = 0; i < previewEntities.length; ++i)
	{
		const entInfo = previewEntities[i];

		let ent = null;
		const tpl = entInfo.template;
		const tplData = this.placementWallEntities[tpl].templateData;
		const entPool = this.placementWallEntities[tpl];

		if (entPool.numUsed >= entPool.entities.length)
		{
			ent = Engine.AddLocalEntity("preview|" + tpl);
			entPool.entities.push(ent);
		}
		else
			ent = entPool.entities[entPool.numUsed];

		if (!ent)
		{
			error("[SetWallPlacementPreview] Failed to allocate or reuse preview entity of template '" + tpl + "'");
			continue;
		}

		// Move piece to right location.
		// TODO: Consider reusing SetBuildingPlacementReview for this, enhanced to be able to deal with multiple entities.
		const cmpPosition = Engine.QueryInterface(ent, IID_Position);
		if (cmpPosition)
		{
			cmpPosition.JumpTo(entInfo.pos.x, entInfo.pos.z);
			cmpPosition.SetYRotation(entInfo.angle);

			// If this piece is a tower, then it should have a Y position that is at least as high as its surrounding pieces.
			if (tpl === wallSet.templates.tower)
			{
				let terrainGroundPrev = null;
				let terrainGroundNext = null;

				if (i > 0)
					terrainGroundPrev = cmpTerrain.GetGroundLevel(previewEntities[i - 1].pos.x, previewEntities[i - 1].pos.z);

				if (i < previewEntities.length - 1)
					terrainGroundNext = cmpTerrain.GetGroundLevel(previewEntities[i + 1].pos.x, previewEntities[i + 1].pos.z);

				if (terrainGroundPrev != null || terrainGroundNext != null)
				{
					const targetY = Math.max(terrainGroundPrev, terrainGroundNext);
					cmpPosition.SetHeightFixed(targetY);
				}
			}
		}

		const cmpObstruction = Engine.QueryInterface(ent, IID_Obstruction);
		if (!cmpObstruction)
		{
			error("[SetWallPlacementPreview] Preview entity of template '" + tpl + "' does not have an Obstruction component");
			continue;
		}

		// Assign any predefined control groups. Note that there can only be 0, 1 or 2 predefined control groups; if there are
		// more, we've made a programming error. The control groups are assigned from the entInfo.controlGroups array on a
		// first-come first-served basis; the first value in the array is always assigned as the primary control group, and
		// any second value as the secondary control group.

		// By default, we reset the control groups to their standard values. Remember that we're reusing entities; if we don't
		// reset them, then an ending wall segment that was e.g. at one point snapped to an existing tower, and is subsequently
		// reused as a non-snapped ending wall segment, would no longer be capable of being obstructed by the same tower it was
		// once snapped to.

		let primaryControlGroup = ent;
		let secondaryControlGroup = INVALID_ENTITY;

		if (entInfo.controlGroups && entInfo.controlGroups.length > 0)
		{
			if (entInfo.controlGroups.length > 2)
			{
				error("[SetWallPlacementPreview] Encountered preview entity of template '" + tpl + "' with more than 2 initial control groups");
				break;
			}

			primaryControlGroup = entInfo.controlGroups[0];
			if (entInfo.controlGroups.length > 1)
				secondaryControlGroup = entInfo.controlGroups[1];
		}

		cmpObstruction.SetControlGroup(primaryControlGroup);
		cmpObstruction.SetControlGroup2(secondaryControlGroup);

		let validPlacement = false;

		const cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
		cmpOwnership.SetOwner(player);

		// Check whether it's in a visible or fogged region.
		// TODO: Should definitely reuse SetBuildingPlacementPreview, this is just straight up copy/pasta.
		const visible = cmpRangeManager.GetLosVisibility(ent, player) != "hidden";
		if (visible)
		{
			const cmpBuildRestrictions = Engine.QueryInterface(ent, IID_BuildRestrictions);
			if (!cmpBuildRestrictions)
			{
				error("[SetWallPlacementPreview] cmpBuildRestrictions not defined for preview entity of template '" + tpl + "'");
				continue;
			}

			// TODO: Handle results of CheckPlacement.
			validPlacement = cmpBuildRestrictions && cmpBuildRestrictions.CheckPlacement().success;

			// If a wall piece has two control groups, it's likely a segment that spans
			// between two existing towers. To avoid placing a duplicate wall segment,
			// check for collisions with entities that share both control groups.
			if (validPlacement && entInfo.controlGroups && entInfo.controlGroups.length > 1)
				validPlacement = cmpObstruction.CheckDuplicateFoundation();
		}

		allPiecesValid = allPiecesValid && validPlacement;

		// The requirement below that all pieces so far have to have valid positions, rather than only this single one,
		// ensures that no more foundations will be placed after a first invalidly-positioned piece. (It is possible
		// for pieces past some invalidly-positioned ones to still have valid positions, e.g. if you drag a wall
		// through and past an existing building).

		// Additionally, the excludeFromResult flag is set for preview entities that were manually added to be placed
		// on top of foundations of incompleted towers that we snapped to; they must not be part of the result.

		if (!entInfo.excludeFromResult)
			++numRequiredPieces;

		if (allPiecesValid && !entInfo.excludeFromResult)
		{
			result.pieces.push({
				"template": tpl,
				"x": entInfo.pos.x,
				"z": entInfo.pos.z,
				"angle": entInfo.angle,
			});
			this.placementWallLastAngle = entInfo.angle;

			// Grab the cost of this wall piece and add it up (note; preview entities don't have their Cost components
			// copied over, so we need to fetch it from the template instead).
			// TODO: We should really use a Cost object or at least some utility functions for this, this is mindless
			// boilerplate that's probably duplicated in tons of places.
			for (const res of Resources.GetCodes().concat(["population", "time"]))
				result.cost[res] += tplData.cost[res];
		}

		let canAfford = true;
		const cmpPlayer = QueryPlayerIDInterface(player, IID_Player);
		if (cmpPlayer && cmpPlayer.GetNeededResources(result.cost))
			canAfford = false;

		const cmpVisual = Engine.QueryInterface(ent, IID_Visual);
		if (cmpVisual)
		{
			if (!allPiecesValid || !canAfford)
				cmpVisual.SetShadingColor(1.4, 0.4, 0.4, 1);
			else
				cmpVisual.SetShadingColor(1, 1, 1, 1);
		}

		++entPool.numUsed;
	}

	// If any were entities required to build the wall, but none of them could be validly positioned, return failure
	// (see method-level documentation).
	if (numRequiredPieces > 0 && result.pieces.length == 0)
		return false;

	if (start.snappedEnt && start.snappedEnt != INVALID_ENTITY)
		result.startSnappedEnt = start.snappedEnt;

	// We should only return that we snapped to an entity if all pieces up until that entity can be validly constructed,
	// i.e. are included in result.pieces (see docs for the result object).
	if (end.pos && end.snappedEnt && end.snappedEnt != INVALID_ENTITY && allPiecesValid)
		result.endSnappedEnt = end.snappedEnt;

	return result;
};

/**
 * Given the current position {data.x, data.z} of an foundation of template data.template, returns the position and angle to snap
 * it to (if necessary/useful).
 *
 * @param data.x            The X position of the foundation to snap.
 * @param data.z            The Z position of the foundation to snap.
 * @param data.template     The template to get the foundation snapping data for.
 * @param data.snapEntities Optional; list of entity IDs to snap to if {data.x, data.z} is within a circle of radius data.snapRadius
 *                            around the entity. Only takes effect when used in conjunction with data.snapRadius.
 *                          When this option is used and the foundation is found to snap to one of the entities passed in this list
 *                            (as opposed to e.g. snapping to terrain features), then the result will contain an additional key "ent",
 *                            holding the ID of the entity that was snapped to.
 * @param data.snapRadius   Optional; when used in conjunction with data.snapEntities, indicates the circle radius around an entity that
 *                            {data.x, data.z} must be located within to have it snap to that entity.
 */
GuiInterface.prototype.GetFoundationSnapData = function(player, data)
{
	const template = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager).GetTemplate(data.template);
	if (!template)
	{
		warn("[GetFoundationSnapData] Failed to load template '" + data.template + "'");
		return false;
	}

	if (data.snapEntities && data.snapRadius && data.snapRadius > 0)
	{
		// See if {data.x, data.z} is inside the snap radius of any of the snap entities; and if so, to which it is closest.
		// (TODO: Break unlikely ties by choosing the lowest entity ID.)

		let minDist2 = -1;
		let minDistEntitySnapData = null;
		const radius2 = data.snapRadius * data.snapRadius;

		for (const ent of data.snapEntities)
		{
			const cmpPosition = Engine.QueryInterface(ent, IID_Position);
			if (!cmpPosition || !cmpPosition.IsInWorld())
				continue;

			const pos = cmpPosition.GetPosition();
			const dist2 = (data.x - pos.x) * (data.x - pos.x) + (data.z - pos.z) * (data.z - pos.z);
			if (dist2 > radius2)
				continue;

			if (minDist2 < 0 || dist2 < minDist2)
			{
				minDist2 = dist2;
				minDistEntitySnapData = {
					"x": pos.x,
					"z": pos.z,
					"angle": cmpPosition.GetRotation().y,
					"ent": ent
				};
			}
		}

		if (minDistEntitySnapData != null)
			return minDistEntitySnapData;
	}

	if (data.snapToEdges)
	{
		const position = this.obstructionSnap.getPosition(data, template);
		if (position)
			return position;
	}

	if (template.BuildRestrictions.PlacementType == "shore")
	{
		const angle = GetDockAngle(template, data.x, data.z);
		if (angle !== undefined)
			return {
				"x": data.x,
				"z": data.z,
				"angle": angle
			};
	}

	return false;
};

GuiInterface.prototype.PlaySoundForPlayer = function(player, data)
{
	const playerEntityID = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetPlayerByID(player);
	const cmpSound = Engine.QueryInterface(playerEntityID, IID_Sound);
	if (!cmpSound)
		return;

	const soundGroup = cmpSound.GetSoundGroup(data.name);
	if (soundGroup)
		Engine.QueryInterface(SYSTEM_ENTITY, IID_SoundManager).PlaySoundGroupForPlayer(soundGroup, player);
};

GuiInterface.prototype.PlaySound = function(player, data)
{
	if (!data.entity)
		return;

	PlaySound(data.name, data.entity);
};

/**
 * Find any idle units.
 *
 * @param data.idleClasses		Array of class names to include.
 * @param data.prevUnit		The previous idle unit, if calling a second time to iterate through units.  May be left undefined.
 * @param data.limit			The number of idle units to return.  May be left undefined (will return all idle units).
 * @param data.excludeUnits	Array of units to exclude.
 *
 * Returns an array of idle units.
 * If multiple classes were supplied, and multiple items will be returned, the items will be sorted by class.
 */
GuiInterface.prototype.FindIdleUnits = function(player, data)
{
	const idleUnits = [];
	// The general case is that only the 'first' idle unit is required; filtering would examine every unit.
	// This loop imitates a grouping/aggregation on the first matching idle class.
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	for (const entity of cmpRangeManager.GetEntitiesByPlayer(player))
	{
		const filtered = this.IdleUnitFilter(entity, data.idleClasses, data.excludeUnits);
		if (!filtered.idle)
			continue;

		// If the entity is in the 'current' (first, 0) bucket on a resumed search, it must be after the "previous" unit, if any.
		// By adding to the 'end', there is no pause if the series of units loops.
		let bucket = filtered.bucket;
		if (bucket == 0 && data.prevUnit && entity <= data.prevUnit)
			bucket = data.idleClasses.length;

		if (!idleUnits[bucket])
			idleUnits[bucket] = [];
		idleUnits[bucket].push(entity);

		// If enough units have been collected in the first bucket, go ahead and return them.
		if (data.limit && bucket == 0 && idleUnits[0].length == data.limit)
			return idleUnits[0];
	}

	const reduced = idleUnits.reduce((prev, curr) => prev.concat(curr), []);
	if (data.limit && reduced.length > data.limit)
		return reduced.slice(0, data.limit);

	return reduced;
};

/**
 * Discover if the player has idle units.
 *
 * @param data.idleClasses	Array of class names to include.
 * @param data.excludeUnits	Array of units to exclude.
 *
 * Returns a boolean of whether the player has any idle units
 */
GuiInterface.prototype.HasIdleUnits = function(player, data)
{
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	return cmpRangeManager.GetEntitiesByPlayer(player).some(unit => this.IdleUnitFilter(unit, data.idleClasses, data.excludeUnits).idle);
};

/**
 * Whether to filter an idle unit
 *
 * @param unit			The unit to filter.
 * @param idleclasses	Array of class names to include.
 * @param excludeUnits	Array of units to exclude.
 *
 * Returns an object with the following fields:
 * 	- idle - true if the unit is considered idle by the filter, false otherwise.
 * 	- bucket - if idle, set to the index of the first matching idle class, undefined otherwise.
 */
GuiInterface.prototype.IdleUnitFilter = function(unit, idleClasses, excludeUnits)
{
	const cmpUnitAI = Engine.QueryInterface(unit, IID_UnitAI);
	if (!cmpUnitAI || !cmpUnitAI.IsIdle())
		return { "idle": false };

	const cmpGarrisonable = Engine.QueryInterface(unit, IID_Garrisonable);
	if (cmpGarrisonable && cmpGarrisonable.IsGarrisoned())
		return { "idle": false };

	const cmpTurretable = Engine.QueryInterface(unit, IID_Turretable);
	if (cmpTurretable && cmpTurretable.IsTurreted())
		return { "idle": false };

	const cmpIdentity = Engine.QueryInterface(unit, IID_Identity);
	if (!cmpIdentity)
		return { "idle": false };

	const bucket = idleClasses.findIndex(elem => MatchesClassList(cmpIdentity.GetClassesList(), elem));
	if (bucket == -1 || excludeUnits.indexOf(unit) > -1)
		return { "idle": false };

	return { "idle": true, "bucket": bucket };
};

GuiInterface.prototype.GetTradingRouteGain = function(player, data)
{
	if (!data.firstMarket || !data.secondMarket)
		return null;

	const cmpMarket = QueryMiragedInterface(data.firstMarket, IID_Market);
	return cmpMarket && cmpMarket.CalculateTraderGain(data.secondMarket, data.template);
};

GuiInterface.prototype.GetTradingDetails = function(player, data)
{
	const cmpEntityTrader = Engine.QueryInterface(data.trader, IID_Trader);
	if (!cmpEntityTrader || !cmpEntityTrader.CanTrade(data.target))
		return null;

	const firstMarket = cmpEntityTrader.GetFirstMarket();
	const secondMarket = cmpEntityTrader.GetSecondMarket();
	let result = null;
	if (data.target === firstMarket)
	{
		result = {
			"type": "is first",
			"hasBothMarkets": cmpEntityTrader.HasBothMarkets()
		};
		if (cmpEntityTrader.HasBothMarkets())
			result.gain = cmpEntityTrader.GetGoods().amount;
	}
	else if (data.target === secondMarket)
		result = {
			"type": "is second",
			"gain": cmpEntityTrader.GetGoods().amount,
		};
	else if (!firstMarket)
		result = { "type": "set first" };
	else if (!secondMarket)
		result = {
			"type": "set second",
			"gain": cmpEntityTrader.CalculateGain(firstMarket, data.target),
		};
	else
		result = { "type": "set first" };

	return result;
};

GuiInterface.prototype.CanAttack = function(player, data)
{
	const cmpAttack = Engine.QueryInterface(data.entity, IID_Attack);
	return cmpAttack && cmpAttack.CanAttack(data.target, data.types || undefined);
};

/*
 * Returns batch build time.
 */
GuiInterface.prototype.GetBatchTime = function(player, data)
{
	return Engine.QueryInterface(data.entity, IID_Trainer)?.GetBatchTime(data.batchSize) || 0;
};

GuiInterface.prototype.IsMapRevealed = function(player)
{
	return Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetLosRevealAll(player);
};

GuiInterface.prototype.SetPathfinderDebugOverlay = function(player, enabled)
{
	Engine.QueryInterface(SYSTEM_ENTITY, IID_Pathfinder).SetDebugOverlay(enabled);
};

GuiInterface.prototype.SetPathfinderHierDebugOverlay = function(player, enabled)
{
	Engine.QueryInterface(SYSTEM_ENTITY, IID_Pathfinder).SetHierDebugOverlay(enabled);
};

GuiInterface.prototype.SetObstructionDebugOverlay = function(player, enabled)
{
	Engine.QueryInterface(SYSTEM_ENTITY, IID_ObstructionManager).SetDebugOverlay(enabled);
};

GuiInterface.prototype.SetMotionDebugOverlay = function(player, data)
{
	for (const ent of data.entities)
	{
		const cmpUnitMotion = Engine.QueryInterface(ent, IID_UnitMotion);
		if (cmpUnitMotion)
			cmpUnitMotion.SetDebugOverlay(data.enabled);
	}
};

GuiInterface.prototype.SetRangeDebugOverlay = function(player, enabled)
{
	Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).SetDebugOverlay(enabled);
};

GuiInterface.prototype.GetTraderNumber = function(player)
{
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	const traders = cmpRangeManager.GetEntitiesByPlayer(player).filter(e => Engine.QueryInterface(e, IID_Trader));

	const landTrader = { "total": 0, "trading": 0, "garrisoned": 0 };
	const shipTrader = { "total": 0, "trading": 0 };

	for (const ent of traders)
	{
		const cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
		const cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
		if (!cmpIdentity || !cmpUnitAI)
			continue;

		if (cmpIdentity.HasClass("Ship"))
		{
			++shipTrader.total;
			if (cmpUnitAI.order && cmpUnitAI.order.type == "Trade")
				++shipTrader.trading;
		}
		else
		{
			++landTrader.total;
			if (cmpUnitAI.order && cmpUnitAI.order.type == "Trade")
				++landTrader.trading;
			if (cmpUnitAI.order && cmpUnitAI.order.type == "Garrison")
			{
				const holder = cmpUnitAI.order.data.target;
				const cmpHolderUnitAI = Engine.QueryInterface(holder, IID_UnitAI);
				if (cmpHolderUnitAI && cmpHolderUnitAI.order && cmpHolderUnitAI.order.type == "Trade")
					++landTrader.garrisoned;
			}
		}
	}

	return { "landTrader": landTrader, "shipTrader": shipTrader };
};

GuiInterface.prototype.GetTradingGoods = function(player)
{
	const cmpPlayer = QueryPlayerIDInterface(player);
	if (!cmpPlayer)
		return [];

	return cmpPlayer.GetTradingGoods();
};

GuiInterface.prototype.OnGlobalEntityRenamed = function(msg)
{
	this.renamedEntities.push(msg);
};

/**
 * List the GuiInterface functions that can be safely called by GUI scripts.
 * (GUI scripts are non-deterministic and untrusted, so these functions must be
 * appropriately careful. They are called with a first argument "player", which is
 * trusted and indicates the player associated with the current client; no data should
 * be returned unless this player is meant to be able to see it.)
 */
GuiInterface.prototype.exposedFunctions = {

	"GetSimulationState": 1,
	"GetExtendedSimulationState": 1,
	"GetInitAttributes": 1,
	"GetReplayMetadata": 1,
	"GetCampaignGameEndData": 1,
	"GetRenamedEntities": 1,
	"ClearRenamedEntities": 1,
	"GetEntityState": 1,
	"GetMultipleEntityStates": 1,
	"GetAverageRangeForBuildings": 1,
	"GetTemplateData": 1,
	"AreRequirementsMet": 1,
	"CheckTechnologyRequirements": 1,
	"GetStartedResearch": 1,
	"GetBattleState": 1,
	"GetIncomingAttacks": 1,
	"GetNeededResources": 1,
	"GetNotifications": 1,
	"GetTimeNotifications": 1,

	"GetAvailableFormations": 1,
	"GetFormationRequirements": 1,
	"CanMoveEntsIntoFormation": 1,
	"IsFormationSelected": 1,
	"GetFormationInfoFromTemplate": 1,
	"IsStanceSelected": 1,

	"UpdateDisplayedPlayerColors": 1,
	"SetSelectionHighlight": 1,
	"GetAllBuildableEntities": 1,
	"SetStatusBars": 1,
	"GetPlayerEntities": 1,
	"GetNonGaiaEntities": 1,
	"DisplayRallyPoint": 1,
	"AddTargetMarker": 1,
	"SetBuildingPlacementPreview": 1,
	"SetWallPlacementPreview": 1,
	"GetFoundationSnapData": 1,
	"PlaySound": 1,
	"PlaySoundForPlayer": 1,
	"FindIdleUnits": 1,
	"HasIdleUnits": 1,
	"GetTradingRouteGain": 1,
	"GetTradingDetails": 1,
	"CanAttack": 1,
	"GetBatchTime": 1,

	"IsMapRevealed": 1,
	"SetPathfinderDebugOverlay": 1,
	"SetPathfinderHierDebugOverlay": 1,
	"SetObstructionDebugOverlay": 1,
	"SetMotionDebugOverlay": 1,
	"SetRangeDebugOverlay": 1,
	"EnableVisualRangeOverlayType": 1,
	"SetRangeOverlays": 1,

	"GetTraderNumber": 1,
	"GetTradingGoods": 1,
	"IsTemplateModified": 1,
	"ResetTemplateModified": 1,
	"IsSelectionDirty": 1,
	"ResetSelectionDirty": 1
};

GuiInterface.prototype.ScriptCall = function(player, name, args)
{
	if (this.exposedFunctions[name])
		return this[name](player, args);

	throw new Error("Invalid GuiInterface Call name \"" + name + "\"");
};

Engine.RegisterSystemComponentType(IID_GuiInterface, "GuiInterface", GuiInterface);
