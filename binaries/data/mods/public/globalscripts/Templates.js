/**
 * Loads history and gameplay data of all civs.
 *
 * @param selectableOnly {boolean} - Only load civs that can be selected
 *        in the gamesetup. Scenario maps might set non-selectable civs.
 */
function loadCivFiles(selectableOnly)
{
	const propertyNames = [
		"Code", "Culture", "Music", "CivBonuses", "StartEntities",
		"AINames", "SkirmishReplacements", "SelectableInGameSetup"];

	const civData = {};

	for (const filename of Engine.ListDirectoryFiles("simulation/data/civs/", "*.json", false))
	{
		const data = Engine.ReadJSONFile(filename);

		for (const prop of propertyNames)
			if (data[prop] === undefined)
				throw new Error(filename + " doesn't contain " + prop);

		if (selectableOnly && !data.SelectableInGameSetup)
			continue;

		const template = Engine.GetTemplate("special/players/" + data.Code);
		data.Name = template.Identity.GenericName;
		data.Emblem = "session/portraits/" + template.Identity.Icon;
		data.History = template.Identity.History;

		civData[data.Code] = data;
	}

	return civData;
}

/**
 * @return {string[]} - All the classes for this identity template.
 */
function GetIdentityClasses(template)
{
	let classString = "";

	if (template.Classes && template.Classes._string)
		classString += " " + template.Classes._string;

	if (template.VisibleClasses && template.VisibleClasses._string)
		classString += " " + template.VisibleClasses._string;

	if (template.Rank)
		classString += " " + template.Rank;

	return classString.length > 1 ? classString.substring(1).split(" ") : [];
}

/**
 * Gets an array with all classes for this identity template
 * that should be shown in the GUI
 */
function GetVisibleIdentityClasses(template)
{
	return template.VisibleClasses && template.VisibleClasses._string ? template.VisibleClasses._string.split(" ") : [];
}

/**
 * Check if a given list of classes matches another list of classes.
 * Useful f.e. for checking identity classes.
 *
 * @param classes - List of the classes to check against.
 * @param match - Either a string in the form
 *     "Class1 Class2+Class3"
 * where spaces are handled as OR and '+'-signs as AND,
 * and ! is handled as NOT, thus Class1+!Class2 = Class1 AND NOT Class2.
 * Or a list in the form
 *     [["Class1"], ["Class2", "Class3"]]
 * where the outer list is combined as OR, and the inner lists are AND-ed.
 * Or a hybrid format containing a list of strings, where the list is
 * combined as OR, and the strings are split by space and '+' and AND-ed.
 *
 * @return undefined if there are no classes or no match object
 * true if the the logical combination in the match object matches the classes
 * false otherwise.
 */
function MatchesClassList(classes, match)
{
	if (!match || !classes)
		return undefined;
	// Transform the string to an array
	if (typeof match === "string")
		match = match.split(/\s+/);

	for (let sublist of match)
	{
		// If the elements are still strings, split them by space or by '+'
		if (typeof sublist === "string")
			sublist = sublist.split(/[+\s]+/);
		if (sublist.every(c => (c[0] === "!" && classes.indexOf(c.substr(1)) === -1) ||
		                       (c[0] !== "!" && classes.indexOf(c) !== -1)))
			return true;
	}

	return false;
}

/**
 * Gets the value originating at the value_path as-is, with no modifiers applied.
 *
 * @param {Object} template - A valid template as returned from a template loader.
 * @param {string} value_path - Route to value within the xml template structure.
 * @param {number} default_value - A value to use if one is not specified in the template.
 * @return {number}
 */
function GetBaseTemplateDataValue(template, value_path, default_value)
{
	let current_value = template;
	for (const property of value_path.split("/"))
		current_value = current_value[property] || default_value;
	return +current_value;
}

/**
 * Gets the value originating at the value_path with the modifiers dictated by the mod_key applied.
 *
 * @param {Object} template - A valid template as returned from a template loader.
 * @param {string} value_path - Route to value within the xml template structure.
 * @param {string} mod_key - Tech modification key, if different from value_path.
 * @param {number} player - Optional player id.
 * @param {Object} modifiers - Value modifiers from auto-researched techs, unit upgrades,
 *                             etc. Optional as only used if no player id provided.
 * @param {number} default_value - A value to use if one is not specified in the template.
 * @return {number} Modifier altered value.
 */
