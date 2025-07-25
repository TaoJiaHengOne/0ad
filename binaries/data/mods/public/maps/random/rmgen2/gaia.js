import { addElements, allAmounts, allMixes, allSizes } from "maps/random/rmgen2/setup.js";

var g_Props = {
	"barrels": "actor|props/special/eyecandy/barrels_buried.xml",
	"crate": "actor|props/special/eyecandy/crate_a.xml",
	"cart": "actor|props/special/eyecandy/handcart_1_broken.xml",
	"well": "actor|props/special/eyecandy/well_1_c.xml",
	"skeleton": "actor|props/special/eyecandy/skeleton.xml",
};

/**
 * Prevent circular patterns around the CC by marking a random chain of circles there to be ignored by bluffs.
 */
export function markPlayerAvoidanceArea(playerPosition, radius)
{
	for (const position of playerPosition)
		createArea(
			new ChainPlacer(3, 6, scaleByMapSize(25, 60), Infinity, position, radius),
			new TileClassPainter(g_TileClasses.bluffIgnore),
			undefined,
			scaleByMapSize(7, 14));

	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.bluffIgnore),
		new NearTileClassConstraint(g_TileClasses.baseResource, 5));
}

/**
 * Paints a ramp from the given positions to t
 * Bluffs might surround playerbases either entirely or unfairly.
 */
export function createBluffsPassages(playerPosition)
{
	g_Map.log("Creating passages towards the center");
	for (const position of playerPosition)
	{
		const successful = true;
		for (let tryCount = 0; tryCount < 80; ++tryCount)
		{
			const angle = position.angleTo(g_Map.getCenter()) + randFloat(-1, 1) * Math.PI / 2;
			const start = Vector2D.add(position, new Vector2D(defaultPlayerBaseRadius() * 0.7, 0).rotate(angle).perpendicular()).round();
			const end = Vector2D.add(position, new Vector2D(defaultPlayerBaseRadius() * randFloat(1.7, 2), 0).rotate(angle).perpendicular()).round();

			if (g_TileClasses.forest.has(end) || !stayClasses(g_TileClasses.bluff, 12).allows(end))
				continue;

			if ((g_Map.getHeight(end.clone().floor()) - g_Map.getHeight(start.clone().floor())) / start.distanceTo(end) > 1.5)
				continue;

			const area = createPassage({
				"start": start,
				"end": end,
				"startWidth": scaleByMapSize(10, 20),
				"endWidth": scaleByMapSize(10, 14),
				"smoothWidth": 3,
				"terrain": g_Terrains.mainTerrain,
				"tileClass": g_TileClasses.bluffsPassage
			});

			for (const point of area.getPoints())
				g_Map.deleteTerrainEntity(point);

			createArea(
				new MapBoundsPlacer(),
				new TerrainPainter(g_Terrains.cliff),
				[
					new StayAreasConstraint([area]),
					new SlopeConstraint(2, Infinity)
				]);

			break;
		}
	}
}

/**
 * Create bluffs, i.e. a slope hill reachable from ground level.
 * Fill it with wood, mines, animals and decoratives.
 *
 * @param {Array} constraint - where to place them
 * @param {number} size - size of the bluffs (1.2 would be 120% of normal)
 * @param {number} deviation - degree of deviation from the defined size (0.2 would be 20% plus/minus)
 * @param {number} fill - size of map to fill (1.5 would be 150% of normal)
 * @param {number} baseHeight - elevation of the floor, making the bluff reachable
 */
