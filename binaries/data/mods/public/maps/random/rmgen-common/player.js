/**
 * @file These functions locate and place the starting entities of players.
 */

var g_NomadTreasureTemplates = {
	"food": "gaia/treasure/food_jars",
	"wood": "gaia/treasure/wood",
	"stone": "gaia/treasure/stone",
	"metal": "gaia/treasure/metal"
};

/**
 * These are identifiers of functions that can generate parts of a player base.
 * There must be a function starting with placePlayerBase and ending with this name.
 * This is a global so mods can extend this from external files.
 */
var g_PlayerBaseFunctions = [
	// Possibly mark player class first here and use it afterwards
	"CityPatch",
	// Create the largest and most important entities first
	"Trees",
	"Mines",
	"Treasures",
	"Berries",
	"StartingAnimal",
	"Decoratives"
];

function isNomad()
{
	return !!g_MapSettings.Nomad;
}

function getNumPlayers()
{
	return g_MapSettings.PlayerData.length - 1;
}

function getCivCode(playerID)
{
	return g_MapSettings.PlayerData[playerID].Civ;
}

function areAllies(playerID1, playerID2)
{
	return g_MapSettings.PlayerData[playerID1].Team !== undefined &&
	       g_MapSettings.PlayerData[playerID2].Team !== undefined &&
	       g_MapSettings.PlayerData[playerID1].Team != -1 &&
	       g_MapSettings.PlayerData[playerID2].Team != -1 &&
	       g_MapSettings.PlayerData[playerID1].Team === g_MapSettings.PlayerData[playerID2].Team;
}

function getPlayerTeam(playerID)
{
	if (g_MapSettings.PlayerData[playerID].Team === undefined)
		return -1;

	return g_MapSettings.PlayerData[playerID].Team;
}

/**
 * Gets the default starting entities for the civ of the given player, as defined by the civ file.
 */
function getStartingEntities(playerID)
{
	return g_CivData[getCivCode(playerID)].StartEntities;
}

/**
 * Places the given entities at the given location (typically a civic center and starting units).
 * @param location - A Vector2D specifying tile coordinates.
 * @param civEntities - An array of objects with the Template property and optionally a Count property.
 * The first entity is placed in the center, the other ones surround it.
 */
function placeStartingEntities(location, playerID, civEntities, dist = 6, orientation = BUILDING_ORIENTATION)
{
	// Place the central structure
	let i = 0;
	const firstTemplate = civEntities[i].Template;
	if (firstTemplate.startsWith("structures/"))
	{
		g_Map.placeEntityPassable(firstTemplate, playerID, location, orientation);
		++i;
	}

	// Place entities surrounding it
	const space = 2;
	for (let j = i; j < civEntities.length; ++j)
	{
		const angle = orientation - Math.PI * (1 - j / 2);
		const count = civEntities[j].Count || 1;

		for (let num = 0; num < count; ++num)
		{
			const position = Vector2D.sum([
				location,
				new Vector2D(dist, 0).rotate(-angle),
				new Vector2D(space * (-num + (count - 1) / 2), 0).rotate(angle)
			]);

			g_Map.placeEntityPassable(civEntities[j].Template, playerID, position, angle);
		}
	}
}

/**
 * Places the default starting entities as defined by the civilization definition, optionally including city walls.
 */
function placeCivDefaultStartingEntities(position, playerID, wallType, dist = 6, orientation = BUILDING_ORIENTATION)
{
	placeStartingEntities(position, playerID, getStartingEntities(playerID), dist, orientation);
	placeStartingWalls(position, playerID, wallType, orientation);
}

/**
 * If the map is large enough and the civilization defines them, places the initial city walls or towers.
 * @param {string|boolean} wallType - Either "towers" to only place the wall turrets or a boolean indicating enclosing city walls.
 */