function GetModifiedTemplateDataValue(template, value_path, mod_key, player, modifiers={}, default_value)
{
	let current_value = GetBaseTemplateDataValue(template, value_path, default_value);
	mod_key = mod_key || value_path;

	if (player)
		current_value = ApplyValueModificationsToTemplate(mod_key, current_value, player, template);
	else if (modifiers && modifiers[mod_key])
		current_value = GetTechModifiedProperty(modifiers[mod_key], GetIdentityClasses(template.Identity), current_value);

	// Using .toFixed() to get around spidermonkey's treatment of numbers (3 * 1.1 = 3.3000000000000003 for instance).
	return +current_value.toFixed(8);
}

/**
 * Get information about a template with or without technology modifications.
 *
 * NOTICE: The data returned here should have the same structure as
 * the object returned by GetEntityState and GetExtendedEntityState!
 *
 * @param {Object} template - A valid template as returned by the template loader.
 * @param {number} player - An optional player id to get the technology modifications
 *                          of properties.
 * @param {Object} auraTemplates - In the form of { key: { "auraName": "", "auraDescription": "" } }.
 * @param {Object} resources - An instance of the Resources class.
 * @param {Object} modifiers - Modifications from auto-researched techs, unit upgrades
 *                             etc. Optional as only used if there's no player
 *                             id provided.
 */