export function addBluffs(constraint, size, deviation, fill, baseHeight)
{
	g_Map.log("Creating bluffs");

	const elevation = 30;

	// Percent of the length of the bluff determining the entrance area
	const margin = 0.08;

	let constrastTerrain = g_Terrains.tier2Terrain;

	if (currentBiome() == "generic/india")
		constrastTerrain = g_Terrains.dirt;

	if (currentBiome() == "generic/autumn")
		constrastTerrain = g_Terrains.tier3Terrain;

	for (let i = 0; i < fill * 15; ++i)
	{
		const bluffDeviation = getRandomDeviation(size, deviation);

		// Pick a random bluff location and shape
		const areasBluff = createAreas(
			new ChainPlacer(5 * bluffDeviation, 7 * bluffDeviation, 100 * bluffDeviation, 0.5),
			undefined,
			constraint,
			1);

		if (!areasBluff.length || !areasBluff[0].getPoints().length)
			continue;

		// Get a random starting position for the baseline and the endline
		let angle = randIntInclusive(0, 3);
		let opposingAngle = (angle + 2) % 4;

		// Find the edges of the bluff
		let baseLine;
		let endLine;

		// If we can't access the bluff, try different angles
		let retries = 0;
		let bluffPassable = false;
		while (!bluffPassable && retries++ < 4)
		{
			baseLine = findClearLine(areasBluff[0], angle);
			endLine = findClearLine(areasBluff[0], opposingAngle);
			bluffPassable = isBluffPassable(areasBluff[0], baseLine, endLine);

			angle = (angle + 1) % 4;
			opposingAngle = (angle + 2) % 4;
		}

		if (!bluffPassable)
			continue;

		// Paint bluff texture and elevation
		createArea(
			new MapBoundsPlacer(),
			[
				new LayeredPainter([g_Terrains.mainTerrain, constrastTerrain], [5]),
				new SmoothElevationPainter(ELEVATION_MODIFY, elevation * bluffDeviation, 2),
				new TileClassPainter(g_TileClasses.bluff)
			],
			new StayAreasConstraint(areasBluff));

		const slopeLength = (1 - margin) * Vector2D.average([baseLine.start, baseLine.end]).distanceTo(Vector2D.average([endLine.start, endLine.end]));

		// Adjust the height of each point in the bluff
		for (const point of areasBluff[0].getPoints())
		{
			const dist = Math.abs(distanceOfPointFromLine(baseLine.start, baseLine.end, point));
			g_Map.setHeight(point, Math.max(g_Map.getHeight(point) * (1 - dist / slopeLength) - 2, baseHeight));
		}

		// Flatten all points adjacent to but not on the bluff
		createArea(
			new MapBoundsPlacer(),
			[
				new SmoothingPainter(1, 1, 1),
				new TerrainPainter(g_Terrains.mainTerrain)
			],
			new AdjacentToAreaConstraint(areasBluff));

		// Paint cliffs
		createArea(
			new MapBoundsPlacer(),
			new TerrainPainter(g_Terrains.cliff),
			[
				new StayAreasConstraint(areasBluff),
				new SlopeConstraint(2, Infinity)
			]);

		// Performance improvement
		createArea(
			new MapBoundsPlacer(),
			new TileClassPainter(g_TileClasses.bluffIgnore),
			new NearTileClassConstraint(g_TileClasses.bluff, 8));
	}

	addElements([
		{
			"func": addHills,
			"avoid": [
				g_TileClasses.hill, 3,
				g_TileClasses.player, 20,
				g_TileClasses.valley, 2,
				g_TileClasses.water, 2
			],
			"stay": [g_TileClasses.bluff, 3],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": allAmounts
		}
	]);

	addElements([
		{
			"func": addLayeredPatches,
			"avoid": [
				g_TileClasses.dirt, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 5],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		}
	]);

	addElements([
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.forest, 2,
				g_TileClasses.player, 12,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 5],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		}
	]);

	addElements([
		{
			"func": addProps,
			"avoid": [
				g_TileClasses.forest, 2,
				g_TileClasses.player, 12,
				g_TileClasses.prop, 40,
				g_TileClasses.water, 3
			],
			"stay": [
				g_TileClasses.bluff, 7,
				g_TileClasses.mountain, 7
			],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["scarce"]
		}
	]);

	addElements(shuffleArray([
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 18,
				g_TileClasses.metal, 5,
				g_TileClasses.mountain, 5,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 5,
				g_TileClasses.water, 2
			],
			"stay": [g_TileClasses.bluff, 6],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["normal", "many", "tons"]
		},
		{
			"func": addMetal,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 5,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 50,
				g_TileClasses.rock, 15,
				g_TileClasses.metal, 40,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 6],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["normal"]
		},
		{
			"func": addStone,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 5,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 50,
				g_TileClasses.rock, 40,
				g_TileClasses.metal, 15,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 6],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["normal"]
		}
	]));

	const savanna = currentBiome() == "generic/savanna";
	addElements(shuffleArray([
		{
			"func": addStragglerTrees,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 10,
				g_TileClasses.metal, 5,
				g_TileClasses.mountain, 1,
				g_TileClasses.player, 12,
				g_TileClasses.rock, 5,
				g_TileClasses.water, 5
			],
			"stay": [g_TileClasses.bluff, 6],
			"sizes": savanna ? ["big"] : allSizes,
			"mixes": savanna ? ["varied"] : allMixes,
			"amounts": savanna ? ["tons"] : ["normal", "many", "tons"]
		},
		{
			"func": addAnimals,
			"avoid": [
				g_TileClasses.animals, 20,
				g_TileClasses.forest, 5,
				g_TileClasses.mountain, 1,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 5,
				g_TileClasses.metal, 5,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 6],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["normal", "many", "tons"]
		},
		{
			"func": addBerries,
			"avoid": [
				g_TileClasses.berries, 50,
				g_TileClasses.forest, 5,
				g_TileClasses.metal, 10,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 10,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 6],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["normal", "many", "tons"]
		}
	]));
}

