Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap(mapSettings)
{
	const tPrimary = ["steppe_grass_a", "steppe_grass_c", "steppe_grass_d"];
	const tGrass = ["steppe_grass_a", "steppe_grass_b", "steppe_grass_c", "steppe_grass_d"];
	const tForestFloor = "steppe_grass_c";
	const tGrassA = "steppe_grass_b";
	const tGrassB = "steppe_grass_c";
	const tGrassC = ["steppe_grass_b", "steppe_grass_c", "steppe_grass_d"];
	const tGrassD = "steppe_grass_a";
	const tDirt = ["steppe_dirt_a", "steppe_dirt_b"];
	const tRoad = "road_stones";
	const tRoadWild = "road_stones";

	const oPoplar = "gaia/tree/poplar_lombardy";
	const oBush = "gaia/tree/bush_temperate";
	const oBerryBush = "gaia/fruit/berry_01";
	const oRabbit = "gaia/fauna_rabbit";
	const oAnimal = "gaia/fauna_sheep";
	const oSheep = "gaia/fauna_sheep";
	const oStoneLarge = "gaia/rock/mediterranean_large";
	const oStoneSmall = "gaia/rock/mediterranean_small";
	const oMetalLarge = "gaia/ore/mediterranean_large";

	const aGrass = "actor|props/flora/grass_soft_small_tall.xml";
	const aGrassShort = "actor|props/flora/grass_soft_large.xml";
	const aRockLarge = "actor|geology/stone_granite_med.xml";
	const aRockMedium = "actor|geology/stone_granite_med.xml";
	const aBushMedium = "actor|props/flora/bush_medit_me.xml";
	const aBushSmall = "actor|props/flora/bush_medit_sm.xml";

	const pForest = [
		tForestFloor + TERRAIN_SEPARATOR + oPoplar,
		tForestFloor
	];

	const heightLand = 1;
	const heightOffsetBump = 2;

	globalThis.g_Map = new RandomMap(heightLand, tPrimary);

	const numPlayers = getNumPlayers();

	const clPlayer = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();

	const [playerIDs, playerPosition] =
		playerPlacementByPattern(
			mapSettings.PlayerPlacement,
			fractionToTiles(0.35),
			fractionToTiles(0.1),
			randomAngle(),
			undefined);

	placePlayerBases({
		"PlayerPlacement": [playerIDs, playerPosition],
		"PlayerTileClass": clPlayer,
		"BaseResourceClass": clBaseResource,
		"CityPatch": {
			"outerTerrain": tRoadWild,
			"innerTerrain": tRoad
		},
		"StartingAnimal": {
			"template": oAnimal
		},
		"Berries": {
			"template": oBerryBush
		},
		"Mines": {
			"types": [
				{ "template": oMetalLarge },
				{ "template": oStoneLarge }
			]
		},
		"Trees": {
			"template": oPoplar
		},
		"Decoratives": {
			"template": aGrassShort
		}
	});

	yield 20;

	g_Map.log("Creating bumps");
	createAreas(
		new ChainPlacer(1, Math.floor(scaleByMapSize(4, 6)), Math.floor(scaleByMapSize(2, 5)), 0.5),
		new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetBump, 2),
		avoidClasses(clPlayer, 13),
		scaleByMapSize(300, 800));

	g_Map.log("Creating forests");
	const [forestTrees, stragglerTrees] = getTreeCounts(220, 1000, 0.65);
	const types = [[[tForestFloor, tGrass, pForest], [tForestFloor, pForest]]];
	const size = forestTrees / (scaleByMapSize(2, 8) * numPlayers);
	const num = 4 * Math.floor(size / types.length);
	for (const type of types)
		createAreas(
			new ChainPlacer(1, Math.floor(scaleByMapSize(2, 3)), 4, Infinity),
			[
				new LayeredPainter(type, [2]),
				new TileClassPainter(clForest)
			],
			avoidClasses(clPlayer, 13, clForest, 20, clHill, 1),
			num);
	yield 50;

	g_Map.log("Creating grass patches");
	createLayeredPatches(
		[scaleByMapSize(5, 48), scaleByMapSize(6, 84), scaleByMapSize(8, 128)],
		[[tGrass, tGrassA, tGrassC], [tGrass, tGrassA, tGrassC], [tGrass, tGrassA, tGrassC]],
		[1, 1],
		avoidClasses(clForest, 0, clHill, 0, clDirt, 2, clPlayer, 10),
		scaleByMapSize(50, 70),
		clDirt);

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(5, 32), scaleByMapSize(6, 48), scaleByMapSize(7, 80)],
		[tGrassD, tDirt],
		[1],
		avoidClasses(clForest, 0, clHill, 0, clDirt, 2, clPlayer, 10),
		scaleByMapSize(50, 90),
		clDirt);

	yield 55;

	g_Map.log("Creating big patches");
	createLayeredPatches(
		[scaleByMapSize(10, 60), scaleByMapSize(15, 90), scaleByMapSize(20, 120)],
		[tGrassB, tGrassA],
		[1],
		avoidClasses(clHill, 0, clPlayer, 8),
		scaleByMapSize(30, 90),
		clDirt);

	yield 60;

	g_Map.log("Creating stone mines");
	let group = new SimpleGroup(
		[
			new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
			new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
		],
		true,
		clRock);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 1, clPlayer, 20, clRock, 10, clHill, 1),
		scaleByMapSize(1, 4), 100
	);

	g_Map.log("Creating small stone quarries");
	group = new SimpleGroup([new SimpleObject(oStoneSmall, 2, 5, 1, 3)], true, clRock);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 1, clPlayer, 20, clRock, 10, clHill, 1),
		scaleByMapSize(1, 4), 100
	);

	g_Map.log("Creating metal mines");
	group = new SimpleGroup([new SimpleObject(oMetalLarge, 1, 1, 0, 4)], true, clMetal);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 1, clPlayer, 20, clMetal, 10, clRock, 5, clHill, 1),
		scaleByMapSize(2, 8), 100
	);

	yield 65;

	g_Map.log("Creating small decorative rocks");
	group = new SimpleGroup(
		[new SimpleObject(aRockMedium, 1, 3, 0, 1)],
		true
	);
	createObjectGroupsDeprecated(
		group, 0,
		avoidClasses(clForest, 0, clPlayer, 10, clHill, 0),
		scaleByMapSize(16, 262), 50
	);

	g_Map.log("Creating large decorative rocks");
	group = new SimpleGroup(
		[new SimpleObject(aRockLarge, 1, 2, 0, 1), new SimpleObject(aRockMedium, 1, 3, 0, 2)],
		true
	);
	createObjectGroupsDeprecated(
		group, 0,
		avoidClasses(clForest, 0, clPlayer, 10, clHill, 0),
		scaleByMapSize(8, 131), 50
	);

	yield 70;

	g_Map.log("Creating rabbits");
	group = new SimpleGroup(
		[new SimpleObject(oRabbit, 5, 7, 0, 4)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 10, clHill, 1, clFood, 20),
		6 * numPlayers, 50
	);

	g_Map.log("Creating berry bush");
	group = new SimpleGroup(
		[new SimpleObject(oBerryBush, 5, 7, 0, 4)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 20, clHill, 1, clFood, 20),
		randIntInclusive(1, 4) * numPlayers + 2, 50
	);

	yield 75;

	g_Map.log("Creating sheep");
	group = new SimpleGroup(
		[new SimpleObject(oSheep, 2, 3, 0, 2)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 10, clHill, 1, clFood, 20),
		3 * numPlayers, 50
	);

	yield 85;

	createStragglerTrees(
		[oBush, oPoplar],
		avoidClasses(clForest, 1, clHill, 1, clPlayer, 13, clMetal, 6, clRock, 6),
		clForest,
		stragglerTrees);

	g_Map.log("Creating large grass tufts");
	group = new SimpleGroup(
		[
			new SimpleObject(aGrass, 2, 4, 0, 1.8, -Math.PI / 8, Math.PI / 8),
			new SimpleObject(aGrassShort, 3, 6, 1.2, 2.5, -Math.PI / 8, Math.PI / 8)
		]
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clHill, 2, clPlayer, 10, clDirt, 1, clForest, 0),
		scaleByMapSize(13, 200)
	);

	yield 95;

	g_Map.log("Creating bushes");
	group = new SimpleGroup(
		[new SimpleObject(aBushMedium, 1, 2, 0, 2), new SimpleObject(aBushSmall, 2, 4, 0, 2)]
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clHill, 1, clPlayer, 1, clDirt, 1),
		scaleByMapSize(13, 200), 50
	);

	placePlayersNomad(clPlayer, avoidClasses(clForest, 1, clMetal, 4, clRock, 4, clHill, 4, clFood, 2));

	setFogThickness(0.1);
	setFogFactor(0.2);

	setPPEffect("hdr");
	setPPSaturation(0.45);
	setPPContrast(0.62);
	setPPBloom(0.2);

	return g_Map;
}
