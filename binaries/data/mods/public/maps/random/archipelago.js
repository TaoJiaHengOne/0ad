Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	TILE_CENTERED_HEIGHT_MAP = true;

	setBiome(mapSettings.Biome);

	const biomeTweaks = Engine.ReadJSONFile("maps/random/archipelago_biome_tweaks.json");

	const tMainTerrain = g_Terrains.mainTerrain;
	const tForestFloor1 = g_Terrains.forestFloor1;
	const tForestFloor2 = g_Terrains.forestFloor2;
	const tCliff = g_Terrains.cliff;
	const tTier1Terrain = g_Terrains.tier1Terrain;
	const tTier2Terrain = g_Terrains.tier2Terrain;
	const tTier3Terrain = g_Terrains.tier3Terrain;
	const tHill = g_Terrains.hill;
	const tTier4Terrain = g_Terrains.dirt;
	const tRoad = g_Terrains.road;
	const tRoadWild = g_Terrains.roadWild;
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
	const oWoodTreasure = "gaia/treasure/wood";

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

	const heightSeaGround = -5;
	const heightLand = 3;
	const heightShore = 1;

	globalThis.g_Map = new RandomMap(heightSeaGround, tWater);

	const numPlayers = getNumPlayers();

	const clPlayer = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();
	const clLand = g_Map.createTileClass();

	const islandRadius = scaleByMapSize(22, 31);

	const [playerIDs, playerPosition] =
		playerPlacementByPattern(
			mapSettings.PlayerPlacement,
			fractionToTiles(0.35),
			fractionToTiles(0.1),
			randomAngle(),
			undefined);

	g_Map.log("Creating player islands");
	for (let i = 0; i < numPlayers; ++i)
		createArea(
			new ChainPlacer(
				2,
				Math.floor(scaleByMapSize(5, 10)),
				Math.floor(scaleByMapSize(25, 60)),
				Infinity,
				playerPosition[i],
				0,
				[Math.floor(islandRadius)]),
			[
				new SmoothElevationPainter(ELEVATION_SET, heightLand, 4),
				new TileClassPainter(clLand)
			]);

	g_Map.log("Creating random islands");
	createAreas(
		new ChainPlacer(
			Math.floor(scaleByMapSize(4, 8)),
			Math.floor(scaleByMapSize(8, 14)),
			Math.floor(scaleByMapSize(25, 60)),
			0.07,
			undefined,
			scaleByMapSize(30, 70)),
		[
			new SmoothElevationPainter(ELEVATION_SET, heightLand, 4),
			new TileClassPainter(clLand)
		],
		null,
		scaleByMapSize(1, 5) * randIntInclusive(5, 10));

	paintTerrainBasedOnHeight(heightLand - 0.6, heightLand + 0.4, 3, tMainTerrain);
	paintTerrainBasedOnHeight(heightShore, heightLand, 0, tShore);
	paintTerrainBasedOnHeight(heightSeaGround, heightShore, 2, tWater);

	placePlayerBases({
		"PlayerPlacement": [playerIDs, playerPosition],
		// PlayerTileClass marked below
		"BaseResourceClass": clBaseResource,
		"Walls": "towers",
		"CityPatch": {
			"outerTerrain": tRoadWild,
			"innerTerrain": tRoad,
			"radius": islandRadius / 3,
			"painters": [
				new TileClassPainter(clPlayer)
			]
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
		"Treasures": {
			"types": [
				{
					"template": oWoodTreasure,
					"count": 14
				}
			]
		},
		"Trees": {
			"template": oTree1,
			"count": scaleByMapSize(15, 30)
		},
		"Decoratives": {
			"template": aGrassShort,
		}
	});

	createBumps([avoidClasses(clPlayer, 10), stayClasses(clLand, 5)]);

	if (randBool())
		createHills([tMainTerrain, tCliff, tHill],
			[avoidClasses(clPlayer, 2, clHill, 15), stayClasses(clLand, 0)],
			clHill,
			scaleByMapSize(1, 4) * numPlayers);
	else
		createMountains(tCliff,
			[avoidClasses(clPlayer, 2, clHill, 15), stayClasses(clLand, 0)],
			clHill,
			scaleByMapSize(1, 4) * numPlayers);

	// adjust biomes (especially wood)
	const biomeSpecifics = biomeTweaks[currentBiome()] || biomeTweaks.baseline;

	const [forestTrees, stragglerTrees] = getTreeCounts(...rBiomeTreeCount(biomeSpecifics.treeAmount));
	createForests(
		[tMainTerrain, tForestFloor1, tForestFloor2, pForest1, pForest2],
		[
			avoidClasses(
				clPlayer, biomeSpecifics.forestPlayerSpacing,
				clForest, biomeSpecifics.forestForestSpacing,
				clHill, 0),
			stayClasses(clLand, 3)
		],
		clForest,
		forestTrees
	);
	yield 50;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[[tMainTerrain, tTier1Terrain], [tTier1Terrain, tTier2Terrain], [tTier2Terrain, tTier3Terrain]],
		[1, 1],
		[avoidClasses(clForest, 0, clHill, 0, clDirt, 3, clPlayer, 12), stayClasses(clLand, 7)],
		scaleByMapSize(15, 45),
		clDirt
	);

	g_Map.log("Creating grass patches");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tTier4Terrain,
		[avoidClasses(clForest, 0, clHill, 0, clDirt, 3, clPlayer, 12), stayClasses(clLand, 7)],
		scaleByMapSize(15, 45),
		clDirt
	);
	yield 55;

	g_Map.log("Creating stone mines");
	createMines(
		[
			[
				new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
				new SimpleObject(oStoneLarge, 1, 1, 0, 4)
			],
			[new SimpleObject(oStoneSmall, 2, 5, 1, 3)]
		],
		[avoidClasses(clForest, 1, clPlayer, 7, clRock, 10, clHill, 1), stayClasses(clLand, 6)],
		clRock
	);

	g_Map.log("Creating metal mines");
	createMines(
		[[new SimpleObject(oMetalLarge, 1, 1, 0, 4)]],
		[
			avoidClasses(clForest, 1, clPlayer, 7, clMetal, 10, clRock, 5, clHill, 1),
			stayClasses(clLand, 6)
		],
		clMetal
	);
	yield 65;

	const planetm = currentBiome() == "generic/india" ? 8 : 1;
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
		[avoidClasses(clForest, 0, clPlayer, 0, clHill, 0), stayClasses(clLand, 5)]);
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
		[avoidClasses(clForest, 0, clPlayer, 1, clHill, 1, clFood, 20), stayClasses(clLand, 3)],
		clFood);

	yield 75;

	createFood(
		[
			[new SimpleObject(oFruitBush, 5, 7, 0, 4)]
		],
		[
			3 * numPlayers
		],
		[avoidClasses(clForest, 0, clPlayer, 1, clHill, 1, clFood, 10), stayClasses(clLand, 3)],
		clFood);

	yield 80;

	createFood(
		[
			[new SimpleObject(oFish, 2, 3, 0, 2)]
		],
		[
			35 * numPlayers
		],
		avoidClasses(clLand, 3, clPlayer, 2, clFood, 15),
		clFood);

	yield 85;

	createStragglerTrees(
		[oTree1, oTree2, oTree4, oTree3],
		[
			avoidClasses(clForest, 7, clHill, 1, clPlayer, 3, clMetal, 6, clRock, 6),
			stayClasses(clLand, 7)
		],
		clForest,
		stragglerTrees);

	placePlayersNomad(
		clPlayer,
		new AndConstraint([
			stayClasses(clLand, 4),
			avoidClasses(clForest, 1, clMetal, 4, clRock, 4, clHill, 4, clFood, 2)]));

	setWaterWaviness(4.0);
	setWaterType("ocean");

	return g_Map;
}