/**
 * Add grass, rocks and bushes.
 */
export function addDecoration(constraint, size, deviation, fill)
{
	g_Map.log("Creating decoration");

	const offset = getRandomDeviation(size, deviation);
	const decorations = [
		[
			new SimpleObject(g_Decoratives.rockMedium, offset, 3 * offset, 0, offset)
		],
		[
			new SimpleObject(g_Decoratives.rockLarge, offset, 2 * offset, 0, offset),
			new SimpleObject(g_Decoratives.rockMedium, offset, 3 * offset, 0, 2 * offset)
		],
		[
			new SimpleObject(g_Decoratives.grassShort, offset, 2 * offset, 0, offset)
		],
		[
			new SimpleObject(g_Decoratives.grass, 2 * offset, 4 * offset, 0, 1.8 * offset),
			new SimpleObject(g_Decoratives.grassShort, 3 * offset, 6 * offset, 1.2 * offset, 2.5 * offset)
		],
		[
			new SimpleObject(g_Decoratives.bushMedium, offset, 2 * offset, 0, 2 * offset),
			new SimpleObject(g_Decoratives.bushSmall, 2 * offset, 4 * offset, 0, 2 * offset)
		]
	];

	let baseCount = 1;
	if (currentBiome() == "generic/india")
		baseCount = 8;

	const counts = [
		scaleByMapSize(16, 262),
		scaleByMapSize(8, 131),
		baseCount * scaleByMapSize(13, 200),
		baseCount * scaleByMapSize(13, 200),
		baseCount * scaleByMapSize(13, 200)
	];

	for (let i = 0; i < decorations.length; ++i)
	{
		const decorCount = Math.floor(counts[i] * fill);
		const group = new SimpleGroup(decorations[i], true);
		createObjectGroupsDeprecated(group, 0, constraint, decorCount, 5);
	}
}

/**
 * Create varying elevations.
 *
 * @param {Array} constraint - avoid/stay-classes
 *
 * @param {Object} el - the element to be rendered, for example:
 *  "class": g_TileClasses.hill,
 *	"painter": [g_Terrains.mainTerrain, g_Terrains.mainTerrain],
 *	"size": 1,
 *	"deviation": 0.2,
 *	"fill": 1,
 *	"count": scaleByMapSize(4, 8),
 *	"minSize": Math.floor(scaleByMapSize(3, 8)),
 *	"maxSize": Math.floor(scaleByMapSize(5, 10)),
 *	"spread": Math.floor(scaleByMapSize(10, 20)),
 *	"minElevation": 6,
 *	"maxElevation": 12,
 *	"steepness": 1.5
 */

