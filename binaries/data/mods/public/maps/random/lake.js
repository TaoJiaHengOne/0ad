Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	const tMainTerrain = g_Terrains.mainTerrain;
	const tForestFloor1 = g_Terrains.forestFloor1;
	const tForestFloor2 = g_Terrains.forestFloor2;
	const tCliff = g_Terrains.cliff;
	const tTier1Terrain = g_Terrains.tier1Terrain;
	const tTier2Terrain = g_Terrains.tier2Terrain;
	const tTier3Terrain = g_Terrains.tier3Terrain;
	const tHill = g_Terrains.hill;
	const tRoad = g_Terrains.road;
	const tRoadWild = g_Terrains.roadWild;
	const tTier4Terrain = g_Terrains.tier4Terrain;
	const tShore = g_Terrains.shore;
	const tWater = g_Terrains.water;

	const oTree1 = g_Gaia.tree1;
	const oTree2 = g_Gaia.tree2;
	const oTree3 = g_Gaia.tree3;
	const oTree4 = g_Gaia.tree4;
	const oTree5 = g_Gaia.tree5;
	const oFruitBush = g_Gaia.fruitBush;
	const oMainHuntableAnimal = g_Gaia.mainHuntableAnimal;
	const oFish = g_Gaia.fish;
	const oSecondaryHuntableAnimal = g_Gaia.secondaryHuntableAnimal;
	const oStoneLarge = g_Gaia.stoneLarge;
	const oStoneSmall = g_Gaia.stoneSmall;
	const oMetalLarge = g_Gaia.metalLarge;
	const oMetalSmall = g_Gaia.metalSmall;

	const aGrass = g_Decoratives.grass;
	const aGrassShort = g_Decoratives.grassShort;
	const aRockLarge = g_Decoratives.rockLarge;
	const aRockMedium = g_Decoratives.rockMedium;
	const aBushMedium = g_Decoratives.bushMedium;
	const aBushSmall = g_Decoratives.bushSmall;

	const pForest1 = [
		tForestFloor2 + TERRAIN_SEPARATOR + oTree1,
		tForestFloor2 + TERRAIN_SEPARATOR + oTree2,
		tForestFloor2
	];
	const pForest2 = [
		tForestFloor1 + TERRAIN_SEPARATOR + oTree4,
		tForestFloor1 + TERRAIN_SEPARATOR + oTree5,
		tForestFloor1
	];

	const heightSeaGround = -3;
	const heightLand = 3;

	globalThis.g_Map = new RandomMap(heightLand, tMainTerrain);

	const numPlayers = getNumPlayers();
	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();

	const clPlayer = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clWater = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();

	const pattern = mapSettings.PlayerPlacement;
	const teamDist = (pattern == 'river') ? 0.55 : 0.35;
	const [playerIDs, playerPosition] =
		playerPlacementByPattern(
			pattern,
			fractionToTiles(teamDist),
			fractionToTiles(0.1),
			randomAngle(),
			undefined);

	g_Map.log("Preventing water in player territory");
	for (let i = 0; i < numPlayers; ++i)
		addCivicCenterAreaToClass(playerPosition[i], clPlayer);

	g_Map.log("Creating the lake...");
	createArea(
		new ChainPlacer(
			2,
			Math.floor(scaleByMapSize(5, 16)),
			Math.floor(scaleByMapSize(35, 200)),
			Infinity,
			mapCenter,
			0,
			[Math.floor(fractionToTiles(0.2))]),
		[
			new SmoothElevationPainter(ELEVATION_SET, heightSeaGround, 4),
			new TileClassPainter(clWater)
		],
		avoidClasses(clPlayer, 20));

	g_Map.log("Creating more shore jaggedness");
	createAreas(
		new ChainPlacer(2, Math.floor(scaleByMapSize(4, 6)), 3, Infinity),
		[
			new LayeredPainter([tCliff, tHill], [2]),
			new TileClassUnPainter(clWater)
		],
		borderClasses(clWater, 4, 7),
		scaleByMapSize(12, 130) * 2,
		150);

	paintTerrainBasedOnHeight(2.4, 3.4, 3, tMainTerrain);
	paintTerrainBasedOnHeight(1, 2.4, 0, tShore);
	paintTerrainBasedOnHeight(-8, 1, 2, tWater);
	paintTileClassBasedOnHeight(-6, 0, 1, clWater);

	placePlayerBases({
		"PlayerPlacement": [playerIDs, playerPosition],
		// PlayerTileClass marked above
		"BaseResourceClass": clBaseResource,
		"CityPatch": {
			"outerTerrain": tRoadWild,
			"innerTerrain": tRoad
		},
		"StartingAnimal": {
		},
		"Berries": {
			"template": oFruitBush
		},
		"Mines": {
			"types": [
				{ "template": oMetalLarge },
				{ "template": oStoneLarge }
			]
		},
		"Trees": {
			"template": oTree1,
			"count": 5
		},
		"Decoratives": {
			"template": aGrassShort
		}
	});
	yield 20;

	createBumps(avoidClasses(clWater, 2, clPlayer, 20));

	if (randBool())
		createHills([tMainTerrain, tCliff, tHill],
			avoidClasses(clPlayer, 20, clHill, 15, clWater, 2),
			clHill,
			scaleByMapSize(1, 4) * numPlayers);
	else
		createMountains(tCliff,
			avoidClasses(clPlayer, 20, clHill, 15, clWater, 2),
			clHill,
			scaleByMapSize(1, 4) * numPlayers);

	const [forestTrees, stragglerTrees] = getTreeCounts(...rBiomeTreeCount(1));
	createDefaultForests(
		[tMainTerrain, tForestFloor1, tForestFloor2, pForest1, pForest2],
		avoidClasses(clPlayer, 20, clForest, 17, clHill, 0, clWater, 2),
		clForest,
		forestTrees);

	yield 50;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[[tMainTerrain, tTier1Terrain], [tTier1Terrain, tTier2Terrain], [tTier2Terrain, tTier3Terrain]],
		[1, 1],
		avoidClasses(clWater, 3, clForest, 0, clHill, 0, clDirt, 5, clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);

	g_Map.log("Creating grass patches");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tTier4Terrain,
		avoidClasses(clWater, 3, clForest, 0, clHill, 0, clDirt, 5, clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);
	yield 55;

	g_Map.log("Creating metal mines");
	createBalancedMetalMines(
		oMetalSmall,
		oMetalLarge,
		clMetal,
		avoidClasses(clWater, 3, clForest, 1, clPlayer, scaleByMapSize(20, 35), clHill, 1)
	);

	g_Map.log("Creating stone mines");
	createBalancedStoneMines(
		oStoneSmall,
		oStoneLarge,
		clRock,
		avoidClasses(clWater, 3, clForest, 1, clPlayer, scaleByMapSize(20, 35), clHill, 1, clMetal, 10)
	);

	yield 65;

	let planetm = 1;

	if (currentBiome() == "generic/india")
		planetm = 8;

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
			planetm * scaleByMapAreaAbsolute(13),
			planetm * scaleByMapAreaAbsolute(13),
			planetm * scaleByMapAreaAbsolute(13)
		],
		avoidClasses(clWater, 0, clForest, 0, clPlayer, 0, clHill, 0));

	yield 70;

	createFood(
		[
			[new SimpleObject(oMainHuntableAnimal, 5, 7, 0, 4)],
			[new SimpleObject(oSecondaryHuntableAnimal, 2, 3, 0, 2)]
		],
		[
			3 * numPlayers,
			3 * numPlayers
		],
		avoidClasses(clWater, 3, clForest, 0, clPlayer, 20, clHill, 1, clFood, 20),
		clFood);

	createFood(
		[
			[new SimpleObject(oFruitBush, 5, 7, 0, 4)]
		],
		[
			3 * numPlayers
		],
		avoidClasses(clWater, 3, clForest, 0, clPlayer, 20, clHill, 1, clFood, 10),
		clFood);

	createFood(
		[
			[new SimpleObject(oFish, 2, 3, 0, 2)]
		],
		[
			65 * numPlayers
		],
		[avoidClasses(clFood, 12), stayClasses(clWater, 4)],
		clFood);

	yield 85;

	createStragglerTrees(
		[oTree1, oTree2, oTree4, oTree3],
		avoidClasses(clWater, 5, clForest, 7, clHill, 1, clPlayer, 12, clMetal, 6, clRock, 6),
		clForest,
		stragglerTrees);

	placePlayersNomad(clPlayer,
		avoidClasses(clWater, 4, clHill, 2, clForest, 1, clMetal, 4, clRock, 4, clFood, 2));

	setWaterWaviness(4.0);
	setWaterType("lake");

	return g_Map;
}
