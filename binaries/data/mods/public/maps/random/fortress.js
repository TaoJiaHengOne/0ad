Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap()
{
	const tGrass = ["temperate_grass_04", "temperate_grass_03", "temperate_grass_04"];
	const tForestFloor = "temperate_forestfloor_01";
	const tGrassA = "temperate_grass_05";
	const tGrassB = "temperate_grass_02";
	const tGrassC = "temperate_grass_mud_01";
	const tHill = "temperate_rocks_dirt_01";
	const tCliff = ["temperate_cliff_01", "temperate_cliff_02"];
	const tRoad = "temperate_paving_03";
	const tGrassPatch = "temperate_grass_dirt_01";
	const tShore = "temperate_grass_mud_01";
	const tWater = "temperate_mud_01";

	const oBeech = "gaia/tree/euro_beech_aut";
	const oOak = "gaia/tree/oak_aut";
	const oPine = "gaia/tree/pine";
	const oDeer = "gaia/fauna_deer";
	const oFish = "gaia/fish/generic";
	const oSheep = "gaia/fauna_rabbit";
	const oBerryBush = "gaia/fruit/berry_01";
	const oStoneLarge = "gaia/rock/temperate_large";
	const oStoneSmall = "gaia/rock/temperate_small";
	const oMetalLarge = "gaia/ore/temperate_01";
	const oMetalSmall = "gaia/ore/temperate_small";
	const oFoodTreasure = "gaia/treasure/food_bin";
	const oWoodTreasure = "gaia/treasure/wood";
	const oStoneTreasure = "gaia/treasure/stone";
	const oMetalTreasure = "gaia/treasure/metal";

	const aGrass = "actor|props/flora/grass_soft_dry_small_tall.xml";
	const aGrassShort = "actor|props/flora/grass_soft_dry_large.xml";
	const aRockLarge = "actor|geology/stone_granite_med.xml";
	const aRockMedium = "actor|geology/stone_granite_med.xml";
	const aReeds = "actor|props/flora/reeds_pond_dry.xml";
	const aLillies = "actor|props/flora/water_lillies.xml";
	const aBushMedium = "actor|props/flora/bush_medit_me_dry.xml";
	const aBushSmall = "actor|props/flora/bush_medit_sm_dry.xml";

	const pForestD = [tForestFloor + TERRAIN_SEPARATOR + oBeech, tForestFloor];
	const pForestO = [tForestFloor + TERRAIN_SEPARATOR + oOak, tForestFloor];
	const pForestP = [tForestFloor + TERRAIN_SEPARATOR + oPine, tForestFloor];

	const heightSeaGround = -4;
	const heightLand = 3;

	globalThis.g_Map = new RandomMap(heightLand, tGrass);

	const numPlayers = getNumPlayers();

	const clPlayer = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clWater = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();

	const treasures = [
		{ "template": oFoodTreasure, "count": 5 },
		{ "template": oWoodTreasure, "count": 5 },
		{ "template": oMetalTreasure, "count": 4 },
		{ "template": oStoneTreasure, "count": 2 }
	];

	const [playerIDs, playerPosition] = playerPlacementCircle(fractionToTiles(0.35));

	g_Map.log("Creating playerbases");
	const playerAngle = BUILDING_ORIENTATION;
	for (let i = 0; i < numPlayers; ++i)
	{
		if (isNomad())
			break;

		// CC and units
		for (const dist of [6, 8])
		{
			let ents = getStartingEntities(playerIDs[i]);

			if (dist == 8)
				ents = ents.filter(ent => ent.Template.indexOf("female") != -1 ||
					ent.Template.indexOf("infantry") != -1);

			placeStartingEntities(playerPosition[i], playerIDs[i], ents, dist);
		}

		// Treasure
		for (let j = 0; j < treasures.length; ++j)
			createObjectGroup(
				new SimpleGroup(
					[new SimpleObject(
						treasures[j].template,
						treasures[j].count,
						treasures[j].count,
						0,
						2)],
					false,
					clBaseResource,
					Vector2D.add(playerPosition[i],
						new Vector2D(10, 0).rotate(-j * Math.PI / 2 - playerAngle))),
				0);

		// Ground texture
		const civ = getCivCode(playerIDs[i]);
		const tilesSize = civ == "cart" ? 23 : 21;
		createArea(
			new ConvexPolygonPlacer(
				new Array(4).fill(0).map((zero, j) => new Vector2D(tilesSize, 0)
					.rotate(j * Math.PI / 2 - playerAngle - Math.PI/4).add(playerPosition[i])),
				Infinity),
			[
				new TerrainPainter(tRoad),
				new TileClassPainter(clPlayer)
			]);

		// Fortress
		const wall = ["gate", "tower", "long",
			"cornerIn", "long", "barracks", "tower", "long", "tower", "long",
			"cornerIn", "long", "stable", "tower", "gate", "tower", "long",
			"cornerIn", "long", "temple", "tower", "long", "tower", "long",
			"cornerIn", "long", "market", "tower"];

		placeCustomFortress(playerPosition[i], new Fortress("Spahbod", wall), civ, playerIDs[i],
			playerAngle);
	}

	g_Map.log("Creating lakes");
	const numLakes = Math.round(scaleByMapSize(1, 4) * numPlayers);
	const waterAreas = createAreas(
		new ClumpPlacer(scaleByMapSize(100, 250), 0.8, 0.1, Infinity),
		[
			new LayeredPainter([tShore, tWater], [1]),
			new SmoothElevationPainter(ELEVATION_SET, heightSeaGround, 3),
			new TileClassPainter(clWater)
		],
		avoidClasses(clPlayer, 7, clWater, 20),
		numLakes);

	yield 15;

	g_Map.log("Creating reeds");
	createObjectGroupsByAreasDeprecated(
		new SimpleGroup([new SimpleObject(aReeds, 5, 10, 0, 4), new SimpleObject(aLillies, 0, 1, 0, 4)],
			true),
		0,
		[borderClasses(clWater, 3, 0), stayClasses(clWater, 1)],
		numLakes, 100,
		waterAreas);

	yield 25;

	g_Map.log("Creating fish");
	createObjectGroupsByAreasDeprecated(
		new SimpleGroup(
			[new SimpleObject(oFish, 1, 1, 0, 1)],
			true, clFood
		),
		0,
		[stayClasses(clWater, 4), avoidClasses(clFood, 8)],
		numLakes / 4,
		50,
		waterAreas);
	yield 30;

	createBumps(avoidClasses(clWater, 2, clPlayer, 5));
	yield 35;

	createHills([tCliff, tCliff, tHill], avoidClasses(clPlayer, 5, clWater, 5, clHill, 15), clHill,
		scaleByMapSize(1, 4) * numPlayers);
	yield 40;

	g_Map.log("Creating forests");
	const [forestTrees, stragglerTrees] = getTreeCounts(500, 2500, 0.7);
	const types = [
		[[tForestFloor, tGrass, pForestD], [tForestFloor, pForestD]],
		[[tForestFloor, tGrass, pForestO], [tForestFloor, pForestO]],
		[[tForestFloor, tGrass, pForestP], [tForestFloor, pForestP]]
	];
	const size = forestTrees / (scaleByMapSize(3, 6) * numPlayers);
	const num = Math.floor(size / types.length);
	for (const type of types)
		createAreas(
			new ChainPlacer(1, Math.floor(scaleByMapSize(3, 5)), forestTrees / num, 0.5),
			[
				new LayeredPainter(type, [2]),
				new TileClassPainter(clForest)
			],
			avoidClasses(clPlayer, 5, clWater, 3, clForest, 15, clHill, 1),
			num);
	yield 50;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[[tGrass, tGrassA], [tGrassA, tGrassB], [tGrassB, tGrassC]],
		[1, 1],
		avoidClasses(clWater, 1, clForest, 0, clHill, 0, clDirt, 5, clPlayer, 1),
		scaleByMapSize(15, 45),
		clDirt);
	yield 55;

	g_Map.log("Creating grass patches");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tGrassPatch,
		avoidClasses(clWater, 1, clForest, 0, clHill, 0, clDirt, 5, clPlayer, 1),
		scaleByMapSize(15, 45),
		clDirt);
	yield 60;

	g_Map.log("Creating stone mines");
	createMines(
		[
			[
				new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
				new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
			],
			[new SimpleObject(oStoneSmall, 2, 5, 1, 3)]
		],
		avoidClasses(clWater, 0, clForest, 1, clPlayer, 5, clRock, 10, clHill, 1),
		clRock);
	yield 65;

	g_Map.log("Creating metal mines");
	createBalancedMetalMines(
		oMetalSmall,
		oMetalLarge,
		clMetal,
		avoidClasses(clWater, 0, clForest, 1, clPlayer, 5, clMetal, 10, clRock, 5, clHill, 1)
	);
	yield 70;

	createDecoration(
		[
			[new SimpleObject(aRockMedium, 1, 3, 0, 1)],
			[new SimpleObject(aRockLarge, 1, 2, 0, 1), new SimpleObject(aRockMedium, 1, 3, 0, 2)],
			[new SimpleObject(aGrassShort, 1, 2, 0, 1)],
			[new SimpleObject(aGrass, 2, 4, 0, 1.8), new SimpleObject(aGrassShort, 3, 6, 1.2, 2.5)],
			[new SimpleObject(aBushMedium, 1, 2, 0, 2), new SimpleObject(aBushSmall, 2, 4, 0, 2)]
		],
		[
			scaleByMapAreaAbsolute(16),
			scaleByMapAreaAbsolute(8),
			scaleByMapAreaAbsolute(13),
			scaleByMapAreaAbsolute(13),
			scaleByMapAreaAbsolute(13)
		],
		avoidClasses(clWater, 0, clForest, 0, clPlayer, 1, clHill, 0));
	yield 80;

	createFood(
		[
			[new SimpleObject(oSheep, 2, 3, 0, 2)],
			[new SimpleObject(oDeer, 5, 7, 0, 4)]
		],
		[
			3 * numPlayers,
			3 * numPlayers
		],
		avoidClasses(clWater, 0, clForest, 0, clPlayer, 6, clHill, 1, clFood, 20),
		clFood);
	yield 85;

	createFood(
		[
			[new SimpleObject(oBerryBush, 5, 7, 0, 4)]
		],
		[
			randIntInclusive(1, 4) * numPlayers + 2
		],
		avoidClasses(clWater, 2, clForest, 0, clPlayer, 6, clHill, 1, clFood, 10),
		clFood);

	yield 90;

	createStragglerTrees(
		[oOak, oBeech, oPine],
		avoidClasses(clWater, 1, clForest, 1, clHill, 1, clPlayer, 1, clMetal, 6, clRock, 6),
		clForest,
		stragglerTrees);
	yield 95;

	placePlayersNomad(clPlayer,
		avoidClasses(clWater, 2, clHill, 2, clForest, 1, clMetal, 4, clRock, 4, clHill, 4, clFood, 2));

	setSkySet("sunny");
	setWaterColor(0.157, 0.149, 0.443);
	setWaterTint(0.443, 0.42, 0.824);
	setWaterWaviness(2.0);
	setWaterType("lake");
	setWaterMurkiness(0.83);

	setFogFactor(0.35);
	setFogThickness(0.22);
	setFogColor(0.82, 0.82, 0.73);
	setPPSaturation(0.56);
	setPPContrast(0.56);
	setPPBloom(0.38);
	setPPEffect("hdr");

	return g_Map;
}