function placeStartingWalls(position, playerID, wallType, orientation = BUILDING_ORIENTATION)
{
	const civ = getCivCode(playerID);
	if (civ != "iber" || g_Map.getSize() <= 128)
		return;

	// TODO: should prevent trees inside walls
	// When fixing, remove the DeleteUponConstruction flag from template_gaia_flora.xml

	if (wallType == "towers")
		placePolygonalWall(position, 15, ["entry"], "tower", civ, playerID, orientation, 7);
	else if (wallType)
		placeGenericFortress(position, 20, playerID);
}

/**
 * Places the civic center and starting resources for all given players.
 */
function placePlayerBases(playerBaseArgs)
{
	g_Map.log("Creating playerbases");

	const [playerIDs, playerPosition] = playerBaseArgs.PlayerPlacement;

	for (let i = 0; i < getNumPlayers(); ++i)
	{
		playerBaseArgs.playerID = playerIDs[i];
		playerBaseArgs.playerPosition = playerPosition[i];
		placePlayerBase(playerBaseArgs);
	}
}

/**
 * Places the civic center and starting resources.
 */
function placePlayerBase(playerBaseArgs)
{
	if (isNomad())
		return;

	placeCivDefaultStartingEntities(
		playerBaseArgs.playerPosition,
		playerBaseArgs.playerID,
		playerBaseArgs.Walls !== undefined ? playerBaseArgs.Walls : true,
		6,
		BUILDING_ORIENTATION);

	if (playerBaseArgs.PlayerTileClass)
		addCivicCenterAreaToClass(playerBaseArgs.playerPosition, playerBaseArgs.PlayerTileClass);

	for (const functionID of g_PlayerBaseFunctions)
	{
		const funcName = "placePlayerBase" + functionID;
		const func = global[funcName];
		if (!func)
			throw new Error("Could not find " + funcName);

		if (!playerBaseArgs[functionID])
			continue;

		const args = playerBaseArgs[functionID];

		// Copy some global arguments to the arguments for each function
		for (const prop of ["playerID", "playerPosition", "BaseResourceClass", "baseResourceConstraint"])
			args[prop] = playerBaseArgs[prop];

		func(args);
	}
}

function defaultPlayerBaseRadius()
{
	return scaleByMapSize(15, 25);
}

/**
 * Marks the corner and center tiles of an area that is about the size of a Civic Center with the given TileClass.
 * Used to prevent resource collisions with the Civic Center.
 */
function addCivicCenterAreaToClass(position, tileClass)
{
	createArea(
		new DiskPlacer(5, position),
		new TileClassPainter(tileClass));
}

/**
 * Helper function.
 */
function getPlayerBaseArgs(playerBaseArgs)
{
	let baseResourceConstraint = playerBaseArgs.BaseResourceClass && avoidClasses(playerBaseArgs.BaseResourceClass, 4);

	if (playerBaseArgs.baseResourceConstraint)
		baseResourceConstraint = new AndConstraint([baseResourceConstraint, playerBaseArgs.baseResourceConstraint]);

	return [
		(property, defaultVal) => playerBaseArgs[property] === undefined ? defaultVal : playerBaseArgs[property],
		playerBaseArgs.playerPosition,
		baseResourceConstraint
	];
}

function placePlayerBaseCityPatch(args)
{
	const [get, basePosition, baseResourceConstraint] = getPlayerBaseArgs(args);

	let painters = [];

	if (args.outerTerrain && args.innerTerrain)
		painters.push(new LayeredPainter([args.outerTerrain, args.innerTerrain], [get("width", 1)]));

	if (args.painters)
		painters = painters.concat(args.painters);

	createArea(
		new ClumpPlacer(
			Math.floor(diskArea(get("radius", defaultPlayerBaseRadius() / 3))),
			get("coherence", 0.6),
			get("smoothness", 0.3),
			get("failFraction", Infinity),
			basePosition),
		painters);
}