function addElevation(constraint, el)
{
	const count = el.fill * el.count;
	const minSize = el.minSize;
	const maxSize = el.maxSize;
	const spread = el.spread;

	let elType = ELEVATION_MODIFY;
	if (el.class == g_TileClasses.water)
		elType = ELEVATION_SET;

	const widths = [];

	// Allow for shore and cliff rendering
	for (let s = el.painter.length; s > 2; --s)
		widths.push(1);

	for (let i = 0; i < count; ++i)
	{
		const elevation = randIntExclusive(el.minElevation, el.maxElevation);
		const smooth = Math.floor(elevation / el.steepness);

		const offset = getRandomDeviation(el.size, el.deviation);
		let pMinSize = Math.floor(minSize * offset);
		let pMaxSize = Math.floor(maxSize * offset);
		const pSpread = Math.floor(spread * offset);
		let pSmooth = Math.abs(Math.floor(smooth * offset));
		let pElevation = Math.floor(elevation * offset);

		pElevation = Math.max(el.minElevation, Math.min(pElevation, el.maxElevation));
		pMinSize = Math.min(pMinSize, pMaxSize);
		pMaxSize = Math.min(pMaxSize, el.maxSize);
		pMinSize = Math.max(pMaxSize, el.minSize);
		pSmooth = Math.max(pSmooth, 1);

		createAreas(
			new ChainPlacer(pMinSize, pMaxSize, pSpread, 0.5),
			[
				new LayeredPainter(el.painter, [widths.concat(pSmooth)]),
				new SmoothElevationPainter(elType, pElevation, pSmooth),
				new TileClassPainter(el.class)
			],
			constraint,
			1);
	}
}

/**
 * Create rolling hills.
 */
export function addHills(constraint, size, deviation, fill)
{
	g_Map.log("Creating hills");

	addElevation(constraint, {
		"class": g_TileClasses.hill,
		"painter": [g_Terrains.mainTerrain, g_Terrains.mainTerrain],
		"size": size,
		"deviation": deviation,
		"fill": fill,
		"count": 8,
		"minSize": 5,
		"maxSize": 8,
		"spread": 20,
		"minElevation": 6,
		"maxElevation": 12,
		"steepness": 1.5
	});

	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.bluffIgnore),
		new NearTileClassConstraint(g_TileClasses.hill, 6));
}

/**
 * Create random lakes with fish in it.
 */
