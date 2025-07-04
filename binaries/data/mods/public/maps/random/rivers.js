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
	let tShore = g_Terrains.shore;
	let tWater = g_Terrains.water;
	if (currentBiome() == "generic/india")
	{
		tShore = "tropic_dirt_b_plants";
		tWater = "tropic_dirt_b";
	}
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
	const aReeds = g_Decoratives.reeds;
	const aLillies = g_Decoratives.lillies;
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
	const heightShallows = -1;
	const heightLand = 1;

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
	const clShallow = g_Map.createTileClass();

	const [playerIDs, playerPosition, playerAngle, startAngle] =
		playerPlacementCircle(fractionToTiles(0.35));

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
			"count": 2
		},
		"Decoratives": {
			"template": aGrassShort
		}
	});

	g_Map.log("Creating central lake");
	createArea(
		new ClumpPlacer(diskArea(fractionToTiles(0.075)), 0.7, 0.1, Infinity, mapCenter),
		[
			new LayeredPainter([tShore, tWater], [1]),
			new SmoothElevationPainter(ELEVATION_SET, heightSeaGround, 4),
			new TileClassPainter(clWater)
		]);

	g_Map.log("Creating rivers between opponents");
	const numRivers = isNomad() ? randIntInclusive(4, 8) : numPlayers;
	const rivers = distributePointsOnCircle(numRivers, startAngle + Math.PI / numRivers,
		fractionToTiles(0.5), mapCenter)[0];
	for (let i = 0; i < numRivers; ++i)
	{
		if (isNomad() ? randBool() : areAllies(playerIDs[i], playerIDs[(i + 1) % numPlayers]))
			continue;

		const shallowLocation = randFloat(0.2, 0.7);
		const shallowWidth = randFloat(0.12, 0.21);

		paintRiver({
			"parallel": true,
			"start": rivers[i],
			"end": mapCenter,
			"width": scaleByMapSize(10, 30),
			"fadeDist": 5,
			"deviation": 0,
			"heightLand": heightLand,
			"heightRiverbed": heightSeaGround,
			"minHeight": heightSeaGround,
			"meanderShort": 10,
			"meanderLong": 0,
			"waterFunc": (position, height, riverFraction) => {

				clWater.add(position);

				const isShallow = height < heightShallows &&
					riverFraction > shallowLocation &&
					riverFraction < shallowLocation + shallowWidth;

				const newHeight = isShallow ? heightShallows : Math.max(height, heightSeaGround);

				if (g_Map.getHeight(position) < newHeight)
					return;

				g_Map.setHeight(position, newHeight);
				createTerrain(height >= 0 ? tShore : tWater).place(position);

				if (isShallow)
					clShallow.add(position);
			}
		});
	}
	yield 40;

	createBumps(avoidClasses(clWater, 2, clPlayer, 20));

	if (randBool())
		createHills([tMainTerrain, tCliff, tHill],
			avoidClasses(clPlayer, 20, clHill, 15, clWater, 2), clHill, scaleByMapSize(3, 15));
	else
		createMountains(tCliff,
			avoidClasses(clPlayer, 20, clHill, 15, clWater, 2), clHill, scaleByMapSize(3, 15));

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

	createDecoration(
		[
			[new SimpleObject(aReeds, 1, 3, 0, 1)],
			[new SimpleObject(aLillies, 1, 2, 0, 1)]
		],
		[
			scaleByMapAreaAbsolute(800),
			scaleByMapAreaAbsolute(800)
		],
		stayClasses(clShallow, 0));

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
			35 * numPlayers
		],
		[avoidClasses(clFood, 8), stayClasses(clWater, 6)],
		clFood);

	yield 85;

	createStragglerTrees(
		[oTree1, oTree2, oTree4, oTree3],
		avoidClasses(clWater, 5, clForest, 7, clHill, 1, clPlayer, 12, clMetal, 6, clRock, 6),
		clForest,
		stragglerTrees);

	placePlayersNomad(clPlayer,
		avoidClasses(clWater, 4, clForest, 1, clMetal, 4, clRock, 4, clFood, 2, clHill, 4));

	setWaterWaviness(3.0);
	setWaterType("lake");

	return g_Map;
}