function placePlayerBaseStartingAnimal(args)
{
	const [get, basePosition, baseResourceConstraint] = getPlayerBaseArgs(args);

	const template = get("template", "gaia/fauna_chicken");
	const count = template === "gaia/fauna_chicken" ? 5 :
		Math.round(5 * (Engine.GetTemplate("gaia/fauna_chicken").ResourceSupply.Max / Engine.GetTemplate(get("template")).ResourceSupply.Max));

	for (let i = 0; i < get("groupCount", 2); ++i)
	{
		let success = false;
		for (let tries = 0; tries < get("maxTries", 30); ++tries)
		{
			const position = new Vector2D(0, get("distance", 9)).rotate(randomAngle()).add(basePosition);
			if (createObjectGroup(
				new SimpleGroup(
					[
						new SimpleObject(
							template,
							get("minGroupCount", count),
							get("maxGroupCount", count),
							get("minGroupDistance", 0),
							get("maxGroupDistance", 2))
					],
					true,
					args.BaseResourceClass,
					position),
				0,
				baseResourceConstraint))
			{
				success = true;
				break;
			}
		}

		if (!success)
		{
			error("Could not place startingAnimal for player " + args.playerID);
			return;
		}
	}
}

function placePlayerBaseBerries(args)
{
	const [get, basePosition, baseResourceConstraint] = getPlayerBaseArgs(args);
	for (let tries = 0; tries < get("maxTries", 30); ++tries)
	{
		const position = new Vector2D(0, get("distance", 12)).rotate(randomAngle()).add(basePosition);
		if (createObjectGroup(
			new SimpleGroup(
				[new SimpleObject(args.template, get("minCount", 5), get("maxCount", 5), get("maxDist", 1), get("maxDist", 3))],
				true,
				args.BaseResourceClass,
				position),
			0,
			baseResourceConstraint))
			return;
	}

	error("Could not place berries for player " + args.playerID);
}

function placePlayerBaseMines(args)
{
	const [get, basePosition, baseResourceConstraint] = getPlayerBaseArgs(args);

	const angleBetweenMines = randFloat(get("minAngle", Math.PI / 6), get("maxAngle", Math.PI / 3));
	const mineCount = args.types.length;

	let groupElements = [];
	if (args.groupElements)
		groupElements = groupElements.concat(args.groupElements);

	for (let tries = 0; tries < get("maxTries", 75); ++tries)
	{
		// First find a place where all mines can be placed
		let pos = [];
		const startAngle = randomAngle();
		for (let i = 0; i < mineCount; ++i)
		{
			const angle = startAngle + angleBetweenMines * (i + (mineCount - 1) / 2);
			pos[i] = new Vector2D(0, get("distance", 12)).rotate(angle).add(basePosition).round();
			if (!g_Map.validTilePassable(pos[i]) || !baseResourceConstraint.allows(pos[i]))
			{
				pos = undefined;
				break;
			}
		}

		if (!pos)
			continue;

		// Place the mines
		for (let i = 0; i < mineCount; ++i)
		{
			if (args.types[i].type && args.types[i].type == "stone_formation")
			{
				createStoneMineFormation(pos[i], args.types[i].template, args.types[i].terrain);
				args.BaseResourceClass.add(pos[i]);
				continue;
			}

			createObjectGroup(
				new SimpleGroup(
					[new SimpleObject(args.types[i].template, 1, 1, 0, 0)].concat(groupElements),
					true,
					args.BaseResourceClass,
					pos[i]),
				0);
		}
		return;
	}

	error("Could not place mines for player " + args.playerID);
}

function placePlayerBaseTrees(args)
{
	const [get, basePosition, baseResourceConstraint] = getPlayerBaseArgs(args);

	const num = Math.floor(get("count", scaleByMapSize(7, 20)));

	for (let x = 0; x < get("maxTries", 30); ++x)
	{
		const position = new Vector2D(0, randFloat(get("minDist", 11), get("maxDist", 13))).rotate(randomAngle()).add(basePosition).round();

		if (createObjectGroup(
			new SimpleGroup(
				[new SimpleObject(args.template, num, num, get("minDistGroup", 0), get("maxDistGroup", 5))],
				false,
				args.BaseResourceClass,
				position),
			0,
			baseResourceConstraint))
			return;
	}

	error("Could not place starting trees for player " + args.playerID);
}