export function addLakes(constraint, size, deviation, fill)
{
	g_Map.log("Creating lakes");

	let lakeTile = g_Terrains.water;

	if (currentBiome() == "generic/temperate" || currentBiome() == "generic/india")
		lakeTile = g_Terrains.dirt;

	if (currentBiome() == "generic/aegean")
		lakeTile = g_Terrains.tier2Terrain;

	if (currentBiome() == "generic/autumn")
		lakeTile = g_Terrains.shore;

	addElevation(constraint, {
		"class": g_TileClasses.water,
		"painter": [lakeTile, lakeTile],
		"size": size,
		"deviation": deviation,
		"fill": fill,
		"count": 6,
		"minSize": 7,
		"maxSize": 9,
		"spread": 70,
		"minElevation": -15,
		"maxElevation": -2,
		"steepness": 1.5
	});

	addElements([
		{
			"func": addFish,
			"avoid": [
				g_TileClasses.fish, 12,
				g_TileClasses.hill, 8,
				g_TileClasses.mountain, 8,
				g_TileClasses.player, 8
			],
			"stay": [g_TileClasses.water, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["normal", "many", "tons"]
		}
	]);

	let group = new SimpleGroup([new SimpleObject(g_Decoratives.rockMedium, 1, 3, 1, 3)], true, g_TileClasses.dirt);
	createObjectGroupsDeprecated(group, 0, [stayClasses(g_TileClasses.water, 1), borderClasses(g_TileClasses.water, 4, 3)], 1000, 100);

	group = new SimpleGroup([new SimpleObject(g_Decoratives.reeds, 10, 15, 1, 3), new SimpleObject(g_Decoratives.rockMedium, 1, 3, 1, 3)], true, g_TileClasses.dirt);
	createObjectGroupsDeprecated(group, 0, [stayClasses(g_TileClasses.water, 2), borderClasses(g_TileClasses.water, 4, 3)], 1000, 100);
}

/**
 * Universal function to create layered patches.
 */
export function addLayeredPatches(constraint, size, deviation, fill)
{
	g_Map.log("Creating layered patches");

	const minRadius = 1;
	const maxRadius = Math.floor(scaleByMapSize(3, 5));
	const count = fill * scaleByMapSize(15, 45);

	const patchSizes = [
		scaleByMapSize(3, 6),
		scaleByMapSize(5, 10),
		scaleByMapSize(8, 21)
	];

	for (const patchSize of patchSizes)
	{
		const offset = getRandomDeviation(size, deviation);
		const patchMinRadius = Math.floor(minRadius * offset);
		const patchMaxRadius = Math.floor(maxRadius * offset);

		createAreas(
			new ChainPlacer(Math.min(patchMinRadius, patchMaxRadius), patchMaxRadius, Math.floor(patchSize * offset), 0.5),
			[
				new LayeredPainter(
					[
						[g_Terrains.mainTerrain, g_Terrains.tier1Terrain],
						[g_Terrains.tier1Terrain, g_Terrains.tier2Terrain],
						[g_Terrains.tier2Terrain, g_Terrains.tier3Terrain],
						[g_Terrains.tier4Terrain]
					],
					[1, 1]),
				new TileClassPainter(g_TileClasses.dirt)
			],
			constraint,
			count * offset);
	}
}

/**
 * Create steep mountains.
 */
export function addMountains(constraint, size, deviation, fill)
{
	g_Map.log("Creating mountains");

	addElevation(constraint, {
		"class": g_TileClasses.mountain,
		"painter": [g_Terrains.cliff, g_Terrains.hill],
		"size": size,
		"deviation": deviation,
		"fill": fill,
		"count": 8,
		"minSize": 2,
		"maxSize": 4,
		"spread": 100,
		"minElevation": 100,
		"maxElevation": 120,
		"steepness": 4
	});
}

/**
 * Create plateaus.
 */
export function addPlateaus(constraint, size, deviation, fill)
{
	g_Map.log("Creating plateaus");

	let plateauTile = g_Terrains.dirt;

	if (currentBiome() == "generic/arctic")
		plateauTile = g_Terrains.tier1Terrain;

	if (currentBiome() == "generic/alpine" || currentBiome() == "generic/savanna")
		plateauTile = g_Terrains.tier2Terrain;

	if (currentBiome() == "generic/autumn")
		plateauTile = g_Terrains.tier4Terrain;

	addElevation(constraint, {
		"class": g_TileClasses.plateau,
		"painter": [g_Terrains.cliff, plateauTile],
		"size": size,
		"deviation": deviation,
		"fill": fill,
		"count": 15,
		"minSize": 2,
		"maxSize": 4,
		"spread": 200,
		"minElevation": 20,
		"maxElevation": 30,
		"steepness": 8
	});

	for (let i = 0; i < 40; ++i)
	{
		const hillElevation = randIntInclusive(4, 18);
		createAreas(
			new ChainPlacer(3, 15, 1, 0.5),
			[
				new LayeredPainter([plateauTile, plateauTile], [3]),
				new SmoothElevationPainter(ELEVATION_MODIFY, hillElevation, hillElevation - 2),
				new TileClassPainter(g_TileClasses.hill)
			],
			[
				avoidClasses(g_TileClasses.hill, 7),
				stayClasses(g_TileClasses.plateau, 7)
			],
			1);
	}

	addElements([
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.dirt, 15,
				g_TileClasses.forest, 2,
				g_TileClasses.player, 12,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.plateau, 8],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["tons"]
		},
		{
			"func": addProps,
			"avoid": [
				g_TileClasses.forest, 2,
				g_TileClasses.player, 12,
				g_TileClasses.prop, 40,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.plateau, 8],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["scarce"]
		}
	]);
}

/**
 * Place less usual decoratives like barrels or crates.
 */
export function addProps(constraint, size, deviation, fill)
{
	g_Map.log("Creating rare actors");

	const offset = getRandomDeviation(size, deviation);

	const props = [
		[
			new SimpleObject(g_Props.skeleton, offset, 5 * offset, 0, 3 * offset + 2),
		],
		[
			new SimpleObject(g_Props.barrels, offset, 2 * offset, 2, 3 * offset + 2),
			new SimpleObject(g_Props.cart, 0, offset, 5, 2.5 * offset + 5),
			new SimpleObject(g_Props.crate, offset, 2 * offset, 2, 2 * offset + 2),
			new SimpleObject(g_Props.well, 0, 1, 2, 2 * offset + 2)
		]
	];

	const baseCount = 1;

	const counts = [
		scaleByMapSize(16, 262),
		scaleByMapSize(8, 131),
		baseCount * scaleByMapSize(13, 200),
		baseCount * scaleByMapSize(13, 200),
		baseCount * scaleByMapSize(13, 200)
	];

	// Add small props
	for (let i = 0; i < props.length; ++i)
	{
		const propCount = Math.floor(counts[i] * fill);
		const group = new SimpleGroup(props[i], true);
		createObjectGroupsDeprecated(group, 0, constraint, propCount, 5);
	}

	// Add decorative trees
	const trees = new SimpleObject(g_Decoratives.tree, 5 * offset, 30 * offset, 2, 3 * offset + 10);
	createObjectGroupsDeprecated(new SimpleGroup([trees], true), 0, constraint, counts[0] * 5 * fill, 5);
}