function GetTemplateDataHelper(template, player, auraTemplates, resources, modifiers = {})
{
	// Return data either from template (in tech tree) or sim state (ingame).
	// @param {string} value_path - Route to the value within the template.
	// @param {string} mod_key - Modification key, if not the same as the value_path.
	// @param {number} default_value - A value to use if one is not specified in the template.
	const getEntityValue = function(value_path, mod_key, default_value = 0) {
		return GetModifiedTemplateDataValue(template, value_path, mod_key, player, modifiers, default_value);
	};

	const ret = {};

	if (template.Resistance)
	{
		// Don't show Foundation resistance.
		ret.resistance = {};
		if (template.Resistance.Entity)
		{
			if (template.Resistance.Entity.Damage)
			{
				ret.resistance.Damage = {};
				for (const damageType in template.Resistance.Entity.Damage)
					ret.resistance.Damage[damageType] = getEntityValue("Resistance/Entity/Damage/" + damageType);
			}
			if (template.Resistance.Entity.Capture)
				ret.resistance.Capture = getEntityValue("Resistance/Entity/Capture");
			if (template.Resistance.Entity.ApplyStatus)
			{
				ret.resistance.ApplyStatus = {};
				for (const statusEffect in template.Resistance.Entity.ApplyStatus)
					ret.resistance.ApplyStatus[statusEffect] = {
						"blockChance": getEntityValue("Resistance/Entity/ApplyStatus/" + statusEffect + "/BlockChance"),
						"duration": getEntityValue("Resistance/Entity/ApplyStatus/" + statusEffect + "/Duration")
					};
			}
		}
	}

	const getAttackEffects = (temp, path) => {
		const effects = {};
		if (temp.Capture)
			effects.Capture = getEntityValue(path + "/Capture");

		if (temp.Damage)
		{
			effects.Damage = {};
			for (const damageType in temp.Damage)
				effects.Damage[damageType] = getEntityValue(path + "/Damage/" + damageType);
		}

		if (temp.ApplyStatus)
			effects.ApplyStatus = temp.ApplyStatus;

		return effects;
	};

	if (template.Attack)
	{
		ret.attack = {};
		for (const type in template.Attack)
		{
			const getAttackStat = function(stat) {
				return getEntityValue("Attack/" + type + "/" + stat);
			};

			ret.attack[type] = {
				"attackName": {
					"name": template.Attack[type].AttackName._string || template.Attack[type].AttackName,
					"context": template.Attack[type].AttackName["@context"]
				},
				"minRange": getAttackStat("MinRange"),
				"maxRange": getAttackStat("MaxRange"),
				"yOrigin": getAttackStat("Origin/Y")
			};

			ret.attack[type].elevationAdaptedRange = Math.sqrt(ret.attack[type].maxRange *
				(2 * ret.attack[type].yOrigin + ret.attack[type].maxRange));

			ret.attack[type].repeatTime = getAttackStat("RepeatTime");
			if (template.Attack[type].Projectile)
				ret.attack[type].friendlyFire = template.Attack[type].Projectile.FriendlyFire == "true";

			Object.assign(ret.attack[type], getAttackEffects(template.Attack[type], "Attack/" + type));

			if (template.Attack[type].Splash)
			{
				ret.attack[type].splash = {
					"friendlyFire": template.Attack[type].Splash.FriendlyFire != "false",
					"shape": template.Attack[type].Splash.Shape,
				};
				Object.assign(ret.attack[type].splash, getAttackEffects(template.Attack[type].Splash, "Attack/" + type + "/Splash"));
			}
		}
	}

	if (template.DeathDamage)
	{
		ret.deathDamage = {
			"friendlyFire": template.DeathDamage.FriendlyFire != "false",
		};

		Object.assign(ret.deathDamage, getAttackEffects(template.DeathDamage, "DeathDamage"));
	}

	if (template.Auras && auraTemplates)
	{
		ret.auras = {};
		for (const auraID of template.Auras._string.split(/\s+/))
			ret.auras[auraID] = GetAuraDataHelper(auraTemplates[auraID]);
	}

	if (template.BuildingAI)
		ret.buildingAI = {
			"defaultArrowCount": Math.round(getEntityValue("BuildingAI/DefaultArrowCount")),
			"garrisonArrowMultiplier": getEntityValue("BuildingAI/GarrisonArrowMultiplier"),
			"maxArrowCount": Math.round(getEntityValue("BuildingAI/MaxArrowCount"))
		};

	if (template.BuildRestrictions)
	{
		// required properties
		ret.buildRestrictions = {
			"placementType": template.BuildRestrictions.PlacementType,
			"territory": template.BuildRestrictions.Territory,
			"category": template.BuildRestrictions.Category,
		};

		// optional properties
		if (template.BuildRestrictions.Distance)
		{
			ret.buildRestrictions.distance = {
				"fromClass": template.BuildRestrictions.Distance.FromClass,
			};

			if (template.BuildRestrictions.Distance.MinDistance)
				ret.buildRestrictions.distance.min = getEntityValue("BuildRestrictions/Distance/MinDistance");

			if (template.BuildRestrictions.Distance.MaxDistance)
				ret.buildRestrictions.distance.max = getEntityValue("BuildRestrictions/Distance/MaxDistance");
		}
	}

	if (template.TrainingRestrictions)
	{
		ret.trainingRestrictions = {
			"category": template.TrainingRestrictions.Category
		};
		if (template.TrainingRestrictions.MatchLimit)
			ret.trainingRestrictions.matchLimit = +template.TrainingRestrictions.MatchLimit;
	}

	if (template.Cost)
	{
		ret.cost = {};
		for (const resCode in template.Cost.Resources)
			ret.cost[resCode] = getEntityValue("Cost/Resources/" + resCode);

		if (template.Cost.Population)
			ret.cost.population = getEntityValue("Cost/Population");

		if (template.Cost.BuildTime)
			ret.cost.time = getEntityValue("Cost/BuildTime");
	}

	if (template.Footprint)
	{
		ret.footprint = { "height": template.Footprint.Height };

		if (template.Footprint.Square)
			ret.footprint.square = {
				"width": +template.Footprint.Square["@width"],
				"depth": +template.Footprint.Square["@depth"]
			};
		else if (template.Footprint.Circle)
			ret.footprint.circle = { "radius": +template.Footprint.Circle["@radius"] };
		else
			warn("GetTemplateDataHelper(): Unrecognized Footprint type");
	}

	if (template.Garrisonable)
		ret.garrisonable = {
			"size": getEntityValue("Garrisonable/Size")
		};

	if (template.GarrisonHolder)
	{
		ret.garrisonHolder = {
			"buffHeal": getEntityValue("GarrisonHolder/BuffHeal")
		};

		if (template.GarrisonHolder.Max)
			ret.garrisonHolder.capacity = getEntityValue("GarrisonHolder/Max");
	}

	if (template.Heal)
		ret.heal = {
			"health": getEntityValue("Heal/Health"),
			"range": getEntityValue("Heal/Range"),
			"interval": getEntityValue("Heal/Interval")
		};

	if (template.ResourceGatherer)
	{
		ret.resourceGatherRates = {};
		const baseSpeed = getEntityValue("ResourceGatherer/BaseSpeed");
		for (const type in template.ResourceGatherer.Rates)
			ret.resourceGatherRates[type] = getEntityValue("ResourceGatherer/Rates/"+ type) * baseSpeed;
	}

	if (template.ResourceDropsite)
		ret.resourceDropsite = {
			"types": template.ResourceDropsite.Types.split(" ")
		};

	if (template.ResourceTrickle)
	{
		ret.resourceTrickle = {
			"interval": +template.ResourceTrickle.Interval,
			"rates": {}
		};
		for (const type in template.ResourceTrickle.Rates)
			ret.resourceTrickle.rates[type] = getEntityValue("ResourceTrickle/Rates/" + type);
	}

	if (template.Loot)
	{
		ret.loot = {};
		for (const type in template.Loot)
			ret.loot[type] = getEntityValue("Loot/"+ type);
	}

	if (template.Obstruction)
	{
		ret.obstruction = {
			"active": ("" + template.Obstruction.Active == "true"),
			"blockMovement": ("" + template.Obstruction.BlockMovement == "true"),
			"blockPathfinding": ("" + template.Obstruction.BlockPathfinding == "true"),
			"blockFoundation": ("" + template.Obstruction.BlockFoundation == "true"),
			"blockConstruction": ("" + template.Obstruction.BlockConstruction == "true"),
			"disableBlockMovement": ("" + template.Obstruction.DisableBlockMovement == "true"),
			"disableBlockPathfinding": ("" + template.Obstruction.DisableBlockPathfinding == "true"),
			"shape": {}
		};

		if (template.Obstruction.Static)
		{
			ret.obstruction.shape.type = "static";
			ret.obstruction.shape.width = +template.Obstruction.Static["@width"];
			ret.obstruction.shape.depth = +template.Obstruction.Static["@depth"];
		}
		else if (template.Obstruction.Unit)
		{
			ret.obstruction.shape.type = "unit";
			ret.obstruction.shape.radius = +template.Obstruction.Unit["@radius"];
		}
		else
			ret.obstruction.shape.type = "cluster";
	}

	if (template.Pack)
		ret.pack = {
			"state": template.Pack.State,
			"time": getEntityValue("Pack/Time"),
		};

	if (template.Population && template.Population.Bonus)
		ret.population = {
			"bonus": getEntityValue("Population/Bonus")
		};

	if (template.Health)
		ret.health = Math.round(getEntityValue("Health/Max"));

	if (template.Identity)
	{
		ret.selectionGroupName = template.Identity.SelectionGroupName;
		ret.name = {
			"specific": (template.Identity.SpecificName || template.Identity.GenericName),
			"generic": template.Identity.GenericName
		};
		ret.icon = template.Identity.Icon;
		ret.tooltip = template.Identity.Tooltip;
		ret.requirements = template.Identity.Requirements;
		ret.visibleIdentityClasses = GetVisibleIdentityClasses(template.Identity);
		ret.nativeCiv = template.Identity.Civ;
	}

	if (template.UnitMotion)
	{
		const walkSpeed = getEntityValue("UnitMotion/WalkSpeed");
		ret.speed = {
			"walk": walkSpeed,
			"run": walkSpeed,
			"acceleration": getEntityValue("UnitMotion/Acceleration")
		};
		if (template.UnitMotion.RunMultiplier)
			ret.speed.run *= getEntityValue("UnitMotion/RunMultiplier");
	}

	if (template.Upgrade)
	{
		ret.upgrades = [];
		for (const upgradeName in template.Upgrade)
		{
			const upgrade = template.Upgrade[upgradeName];

			const cost = {};
			if (upgrade.Cost)
				for (const res in upgrade.Cost)
					cost[res] = getEntityValue("Upgrade/" + upgradeName + "/Cost/" + res, "Upgrade/Cost/" + res);
			if (upgrade.Time)
				cost.time = getEntityValue("Upgrade/" + upgradeName + "/Time", "Upgrade/Time");

			ret.upgrades.push({
				"entity": upgrade.Entity,
				"tooltip": upgrade.Tooltip,
				"cost": cost,
				"icon": upgrade.Icon,
				"requirements": upgrade.Requirements
			});
		}
	}

	if (template.Researcher)
	{
		ret.techCostMultiplier = {};
		for (const res of resources.GetCodes().concat(["time"]))
			ret.techCostMultiplier[res] = getEntityValue("Researcher/TechCostMultiplier/" + res, null, 1);
	}

	if (template.Trader)
		ret.trader = {
			"GainMultiplier": getEntityValue("Trader/GainMultiplier")
		};

	if (template.Treasure)
	{
		ret.treasure = {
			"collectTime": getEntityValue("Treasure/CollectTime"),
			"resources": {}
		};
		for (const resource in template.Treasure.Resources)
			ret.treasure.resources[resource] = getEntityValue("Treasure/Resources/" + resource);
	}

	if (template.TurretHolder)
		ret.turretHolder = {
			"turretPoints": template.TurretHolder.TurretPoints
		};

	if (template.Upkeep)
	{
		ret.upkeep = {
			"interval": +template.Upkeep.Interval,
			"rates": {}
		};
		for (const type in template.Upkeep.Rates)
			ret.upkeep.rates[type] = getEntityValue("Upkeep/Rates/" + type);
	}

	if (template.WallSet)
	{
		ret.wallSet = {
			"templates": {
				"tower": template.WallSet.Templates.Tower,
				"gate": template.WallSet.Templates.Gate,
				"fort": template.WallSet.Templates.Fort || "structures/" + template.Identity.Civ + "/fortress",
				"long": template.WallSet.Templates.WallLong,
				"medium": template.WallSet.Templates.WallMedium,
				"short": template.WallSet.Templates.WallShort
			},
			"maxTowerOverlap": +template.WallSet.MaxTowerOverlap,
			"minTowerOverlap": +template.WallSet.MinTowerOverlap
		};
		if (template.WallSet.Templates.WallEnd)
			ret.wallSet.templates.end = template.WallSet.Templates.WallEnd;
		if (template.WallSet.Templates.WallCurves)
			ret.wallSet.templates.curves = template.WallSet.Templates.WallCurves.split(/\s+/);
	}

	if (template.WallPiece)
		ret.wallPiece = {
			"length": +template.WallPiece.Length,
			"angle": +(template.WallPiece.Orientation || 1) * Math.PI,
			"indent": +(template.WallPiece.Indent || 0),
			"bend": +(template.WallPiece.Bend || 0) * Math.PI
		};

	return ret;
}