function placePlayerBaseTreasures(args)
{
	const [_, basePosition, baseResourceConstraint] = getPlayerBaseArgs(args);

	for (const resourceTypeArgs of args.types)
	{
		const get = (property, defaultVal) => resourceTypeArgs[property] === undefined ? defaultVal : resourceTypeArgs[property];

		let success = false;

		for (let tries = 0; tries < get("maxTries", 30); ++tries)
		{
			const position = new Vector2D(0, randFloat(get("minDist", 11), get("maxDist", 13))).rotate(randomAngle()).add(basePosition).round();

			if (createObjectGroup(
				new SimpleGroup(
					[new SimpleObject(resourceTypeArgs.template, get("count", 14), get("count", 14), get("minDistGroup", 1), get("maxDistGroup", 3))],
					false,
					args.BaseResourceClass,
					position),
				0,
				baseResourceConstraint))
			{
				success = true;
				break;
			}
		}
		if (!success)
		{
			error("Could not place treasure " + resourceTypeArgs.template + " for player " + args.playerID);
			return;
		}
	}
}

/**
 * Typically used for placing grass tufts around the civic centers.
 */
function placePlayerBaseDecoratives(args)
{
	const [get, basePosition, baseResourceConstraint] = getPlayerBaseArgs(args);

	for (let i = 0; i < get("count", scaleByMapSize(2, 5)); ++i)
	{
		let success = false;
		for (let x = 0; x < get("maxTries", 30); ++x)
		{
			const position = new Vector2D(0, randIntInclusive(get("minDist", 8), get("maxDist", 11))).rotate(randomAngle()).add(basePosition).round();

			if (createObjectGroup(
				new SimpleGroup(
					[new SimpleObject(args.template, get("minCount", 2), get("maxCount", 5), 0, 1)],
					false,
					args.BaseResourceClass,
					position),
				0,
				baseResourceConstraint))
			{
				success = true;
				break;
			}
		}
		if (!success)
			// Don't warn since the decoratives are not important
			return;
	}
}

function placePlayersNomad(playerClass, constraints)
{
	if (!isNomad())
		return undefined;

	g_Map.log("Placing nomad starting units");

	const distance = scaleByMapSize(60, 240);
	const constraint = new StaticConstraint(constraints);

	const numPlayers = getNumPlayers();
	const playerIDs = shuffleArray(getPlayerIDs());
	const playerPosition = [];

	for (let i = 0; i < numPlayers; ++i)
	{
		const objects = getStartingEntities(playerIDs[i]).filter(ents => ents.Template.startsWith("units/")).map(
			ents => new SimpleObject(ents.Template, ents.Count || 1, ents.Count || 1, 1, 3));

		// Add treasure if too few resources for a civic center
		const ccCost = Engine.GetTemplate("structures/" + getCivCode(playerIDs[i]) + "/civil_centre").Cost.Resources;
		for (const resourceType in ccCost)
		{
			const treasureTemplate = g_NomadTreasureTemplates[resourceType];

			const count = Math.max(0, Math.ceil(
				(ccCost[resourceType] - (g_MapSettings.StartingResources || 0)) /
				Engine.GetTemplate(treasureTemplate).Treasure.Resources[resourceType]));

			objects.push(new SimpleObject(treasureTemplate, count, count, 3, 5));
		}

		// Try place these entities at a random location
		const group = new SimpleGroup(objects, true, playerClass);
		let success = false;
		for (const distanceFactor of [1, 1/2, 1/4, 0])
			if (createObjectGroups(group, playerIDs[i], new AndConstraint([constraint, avoidClasses(playerClass, distance * distanceFactor)]), 1, 200, false).length)
			{
				success = true;
				playerPosition[i] = group.centerPosition;
				break;
			}

		if (!success)
			throw new Error("Could not place starting units for player " + playerIDs[i] + "!");
	}

	return [playerIDs, playerPosition];
}

/**
 * Get the player IDs
 *
 * @returns {Array} - an array with sequential integers from 1 to the number of players
 */