export function addValleys(constraint, size, deviation, fill, baseHeight)
{
	if (baseHeight < 6)
		return;

	g_Map.log("Creating valleys");

	const minElevation = Math.max(-baseHeight, 1 - baseHeight / (size * (deviation + 1)));

	let valleySlope = g_Terrains.tier1Terrain;
	let valleyFloor = g_Terrains.tier4Terrain;

	if (currentBiome() == "generic/sahara")
	{
		valleySlope = g_Terrains.tier3Terrain;
		valleyFloor = g_Terrains.dirt;
	}

	if (currentBiome() == "generic/aegean")
	{
		valleySlope = g_Terrains.tier2Terrain;
		valleyFloor = g_Terrains.dirt;
	}

	if (currentBiome() == "generic/alpine" || currentBiome() == "generic/savanna")
		valleyFloor = g_Terrains.tier2Terrain;

	if (currentBiome() == "generic/india")
		valleySlope = g_Terrains.dirt;

	if (currentBiome() == "generic/autumn")
		valleyFloor = g_Terrains.tier3Terrain;

	addElevation(constraint, {
		"class": g_TileClasses.valley,
		"painter": [valleySlope, valleyFloor],
		"size": size,
		"deviation": deviation,
		"fill": fill,
		"count": 8,
		"minSize": 5,
		"maxSize": 8,
		"spread": 30,
		"minElevation": minElevation,
		"maxElevation": -2,
		"steepness": 4
	});
}

/**
 * Create huntable animals.
 */
export function addAnimals(constraint, size, deviation, fill)
{
	g_Map.log("Creating animals");

	const groupOffset = getRandomDeviation(size, deviation);

	const animals = [
		[new SimpleObject(g_Gaia.mainHuntableAnimal, 5 * groupOffset, 7 * groupOffset, 0, 4 * groupOffset)],
		[new SimpleObject(g_Gaia.secondaryHuntableAnimal, 2 * groupOffset, 3 * groupOffset, 0, 2 * groupOffset)]
	];

	for (const animal of animals)
		createObjectGroupsDeprecated(
			new SimpleGroup(animal, true, g_TileClasses.animals),
			0,
			constraint,
			Math.floor(30 * fill),
			50);
}

export function addBerries(constraint, size, deviation, fill)
{
	g_Map.log("Creating berries");

	const groupOffset = getRandomDeviation(size, deviation);

	createObjectGroupsDeprecated(
		new SimpleGroup([new SimpleObject(g_Gaia.fruitBush, 5 * groupOffset, 5 * groupOffset, 0, 3 * groupOffset)], true, g_TileClasses.berries),
		0,
		constraint,
		Math.floor(50 * fill),
		40);
}

export function addFish(constraint, size, deviation, fill)
{
	g_Map.log("Creating fish");

	const groupOffset = getRandomDeviation(size, deviation);

	const fishes = [
		[new SimpleObject(g_Gaia.fish, groupOffset, 2 * groupOffset, 0, 2 * groupOffset)],
		[new SimpleObject(g_Gaia.fish, 2 * groupOffset, 4 * groupOffset, 10 * groupOffset, 20 * groupOffset)]
	];

	for (const fish of fishes)
		createObjectGroupsDeprecated(
			new SimpleGroup(fish, true, g_TileClasses.fish),
			0,
			constraint,
			Math.floor(40 * fill),
			50);
}