/**
 * Get basic information about a technology template.
 * @param {Object} template - A valid template as obtained by loading the tech JSON file.
 * @param {string} civ - Civilization for which the tech requirements should be calculated.
 */
function GetTechnologyBasicDataHelper(template, civ)
{
	return {
		"name": {
			"generic": template.genericName
		},
		"icon": template.icon ? "technologies/" + template.icon : undefined,
		"description": template.description,
		"reqs": DeriveTechnologyRequirements(template, civ),
		"modifications": template.modifications,
		"affects": template.affects,
		"replaces": template.replaces
	};
}

/**
 * Get information about a technology template.
 * @param {Object} template - A valid template as obtained by loading the tech JSON file.
 * @param {string} civ - Civilization for which the specific name and tech requirements should be returned.
 * @param {Object} resources - An instance of the Resources class.
 */
function GetTechnologyDataHelper(template, civ, resources)
{
	const ret = GetTechnologyBasicDataHelper(template, civ);

	if (template.specificName)
		ret.name.specific = template.specificName[civ] || template.specificName.generic;

	ret.cost = { "time": template.researchTime ? +template.researchTime : 0 };
	for (const type of resources.GetCodes())
		ret.cost[type] = +(template.cost && template.cost[type] || 0);

	ret.tooltip = template.tooltip;
	ret.requirementsTooltip = template.requirementsTooltip || "";

	return ret;
}

/**
 * Get information about an aura template.
 * @param {object} template - A valid template as obtained by loading the aura JSON file.
 */
function GetAuraDataHelper(template)
{
	return {
		"name": {
			"generic": template.auraName,
		},
		"description": template.auraDescription || null,
		"modifications": template.modifications,
		"radius": template.radius || null,
	};
}

function calculateCarriedResources(carriedResources, tradingGoods)
{
	var resources = {};

	if (carriedResources)
		for (const resource of carriedResources)
			resources[resource.type] = (resources[resource.type] || 0) + resource.amount;

	if (tradingGoods && tradingGoods.amount)
		resources[tradingGoods.type] =
			(resources[tradingGoods.type] || 0) +
			(tradingGoods.amount.traderGain || 0) +
			(tradingGoods.amount.market1Gain || 0) +
			(tradingGoods.amount.market2Gain || 0);

	return resources;
}

/**
 * Remove filter prefix (mirage, corpse, etc) from template name.
 *
 * ie. filter|dir/to/template -> dir/to/template
 */
function removeFiltersFromTemplateName(templateName)
{
	return templateName.split("|").pop();
}