function getPlayerIDs()
{
	return Array.from(Array(getNumPlayers()), (_, index) => index + 1);
}

/**
 * Sorts an array of player IDs by team index.
 * Players without teams come first. Randomize order for players of the same team.
 */
function sortPlayers(playerIDs)
{
	return shuffleArray(playerIDs).sort((playerID1, playerID2) => getPlayerTeam(playerID1) - getPlayerTeam(playerID2));
}

/**
 * Randomize playerIDs but sort by team.
 *
 * @returns {Array} - every item is an array of player indices
 */
function sortAllPlayers()
{
	return sortPlayers(getPlayerIDs());
}

/**
 * Rearrange order so that teams of neighboring players alternate (if the given IDs are sorted by team).
 */
function primeSortPlayers(playerIDs)
{
	const prime = [];
	for (let i = 0; i < Math.floor(playerIDs.length / 2); ++i)
	{
		prime.push(playerIDs[i]);
		prime.push(playerIDs[playerIDs.length - 1 - i]);
	}

	if (playerIDs.length % 2)
		prime.push(playerIDs[Math.floor(playerIDs.length / 2)]);

	return prime;
}

function primeSortAllPlayers()
{
	return primeSortPlayers(sortAllPlayers());
}

/*
 * Separates playerIDs into two arrays such that teammates are in the same array,
 * unless everyone's on the same team in which case they'll be split in half.
 */
function partitionPlayers(playerIDs)
{
	const teamIDs = Array.from(new Set(playerIDs.map(getPlayerTeam)));
	let teams = teamIDs.map(teamID => playerIDs.filter(playerID => getPlayerTeam(playerID) == teamID));
	if (teamIDs.indexOf(-1) != -1)
		teams = teams.concat(teams.splice(teamIDs.indexOf(-1), 1)[0].map(playerID => [playerID]));

	if (teams.length == 1)
	{
		const idx = Math.floor(teams[0].length / 2);
		teams = [teams[0].slice(idx), teams[0].slice(0, idx)];
	}

	teams.sort((a, b) => b.length - a.length);

	// Use the greedy algorithm: add the next team to the side with fewer players
	return teams.reduce(([east, west], team) =>
		east.length > west.length ?
			[east, west.concat(team)] :
			[east.concat(team), west],
	[[], []]);
}

/**
 * Return an array where each element is an array of playerIndices of a team.
 */
function getTeamsArray()
{
	const playerIDs = getPlayerIDs();
	const numPlayers = getNumPlayers();

	// Group players by team
	const teams = [];
	for (let i = 0; i < numPlayers; ++i)
	{
		const team = getPlayerTeam(playerIDs[i]);
		if (team == -1)
			continue;

		if (!teams[team])
			teams[team] = [];

		teams[team].push(playerIDs[i]);
	}

	// Players without a team get a custom index
	for (let i = 0; i < numPlayers; ++i)
		if (getPlayerTeam(playerIDs[i]) == -1)
			teams.push([playerIDs[i]]);

	// Remove unused indices
	return teams.filter(team => true);
}

/**
 * Determine player starting positions based on the specified pattern.
 */
function playerPlacementByPattern(patternName, distance = undefined, groupedDistance = undefined, angle = undefined, center = undefined)
{
	if (patternName === undefined)
		patternName = g_MapSettings.PlayerPlacement;

	switch (patternName)
	{
	case "circle":
		return playerPlacementCircle(distance, angle, center).slice(0, 2);
	case "river":
		return playerPlacementRiver(angle, distance, center);
	case "groupedLines":
		return placeLine(getTeamsArray(), distance, groupedDistance, angle);
	case "stronghold":
		return placeStronghold(getTeamsArray(), distance, groupedDistance, angle);
	case "randomGroup":
		return playerPlacementRandom(getPlayerIDs(), undefined);
	default:
		throw new Error("Unknown placement pattern: " + patternName);
	}
}

/**
 * Determine player starting positions on a circular pattern.
 */