export function addForests(constraint, size, deviation, fill)
{
	if (currentBiome() == "generic/savanna")
		return;

	g_Map.log("Creating forests");

	const treeTypes = [
		[
			g_Terrains.forestFloor2 + TERRAIN_SEPARATOR + g_Gaia.tree1,
			g_Terrains.forestFloor2 + TERRAIN_SEPARATOR + g_Gaia.tree2,
			g_Terrains.forestFloor2
		],
		[
			g_Terrains.forestFloor1 + TERRAIN_SEPARATOR + g_Gaia.tree4,
			g_Terrains.forestFloor1 + TERRAIN_SEPARATOR + g_Gaia.tree5,
			g_Terrains.forestFloor1
		]
	];

	const forestTypes = [
		[
			[g_Terrains.forestFloor2, g_Terrains.mainTerrain, treeTypes[0]],
			[g_Terrains.forestFloor2, treeTypes[0]]
		],
		[
			[g_Terrains.forestFloor2, g_Terrains.mainTerrain, treeTypes[1]],
			[g_Terrains.forestFloor1, treeTypes[1]]],
		[
			[g_Terrains.forestFloor1, g_Terrains.mainTerrain, treeTypes[0]],
			[g_Terrains.forestFloor2, treeTypes[0]]],
		[
			[g_Terrains.forestFloor1, g_Terrains.mainTerrain, treeTypes[1]],
			[g_Terrains.forestFloor1, treeTypes[1]]
		]
	];

	for (const forestType of forestTypes)
	{
		const offset = getRandomDeviation(size, deviation);
		createAreas(
			new ChainPlacer(1, Math.floor(scaleByMapSize(3, 5) * offset), Math.floor(50 * offset), 0.5),
			[
				new LayeredPainter(forestType, [2]),
				new TileClassPainter(g_TileClasses.forest)
			],
			constraint,
			10 * fill);
	}
}

export function addMetal(constraint, size, deviation, fill)
{
	g_Map.log("Creating metal mines");

	const offset = getRandomDeviation(size, deviation);
	createObjectGroupsDeprecated(
		new SimpleGroup([new SimpleObject(g_Gaia.metalLarge, offset, offset, 0, 4 * offset)], true, g_TileClasses.metal),
		0,
		constraint,
		1 + 20 * fill,
		100);
}

export function addSmallMetal(constraint, size, mixes, amounts)
{
	g_Map.log("Creating small metal mines");

	const deviation = getRandomDeviation(size, mixes);
	createObjectGroupsDeprecated(
		new SimpleGroup([new SimpleObject(g_Gaia.metalSmall, 2 * deviation, 5 * deviation, deviation, 3 * deviation)], true, g_TileClasses.metal),
		0,
		constraint,
		1 + 20 * amounts,
		100);
}

/**
 * Create stone mines.
 */
export function addStone(constraint, size, deviation, fill)
{
	g_Map.log("Creating stone mines");

	const offset = getRandomDeviation(size, deviation);

	const mines = [
		[
			new SimpleObject(g_Gaia.stoneSmall, 0, 2 * offset, 0, 4 * offset),
			new SimpleObject(g_Gaia.stoneLarge, offset, offset, 0, 4 * offset)
		],
		[
			new SimpleObject(g_Gaia.stoneSmall, 2 * offset, 5 * offset, offset, 3 * offset)
		]
	];

	for (const mine of mines)
		createObjectGroupsDeprecated(
			new SimpleGroup(mine, true, g_TileClasses.rock),
			0,
			constraint,
			1 + 20 * fill,
			100);
}

/**
 * Create straggler trees.
 */
export function addStragglerTrees(constraint, size, deviation, fill)
{
	g_Map.log("Creating straggler trees");

	// Ensure minimum distribution on african biome
	if (currentBiome() == "generic/savanna")
	{
		fill = Math.max(fill, 2);
		size = Math.max(size, 1);
	}

	const trees = [g_Gaia.tree1, g_Gaia.tree2, g_Gaia.tree3, g_Gaia.tree4];

	const treesPerPlayer = 40;
	const playerBonus = Math.max(1, (getNumPlayers() - 3) / 2);

	const offset = getRandomDeviation(size, deviation);
	const treeCount = treesPerPlayer * playerBonus * fill;
	const totalTrees = scaleByMapSize(treeCount, treeCount);

	const count = Math.floor(totalTrees / trees.length) * fill;
	let min = offset;
	let max = 4 * offset;
	let minDist = offset;
	let maxDist = 5 * offset;

	// More trees for the african biome
	if (currentBiome() == "generic/savanna")
	{
		min = 3 * offset;
		max = 5 * offset;
		minDist = 2 * offset + 1;
		maxDist = 3 * offset + 2;
	}

	for (let i = 0; i < trees.length; ++i)
	{
		let treesMax = max;

		// Don't clump fruit trees
		if (i == 2 && (currentBiome() == "generic/sahara" || currentBiome() == "generic/aegean"))
			treesMax = 1;

		min = Math.min(min, treesMax);

		const group = new SimpleGroup([new SimpleObject(trees[i], min, treesMax, minDist, maxDist)], true, g_TileClasses.forest);
		createObjectGroupsDeprecated(group, 0, constraint, count);
	}
}

