Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap(mapSettings)
{
	setFogThickness(0.46);
	setFogFactor(0.5);

	setPPEffect("hdr");
	setPPSaturation(0.48);
	setPPContrast(0.53);
	setPPBloom(0.12);

	const tPrimary = ["alpine_grass_rocky"];
	const tForestFloor = "alpine_grass";
	const tCliff = ["polar_cliff_a", "polar_cliff_b", "polar_cliff_snow"];
	const tSecondary = "alpine_grass";
	const tHalfSnow = ["polar_grass_snow", "ice_dirt"];
	const tSnowLimited = ["polar_snow_rocks", "polar_ice"];
	const tDirt = "ice_dirt";
	const tShore = "alpine_shore_rocks";
	const tWater = "polar_ice_b";
	const tHill = "polar_ice_cracked";

	const oBush = "gaia/tree/bush_badlands";
	const oBush2 = "gaia/tree/bush_temperate";
	const oBerryBush = "gaia/fruit/berry_01";
	const oRabbit = "gaia/fauna_rabbit";
	const oMuskox = "gaia/fauna_muskox";
	const oDeer = "gaia/fauna_deer";
	const oWolf = "gaia/fauna_wolf";
	const oWhaleFin = "gaia/fauna_whale_fin";
	const oWhaleHumpback = "gaia/fauna_whale_humpback";
	const oFish = "gaia/fish/generic";
	const oStoneLarge = "gaia/rock/alpine_large";
	const oStoneSmall = "gaia/rock/alpine_small";
	const oMetalLarge = "gaia/ore/alpine_large";
	const oWoodTreasure = "gaia/treasure/wood";

	const aRockLarge = "actor|geology/stone_granite_med.xml";
	const aRockMedium = "actor|geology/stone_granite_med.xml";

	const pForest = [
		tForestFloor + TERRAIN_SEPARATOR + oBush,
		tForestFloor + TERRAIN_SEPARATOR + oBush2,
		tForestFloor
	];

	const heightSeaGround = -5;
	const heightLand = 2;

	globalThis.g_Map = new RandomMap(heightLand, tPrimary);

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
			"outerTerrain": tPrimary,
			"innerTerrain": tSecondary
		},
		"StartingAnimal": {
			"template": oRabbit
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
		"Treasures": {
			"types": [
				{
					"template": oWoodTreasure,
					"count": 10
				}
			]
		},
		"Trees": {
			"template": oBush,
			"count": 20,
			"maxDistGroup": 3
		}
		// No decoratives
	});
	yield 20;

	createHills(
		[tPrimary, tCliff, tHill],
		avoidClasses(
			clPlayer, 35,
			clForest, 20,
			clHill, 50,
			clWater, 2),
		clHill,
		scaleByMapSize(1, 240));

	yield 30;

	g_Map.log("Creating lakes");
	createAreas(
		new ChainPlacer(
			1,
			Math.floor(scaleByMapSize(4, 8)),
			Math.floor(scaleByMapSize(40, 180)),
			Infinity),
		[
			new LayeredPainter([tShore, tWater], [1]),
			new SmoothElevationPainter(ELEVATION_SET, heightSeaGround, 5),
			new TileClassPainter(clWater)
		],
		avoidClasses(clPlayer, 15),
		scaleByMapSize(1, 20));

	yield 45;

	createBumps(avoidClasses(clPlayer, 6, clWater, 2), scaleByMapSize(30, 300), 1, 8, 4, 0, 3);

	paintTerrainBasedOnHeight(4, 15, 0, tCliff);
	paintTerrainBasedOnHeight(15, 100, 3, tSnowLimited);

	const [forestTrees, stragglerTrees] = getTreeCounts(500, 3000, 0.7);
	createForests(
		[tSecondary, tForestFloor, tForestFloor, pForest, pForest],
		avoidClasses(
			clPlayer, 20,
			clForest, 14,
			clHill, 20,
			clWater, 2),
		clForest,
		forestTrees);
	yield 60;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[[tDirt, tHalfSnow], [tHalfSnow, tSnowLimited]],
		[2],
		avoidClasses(
			clWater, 3,
			clForest, 0,
			clHill, 0,
			clDirt, 5,
			clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);

	g_Map.log("Creating shrubs");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tSecondary,
		avoidClasses(
			clWater, 3,
			clForest, 0,
			clHill, 0,
			clDirt, 5,
			clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);

	g_Map.log("Creating grass patches");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tSecondary,
		avoidClasses(
			clWater, 3,
			clForest, 0,
			clHill, 0,
			clDirt, 5,
			clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);
	yield 65;

	g_Map.log("Creating stone mines");
	createMines(
		[
			[
				new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
				new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
			],
			[new SimpleObject(oStoneSmall, 2, 5, 1, 3)]
		],
		avoidClasses(
			clWater, 3,
			clForest, 1,
			clPlayer, 20,
			clRock, 18,
			clHill, 1),
		clRock);

	g_Map.log("Creating metal mines");
	createMines(
		[
			[new SimpleObject(oMetalLarge, 1, 1, 0, 4)]
		],
		avoidClasses(
			clWater, 3,
			clForest, 1,
			clPlayer, 20,
			clMetal, 18,
			clRock, 5,
			clHill, 1),
		clMetal);
	yield 70;

	createDecoration(
		[
			[
				new SimpleObject(aRockMedium, 1, 3, 0, 1)
			],
			[
				new SimpleObject(aRockLarge, 1, 2, 0, 1),
				new SimpleObject(aRockMedium, 1, 3, 0, 1)
			]
		],
		[
			scaleByMapAreaAbsolute(16),
			scaleByMapAreaAbsolute(8),
		],
		avoidClasses(
			clWater, 0,
			clForest, 0,
			clPlayer, 0,
			clHill, 0));
	yield 75;

	createFood(
		[
			[new SimpleObject(oWolf, 3, 5, 0, 3)],
			[new SimpleObject(oRabbit, 6, 8, 0, 6)],
			[new SimpleObject(oDeer, 3, 4, 0, 3)],
			[new SimpleObject(oMuskox, 3, 4, 0, 3)]
		],
		[
			3 * numPlayers,
			3 * numPlayers,
			3 * numPlayers,
			3 * numPlayers
		],
		avoidClasses(
			clFood, 20,
			clHill, 5,
			clWater, 5),
		clFood);

	createFood(
		[
			[new SimpleObject(oWhaleFin, 1, 1, 0, 3)],
			[new SimpleObject(oWhaleHumpback, 1, 1, 0, 3)]
		],
		[
			3 * numPlayers,
			3 * numPlayers
		],
		[
			avoidClasses(clFood, 20),
			stayClasses(clWater, 6)
		],
		clFood);

	createFood(
		[
			[new SimpleObject(oBerryBush, 5, 7, 0, 4)]
		],
		[
			randIntInclusive(1, 4) * numPlayers + 2
		],
		avoidClasses(
			clWater, 3,
			clForest, 0,
			clPlayer, 20,
			clHill, 1,
			clFood, 10),
		clFood);

	createFood(
		[
			[new SimpleObject(oFish, 2, 3, 0, 2)]
		],
		[
			25 * numPlayers
		],
		[
			avoidClasses(clFood, 10),
			stayClasses(clWater, 6)
		],
		clFood);

	yield 85;

	createStragglerTrees(
		[oBush, oBush2],
		avoidClasses(
			clWater, 5,
			clForest, 3,
			clHill, 1,
			clPlayer, 12,
			clMetal, 4,
			clRock, 4),
		clForest,
		stragglerTrees);

	placePlayersNomad(clPlayer,
		avoidClasses(
			clForest, 1,
			clWater, 4,
			clMetal, 4,
			clRock, 4,
			clHill, 4,
			clFood, 2));

	setSkySet("sunset 1");
	setSunRotation(randomAngle());
	setSunColor(0.8, 0.7, 0.6);
	setAmbientColor(0.6, 0.5, 0.6);
	setSunElevation(Math.PI * randFloat(1/12, 1/7));
	setWaterColor(0, 0.047, 0.286);
	setWaterTint(0.462, 0.756, 0.866);
	setWaterMurkiness(0.92);
	setWaterWaviness(1);
	setWaterType("clap");

	return g_Map;
}