function playerPlacementCircle(radius, startingAngle = undefined, center = undefined)
{
	const startAngle = startingAngle !== undefined ? startingAngle : randomAngle();
	const [playerPosition, playerAngle] = distributePointsOnCircle(getNumPlayers(), startAngle, radius, center || g_Map.getCenter());
	return [getPlayerIDs(), playerPosition.map(p => p.round()), playerAngle, startAngle];
}

/**
 * Determine player starting positions on a circular pattern, with a custom angle for each player.
 * Commonly used for gulf terrains.
 */
function playerPlacementCustomAngle(radius, center, playerAngleFunc)
{
	const playerPosition = [];
	const playerAngle = [];

	const numPlayers = getNumPlayers();

	for (let i = 0; i < numPlayers; ++i)
	{
		playerAngle[i] = playerAngleFunc(i);
		playerPosition[i] = Vector2D.add(center, new Vector2D(radius, 0).rotate(-playerAngle[i])).round();
	}

	return [playerPosition, playerAngle];
}

/**
 * Returns player starting positions equally spaced along an arc.
 */
function playerPlacementArc(playerIDs, center, radius, startAngle, endAngle)
{
	return distributePointsOnCircularSegment(
		playerIDs.length + 2,
		endAngle - startAngle,
		startAngle,
		radius,
		center
	)[0].slice(1, -1).map(p => p.round());
}

/**
 * Returns player starting positions located on two symmetrically placed arcs, with teammates placed on the same arc.
 */
function playerPlacementArcs(playerIDs, center, radius, mapAngle, startAngle, endAngle)
{
	const [east, west] = partitionPlayers(playerIDs);
	const eastPosition = playerPlacementArc(east, center, radius, mapAngle + startAngle, mapAngle + endAngle);
	const westPosition = playerPlacementArc(west, center, radius, mapAngle - startAngle, mapAngle - endAngle);
	return playerIDs.map(playerID => east.indexOf(playerID) != -1 ?
		eastPosition[east.indexOf(playerID)] :
		westPosition[west.indexOf(playerID)]);
}

/**
 * Returns player starting positions located on two parallel lines, typically used by central river maps.
 * If there are two teams with an equal number of players, each team will occupy exactly one line.
 * Angle 0 means the players are placed in north to south direction, i.e. along the Z axis.
 */
function playerPlacementRiver(angle, width, center = undefined)
{
	const numPlayers = getNumPlayers();
	const numPlayersEven = numPlayers % 2 == 0;
	const mapSize = g_Map.getSize();
	const centerPosition = center || g_Map.getCenter();
	const playerPosition = [];

	for (let i = 0; i < numPlayers; ++i)
	{
		const currentPlayerEven = i % 2 == 0;

		const offsetDivident = numPlayersEven || currentPlayerEven ? (i + 1) % 2 : 0;
		const offsetDivisor = numPlayersEven ? 0 : currentPlayerEven ? +1 : -1;

		playerPosition[i] = new Vector2D(
			width * (i % 2) + (mapSize - width) / 2,
			fractionToTiles(((i - 1 + offsetDivident) / 2 + 1) / ((numPlayers + offsetDivisor) / 2 + 1))
		).rotateAround(angle, centerPosition).round();
	}

	return groupPlayersByArea(new Array(numPlayers).fill(0).map((_p, i) => i + 1), playerPosition);
}

/**
 * Returns starting positions located on two parallel lines.
 * The locations on the first line are shifted in comparison to the other line.
 */
function playerPlacementLine(angle, center, width)
{
	const playerPosition = [];
	const numPlayers = getNumPlayers();

	for (let i = 0; i < numPlayers; ++i)
		playerPosition[i] = Vector2D.add(
			center,
			new Vector2D(
				fractionToTiles((i + 1) / (numPlayers + 1) - 0.5),
				width * (i % 2 - 1/2)
			).rotate(angle)
		).round();

	return playerPosition;
}