/**
 * Determine if the endline of the bluff is within the tilemap.
 */
function isBluffPassable(bluffArea, baseLine, endLine)
{
	if (!baseLine ||
	    !endLine ||
	    !g_Map.validTilePassable(endLine.start) &&
	    !g_Map.validTilePassable(endLine.end))
		return false;

	const minTilesInGroup = 2;
	let insideBluff = false;
	let outsideBluff = false;

	// If there aren't enough points in each row
	const corners = getBoundingBox(bluffArea.getPoints());
	for (let x = corners.min.x; x <= corners.max.x; ++x)
	{
		let count = 0;
		for (let y = corners.min.y; y <= corners.max.y; ++y)
		{
			const pos = new Vector2D(x, y);
			if (!bluffArea.contains(pos))
				continue;

			const valid = g_Map.validTilePassable(pos);
			if (valid)
				++count;

			if (valid)
				insideBluff = true;

			if (outsideBluff && valid)
				return false;
		}

		// We're expecting the end of the bluff
		if (insideBluff && count < minTilesInGroup)
			outsideBluff = true;
	}

	insideBluff = false;
	outsideBluff = false;

	// If there aren't enough points in each column
	for (let y = corners.min.y; y <= corners.max.y; ++y)
	{
		let count = 0;
		for (let x = corners.min.x; x <= corners.max.x; ++x)
		{
			const pos = new Vector2D(x, y);
			if (!bluffArea.contains(pos))
				continue;

			const valid = g_Map.validTilePassable(pos.add(corners.min));
			if (valid)
				++count;

			if (valid)
				insideBluff = true;

			if (outsideBluff && valid)
				return false;
		}

		// We're expecting the end of the bluff
		if (insideBluff && count < minTilesInGroup)
			outsideBluff = true;
	}

	return true;
}

/**
 * Find a 45 degree line that does not intersect with the bluff.
 */
function findClearLine(bluffArea, angle)
{
	const corners = getBoundingBox(bluffArea.getPoints());

	// Angle - 0: northwest; 1: northeast; 2: southeast; 3: southwest
	let offset;
	let y;
	switch (angle)
	{
	case 0:
		offset = new Vector2D(-1, -1);
		y = corners.max.y;
		break;
	case 1:
		offset = new Vector2D(1, -1);
		y = corners.max.y;
		break;
	case 2:
		offset = new Vector2D(1, 1);
		y = corners.min.y;
		break;
	case 3:
		offset = new Vector2D(-1, 1);
		y = corners.min.y;
		break;
	default:
		throw new Error("Unknown angle " + angle);
	}

	let clearLine;
	for (let x = corners.min.x; x <= corners.max.x; ++x)
	{
		const start = new Vector2D(x, y);

		let intersectsBluff = false;
		const end = start.clone();

		while (end.x >= corners.min.x && end.x <= corners.max.x && end.y >= corners.min.y && end.y <= corners.max.y)
		{
			if (bluffArea.contains(end) && g_Map.validTilePassable(end))
			{
				intersectsBluff = true;
				break;
			}
			end.add(offset);
		}

		if (!intersectsBluff)
			clearLine = {
				"start": start,
				"end": end.sub(offset)
			};

		if (intersectsBluff ? (angle == 0 || angle == 3) : (angle == 1 || angle == 2))
			break;
	}

	return clearLine;
}

/**
 * Returns a number within a random deviation of a base number.
 */
function getRandomDeviation(base, deviation)
{
	return base + randFloat(-1, 1) * Math.min(base, deviation);
}
