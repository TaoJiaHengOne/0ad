Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap(mapSettings)
{
	const tPrimary = ["medit_rocks_grass", "medit_rocks_grass", "medit_rocks_grass",
		"medit_rocks_grass", "medit_rocks_grass_shrubs", "medit_rocks_shrubs"];
	const tGrass = ["medit_rocks_grass_shrubs", "medit_rocks_shrubs"];
	const tForestFloor = "medit_grass_field_dry";
	const tCliff = "medit_cliff_italia";
	const tGrassDirt = "medit_rocks_grass";
	const tDirt = "medit_dirt";
	const tRoad = "medit_city_tile";
	const tRoadWild = "medit_city_tile";
	const tGrass2 = "medit_rocks_grass_shrubs";
	const tGrassPatch = "medit_grass_wild";

	const oCarob = "gaia/tree/carob";
	const oAleppoPine = "gaia/tree/aleppo_pine";
	const oBerryBush = "gaia/fruit/berry_01";
	const oDeer = "gaia/fauna_deer";
	const oSheep = "gaia/fauna_sheep";
	const oStoneLarge = "gaia/rock/mediterranean_large";
	const oStoneSmall = "gaia/rock/mediterranean_small";
	const oMetalLarge = "gaia/ore/mediterranean_large";
	const oWoodTreasure = "gaia/treasure/wood";
	const oFoodTreasure = "gaia/treasure/food_bin";

	const aGrass = "actor|props/flora/grass_soft_large_tall.xml";
	const aGrassShort = "actor|props/flora/grass_soft_large.xml";
	const aRockLarge = "actor|geology/stone_granite_large.xml";
	const aRockMedium = "actor|geology/stone_granite_med.xml";
	const aBushMedium = "actor|props/flora/bush_medit_me.xml";
	const aBushSmall = "actor|props/flora/bush_medit_sm.xml";
	const aCarob = "actor|flora/trees/carob.xml";
	const aAleppoPine = "actor|flora/trees/aleppo_pine.xml";

	const pForest1 = [
		tForestFloor + TERRAIN_SEPARATOR + oCarob,
		tForestFloor
	];
	const pForest2 = [
		tForestFloor + TERRAIN_SEPARATOR + oAleppoPine,
		tForestFloor
	];

	const heightLand = 3;

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
	const clTreasure = g_Map.createTileClass();

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
			"template": oCarob,
			"count": scaleByMapSize(2, 8)
		},
		"Decoratives": {
			"template": aGrassShort
		}
	});
	yield 10;

	createBumps(avoidClasses(clPlayer, 9));

	createMountains(tCliff, avoidClasses(clPlayer, 20, clHill, 8), clHill, scaleByMapSize(20, 120));

	yield 25;

	const [forestTrees, stragglerTrees] = getTreeCounts(500, 3000, 0.7);
	createForests(
		[tGrass, tForestFloor, tForestFloor, pForest1, pForest2],
		avoidClasses(clPlayer, 20, clForest, 14, clHill, 1),
		clForest,
		forestTrees);

	yield 40;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[tGrassDirt, tDirt],
		[2],
		avoidClasses(clForest, 0, clHill, 0, clDirt, 3, clPlayer, 10),
		scaleByMapSize(15, 45),
		clDirt);

	g_Map.log("Creating grass patches");
	createLayeredPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		[tGrass2, tGrassPatch],
		[1],
		avoidClasses(clForest, 0, clHill, 0, clDirt, 3, clPlayer, 10),
		scaleByMapSize(15, 45),
		clDirt);

	yield 50;

	g_Map.log("Creating stone mines");
	createMines(
		[
			[
				new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
				new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
			],
			[new SimpleObject(oStoneSmall, 2, 5, 1, 3)]
		],
		avoidClasses(clForest, 1, clPlayer, 20, clMetal, 10, clRock, 5, clHill, 2),
		clRock);

	g_Map.log("Creating metal mines");
	createMines(
		[
			[new SimpleObject(oMetalLarge, 1, 1, 0, 4)]
		],
		avoidClasses(clForest, 1, clPlayer, 20, clMetal, 10, clRock, 5, clHill, 2),
		clMetal
	);

	yield 60;

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
		avoidClasses(clForest, 0, clPlayer, 0, clHill, 0));

	yield 75;

	createFood(
		[
			[new SimpleObject(oSheep, 5, 7, 0, 4)],
			[new SimpleObject(oDeer, 2, 3, 0, 2)]
		],
		[
			3 * numPlayers,
			3 * numPlayers
		],
		avoidClasses(clForest, 0, clPlayer, 20, clHill, 1, clFood, 20),
		clFood);

	createFood(
		[
			[new SimpleObject(oBerryBush, 5, 7, 0, 4)]
		],
		[
			randIntInclusive(3, 12) * numPlayers + 2
		],
		avoidClasses(clForest, 0, clPlayer, 20, clHill, 1, clFood, 10),
		clFood);

	g_Map.log("Creating food treasures");
	let group = new SimpleGroup(
		[new SimpleObject(oFoodTreasure, 2, 3, 0, 2)],
		true, clTreasure
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 18, clHill, 1, clFood, 5),
		3 * numPlayers, 50
	);

	g_Map.log("Creating food treasures");
	group = new SimpleGroup(
		[new SimpleObject(oWoodTreasure, 2, 3, 0, 2)],
		true, clTreasure
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 18, clHill, 1),
		3 * numPlayers, 50
	);

	yield 80;

	createStragglerTrees(
		[oCarob, oAleppoPine],
		avoidClasses(clForest, 1, clHill, 1, clPlayer, 10, clMetal, 6, clRock, 6, clTreasure, 4),
		clForest,
		stragglerTrees);

	createStragglerTrees(
		[aCarob, aAleppoPine],
		stayClasses(clHill, 2),
		clForest,
		stragglerTrees / 5);

	placePlayersNomad(clPlayer, avoidClasses(clForest, 1, clMetal, 4, clRock, 4, clHill, 4, clFood, 2));

	setFogFactor(0.2);
	setFogThickness(0.14);

	setPPEffect("hdr");
	setPPContrast(0.45);
	setPPSaturation(0.56);
	setPPBloom(0.1);

	return g_Map;
}