/**
 * Place teams in a line-pattern.
 *
 * @param {Array} playerIDs - typically randomized indices of players of a single team
 * @param {number} distance - radial distance from the center of the map
 * @param {number} groupedDistance - distance between players
 * @param {number} startAngle - determined by the map that might want to place something between players.
 *
 * @returns {Array} - contains id, angle, x, z for every player
 */
function placeLine(teamsArray, distance, groupedDistance, startAngle)
{
	const playerIDs = [];
	const playerPosition = [];

	const mapCenter = g_Map.getCenter();
	const numAcross = 2 * getNumPlayers() / teamsArray.length; // if its two teams, numAcross is the same as numPlayers.
	const dist = fractionToTiles(numAcross == 2 ? 0.45 : 0.66 + (-0.01 * numAcross));
	groupedDistance *= (3.00 + (-0.225 * numAcross));
	for (let i = 0; i < teamsArray.length; ++i)
	{
		let safeDist = distance;
		if (distance + teamsArray[i].length * groupedDistance > dist)
			safeDist = dist - teamsArray[i].length * groupedDistance;

		const teamAngle = startAngle + (i + 1) * 2 * Math.PI / teamsArray.length;

		for (let p = 0; p < teamsArray[i].length; ++p)
		{
			playerIDs.push(teamsArray[i][p]);
			playerPosition.push(Vector2D.add(mapCenter, new Vector2D(safeDist + p * groupedDistance, 0).rotate(-teamAngle)).round());
		}
	}

	return [playerIDs, playerPosition];
}

/**
 * Place given players in a stronghold-pattern.
 *
 * @param teamsArray - each item is an array of playerIDs placed per stronghold
 * @param distance - radial distance from the center of the map
 * @param groupedDistance - distance between neighboring players
 * @param {number} startAngle - determined by the map that might want to place something between players
 */
function placeStronghold(teamsArray, distance, groupedDistance, startAngle)
{
	const mapCenter = g_Map.getCenter();

	const playerIDs = [];
	const playerPosition = [];

	for (let i = 0; i < teamsArray.length; ++i)
	{
		const teamAngle = startAngle + (i + 1) * 2 * Math.PI / teamsArray.length;
		const teamPosition = Vector2D.add(mapCenter, new Vector2D(distance * 0.8, 0).rotate(-teamAngle));
		let teamGroupDistance = groupedDistance * 1.2;

		// If we have a team of above average size, make sure they're spread out
		if (teamsArray[i].length > 4)
			teamGroupDistance = Math.max(fractionToTiles(0.08), groupedDistance);

		// If we have a solo player, place them on the center of the team's location
		if (teamsArray[i].length == 1)
			teamGroupDistance = fractionToTiles(0);

		// TODO: Ensure players are not placed outside of the map area, similar to placeLine

		// Create player base
		for (let p = 0; p < teamsArray[i].length; ++p)
		{
			const angle = startAngle + (p + 1) * 2 * Math.PI / teamsArray[i].length;
			playerIDs.push(teamsArray[i][p]);
			playerPosition.push(Vector2D.add(teamPosition, new Vector2D(teamGroupDistance, 0).rotate(-angle)).round());
		}
	}

	return [playerIDs, playerPosition];
}

/**
 * Returns a random location for each player that meets the given constraints and
 * orders the playerIDs so that players become grouped by team.
 */
function playerPlacementRandom(playerIDs, constraints = undefined)
{
	let locations = [];
	let attempts = 0;
	let resets = 0;

	const mapCenter = g_Map.getCenter();
	let playerMinDistSquared = Math.square(fractionToTiles(0.25));
	const borderDistance = fractionToTiles(0.08);

	const area = createArea(new MapBoundsPlacer(), undefined, new AndConstraint(constraints));

	for (let i = 0; i < getNumPlayers(); ++i)
	{
		const position = pickRandom(area.getPoints());
		if (!position)
			return undefined;

		// Minimum distance between initial bases must be a quarter of the map diameter
		if (locations.some(loc => loc.distanceToSquared(position) < playerMinDistSquared) ||
		    position.distanceToSquared(mapCenter) > Math.square(mapCenter.x - borderDistance))
		{
			--i;
			++attempts;

			// Reset if we're in what looks like an infinite loop
			if (attempts > 500)
			{
				locations = [];
				i = -1;
				attempts = 0;
				++resets;

				// Reduce minimum player distance progressively
				if (resets % 25 == 0)
					playerMinDistSquared *= 0.95;

				// If we only pick bad locations, stop trying to place randomly
				if (resets == 500)
					return undefined;
			}
			continue;
		}

		locations[i] = position;
	}
	return groupPlayersByArea(playerIDs, locations);
}

/**
 *  Pick locations from the given set so that teams end up grouped.
 */
function groupPlayersByArea(playerIDs, locations)
{
	playerIDs = sortPlayers(playerIDs);

	let minDist = Infinity;
	let minLocations;

	// Of all permutations of starting locations, find the one where
	// the sum of the distances between allies is minimal, weighted by teamsize.
	heapsPermute(shuffleArray(locations).slice(0, playerIDs.length), v => v.clone(), permutation => {
		let dist = 0;
		let teamDist = 0;
		let teamSize = 0;

		for (let i = 1; i < playerIDs.length; ++i)
		{
			const team1 = getPlayerTeam(playerIDs[i - 1]);
			const team2 = getPlayerTeam(playerIDs[i]);
			++teamSize;
			if (team1 != -1 && team1 == team2)
				teamDist += permutation[i - 1].distanceTo(permutation[i]);
			else
			{
				dist += teamDist / teamSize;
				teamDist = 0;
				teamSize = 0;
			}
		}

		if (teamSize)
			dist += teamDist / teamSize;

		if (dist < minDist)
		{
			minDist = dist;
			minLocations = permutation;
		}
	});

	return [playerIDs, minLocations];
}

/**
 * Sorts the playerIDs so that team members are as close as possible on a ring.
 */
function groupPlayersCycle(startLocations)
{
	const startLocationOrder = sortPointsShortestCycle(startLocations);

	const newStartLocations = [];
	for (let i = 0; i < startLocations.length; ++i)
		newStartLocations.push(startLocations[startLocationOrder[i]]);

	startLocations = newStartLocations;

	// Sort players by team
	let playerIDs = [];
	const teams = [];
	for (let i = 0; i < g_MapSettings.PlayerData.length - 1; ++i)
	{
		playerIDs.push(i+1);
		const t = g_MapSettings.PlayerData[i + 1].Team;
		if (teams.indexOf(t) == -1 && t !== undefined)
			teams.push(t);
	}

	playerIDs = sortPlayers(playerIDs);

	if (!teams.length)
		return [playerIDs, startLocations];

	// Minimize maximum distance between players within a team
	let minDistance = Infinity;
	let bestShift;
	for (let s = 0; s < playerIDs.length; ++s)
	{
		let maxTeamDist = 0;
		for (let pi = 0; pi < playerIDs.length - 1; ++pi)
		{
			const t1 = getPlayerTeam(playerIDs[(pi + s) % playerIDs.length]);

			if (teams.indexOf(t1) === -1)
				continue;

			for (let pj = pi + 1; pj < playerIDs.length; ++pj)
			{
				if (t1 != getPlayerTeam(playerIDs[(pj + s) % playerIDs.length]))
					continue;

				maxTeamDist = Math.max(
					maxTeamDist,
					Math.euclidDistance2D(
						startLocations[pi].x,
						startLocations[pi].y,
						startLocations[pj].x,
						startLocations[pj].y));
			}
		}

		if (maxTeamDist < minDistance)
		{
			minDistance = maxTeamDist;
			bestShift = s;
		}
	}

	if (bestShift)
	{
		const newPlayerIDs = [];
		for (let i = 0; i < playerIDs.length; ++i)
			newPlayerIDs.push(playerIDs[(i + bestShift) % playerIDs.length]);
		playerIDs = newPlayerIDs;
	}

	return [playerIDs, startLocations];
}
