Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	const tDirtMain = g_Terrains.mainTerrain;
	const tCity = g_Terrains.road;
	const tCliff = g_Terrains.cliff;
	const tLakebed1 = g_Terrains.lakebed1;
	const tLakebed2 = g_Terrains.lakebed2;
	const tForestFloor = g_Terrains.forestFloor1;
	const tRocky = g_Terrains.tier1Terrain;
	const tRocks = g_Terrains.tier2Terrain;
	const tGrass = g_Terrains.tier3Terrain;

	const oOak = g_Gaia.tree1;
	const oGrapesBush = g_Gaia.fruitBush;
	const oCamel = g_Gaia.mainHuntableAnimal;
	const oSheep = g_Gaia.secondaryHuntableAnimal;
	const oGoat = g_Gaia.thirdHuntableAnimal;
	const oStoneLarge = g_Gaia.stoneLarge;
	const oStoneSmall = g_Gaia.stoneSmall;
	const oMetalLarge = g_Gaia.metalLarge;

	const aDecorativeRock = g_Decoratives.rockMedium;
	const aBush1 = g_Decoratives.bush1;
	const aBush2 = g_Decoratives.bush2;
	const aBush3 = g_Decoratives.bush3;
	const aBush4 = g_Decoratives.bush4;
	const aBushes = [aBush1, aBush2, aBush3, aBush4];

	const pForestO = [
		tForestFloor + TERRAIN_SEPARATOR + oOak,
		tForestFloor + TERRAIN_SEPARATOR + oOak,
		tForestFloor,
		tDirtMain,
		tDirtMain
	];

	const heightLand = 10;
	const heightOffsetValley = -10;

	globalThis.g_Map = new RandomMap(heightLand, tDirtMain);

	const numPlayers = getNumPlayers();
	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();

	const clPlayer = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clPatch = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();
	const clCP = g_Map.createTileClass();

	const pattern = mapSettings.PlayerPlacement;
	const teamDist = (pattern == "river") ? 0.50 : 0.35;
	const [playerIDs, playerPosition] =
		playerPlacementByPattern(
			pattern,
			fractionToTiles(teamDist),
			fractionToTiles(0.1),
			randomAngle(),
			undefined);

	placePlayerBases({
		"PlayerPlacement": [playerIDs, playerPosition],
		"BaseResourceClass": clBaseResource,
		"CityPatch": {
			"outerTerrain": tCity,
			"innerTerrain": tCity,
			"painters": [
				new TileClassPainter(clPlayer)
			]
		},
		"StartingAnimal": {
		},
		"Berries": {
			"template": oGrapesBush,
		},
		"Mines": {
			"types": [
				{ "template": oMetalLarge },
				{ "template": oStoneLarge }
			],
			"groupElements": shuffleArray(aBushes).map(t => new SimpleObject(t, 1, 1, 3, 4))
		},
		"Trees": {
			"template": oOak,
			"count": 3
		}
		// No decoratives
	});
	yield 10;

	g_Map.log("Creating rock patches");
	createAreas(
		new ChainPlacer(1, Math.floor(scaleByMapSize(3, 6)), Math.floor(scaleByMapSize(20, 45)), 0),
		[
			new TerrainPainter(tRocky),
			new TileClassPainter(clPatch)
		],
		avoidClasses(clPatch, 2, clPlayer, 0),
		scaleByMapSize(5, 20));
	yield 15;

	g_Map.log("Creating secondary rock patches");
	createAreas(
		new ChainPlacer(1, Math.floor(scaleByMapSize(3, 5)), Math.floor(scaleByMapSize(15, 40)), 0),
		[
			new TerrainPainter([tRocky, tRocks]),
			new TileClassPainter(clPatch)
		],
		avoidClasses(clPatch, 2, clPlayer, 4),
		scaleByMapSize(15, 50));
	yield 20;

	g_Map.log("Creating dirt patches");
	createAreas(
		new ChainPlacer(
			1,
			Math.floor(scaleByMapSize(3, 5)),
			Math.floor(scaleByMapSize(15, 40)),
			0),
		[
			new TerrainPainter([tGrass]),
			new TileClassPainter(clPatch)
		],
		avoidClasses(clPatch, 2, clPlayer, 4),
		scaleByMapSize(15, 50));
	yield 25;

	g_Map.log("Creating centeral plateau");
	createArea(
		new ChainPlacer(
			2,
			Math.floor(scaleByMapSize(5, 13)),
			Math.floor(scaleByMapSize(35, 200)),
			Infinity,
			mapCenter,
			0,
			[Math.floor(scaleByMapSize(18, 68))]),
		[
			new LayeredPainter([tLakebed2, tLakebed1], [6]),
			new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetValley, 8),
			new TileClassPainter(clCP)
		],
		avoidClasses(clPlayer, 18));
	yield 30;

	g_Map.log("Creating hills");
	for (let i = 0; i < scaleByMapSize(20, 80); ++i)
		createMountain(
			Math.floor(scaleByMapSize(40, 60)),
			Math.floor(scaleByMapSize(3, 4)),
			Math.floor(scaleByMapSize(6, 12)),
			Math.floor(scaleByMapSize(4, 10)),
			avoidClasses(clPlayer, 7, clCP, 5, clHill, Math.floor(scaleByMapSize(18, 25))),
			randIntExclusive(0, mapSize),
			randIntExclusive(0, mapSize),
			tCliff,
			clHill,
			14);
	yield 35;

	g_Map.log("Creating forests");
	const [forestTrees, stragglerTrees] = getTreeCounts(500, 2500, 0.7);
	const types = [
		[[tDirtMain, tForestFloor, pForestO], [tForestFloor, pForestO]],
		[[tDirtMain, tForestFloor, pForestO], [tForestFloor, pForestO]]
	];
	const forestSize = forestTrees / (scaleByMapSize(3, 6) * numPlayers);
	const num = Math.floor(forestSize / types.length);
	for (const type of types)
		createAreas(
			new ChainPlacer(
				Math.floor(scaleByMapSize(1, 2)),
				Math.floor(scaleByMapSize(2, 5)),
				Math.floor(forestSize / Math.floor(scaleByMapSize(8, 3))),
				Infinity),
			[
				new LayeredPainter(type, [2]),
				new TileClassPainter(clForest)
			],
			avoidClasses(
				clPlayer, 6,
				clForest, 10,
				clHill, 1,
				clCP, 1),
			num);
	yield 50;

	g_Map.log("Creating stone mines");
	createObjectGroupsDeprecated(
		new SimpleGroup(
			[
				new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
				new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4),
				new RandomObject(aBushes, 2, 4, 0, 2)
			],
			true,
			clRock),
		0,
		[avoidClasses(clForest, 1, clPlayer, 10, clRock, 10, clHill, 1, clCP, 1)],
		scaleByMapSize(2, 8),
		100);

	g_Map.log("Creating small stone quarries");
	createObjectGroupsDeprecated(
		new SimpleGroup(
			[
				new SimpleObject(oStoneSmall, 2, 5, 1, 3),
				new RandomObject(aBushes, 2, 4, 0, 2)
			], true, clRock),
		0,
		[avoidClasses(clForest, 1, clPlayer, 10, clRock, 10, clHill, 1, clCP, 1)],
		scaleByMapSize(2, 8),
		100);

	g_Map.log("Creating metal mines");
	createObjectGroupsDeprecated(
		new SimpleGroup(
			[
				new SimpleObject(oMetalLarge, 1, 1, 0, 4),
				new RandomObject(aBushes, 2, 4, 0, 2)
			], true, clMetal),
		0,
		[avoidClasses(clForest, 1, clPlayer, 10, clMetal, 10, clRock, 5, clHill, 1, clCP, 1)],
		scaleByMapSize(2, 8),
		100);

	g_Map.log("Creating centeral stone mines");
	createObjectGroupsDeprecated(
		new SimpleGroup(
			[
				new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
				new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4),
				new RandomObject(aBushes, 2, 4, 0, 2)
			],
			true,
			clRock),
		0,
		stayClasses(clCP, 6),
		5 * scaleByMapSize(5, 30),
		50);

	g_Map.log("Creating small stone quarries");
	let group = new SimpleGroup(
		[
			new SimpleObject(oStoneSmall, 2, 5, 1, 3),
			new RandomObject(aBushes, 2, 4, 0, 2)
		], true, clRock);
	createObjectGroupsDeprecated(group, 0,
		stayClasses(clCP, 6),
		5 * scaleByMapSize(5, 30), 50
	);

	g_Map.log("Creating centeral metal mines");
	group = new SimpleGroup(
		[
			new SimpleObject(oMetalLarge, 1, 1, 0, 4),
			new RandomObject(aBushes, 2, 4, 0, 2)
		], true, clMetal);
	createObjectGroupsDeprecated(group, 0,
		stayClasses(clCP, 6),
		5 * scaleByMapSize(5, 30), 50
	);

	yield 60;

	g_Map.log("Creating small decorative rocks");
	group = new SimpleGroup(
		[new SimpleObject(aDecorativeRock, 1, 3, 0, 1)],
		true
	);
	createObjectGroupsDeprecated(
		group, 0,
		avoidClasses(clForest, 0, clPlayer, 0, clHill, 0),
		scaleByMapSize(16, 262), 50
	);

	yield 65;

	g_Map.log("Creating bushes");
	group = new SimpleGroup(
		[new SimpleObject(aBush2, 1, 2, 0, 1), new SimpleObject(aBush1, 1, 3, 0, 2)],
		true
	);
	createObjectGroupsDeprecated(
		group, 0,
		avoidClasses(clForest, 0, clPlayer, 0, clHill, 0),
		scaleByMapSize(8, 131), 50
	);

	yield 70;

	g_Map.log("Creating goat");
	group = new SimpleGroup(
		[new SimpleObject(oGoat, 5, 7, 0, 4)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 1, clHill, 1, clFood, 20, clCP, 2),
		3 * numPlayers, 50
	);

	g_Map.log("Creating sheep");
	group = new SimpleGroup(
		[new SimpleObject(oSheep, 2, 3, 0, 2)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 1, clHill, 1, clFood, 20, clCP, 2),
		3 * numPlayers, 50
	);

	g_Map.log("Creating grape bush");
	group = new SimpleGroup(
		[new SimpleObject(oGrapesBush, 5, 7, 0, 4)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 20, clHill, 1, clFood, 10, clCP, 2),
		randIntInclusive(1, 4) * numPlayers + 2, 50
	);

	g_Map.log("Creating camels");
	group = new SimpleGroup(
		[new SimpleObject(oCamel, 2, 3, 0, 2)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		stayClasses(clCP, 2),
		3 * numPlayers, 50
	);

	yield 90;

	createStragglerTrees(
		[oOak],
		avoidClasses(
			clForest, 1,
			clHill, 1,
			clPlayer, 1,
			clBaseResource, 6,
			clMetal, 6,
			clRock, 6,
			clCP, 2),
		clForest,
		stragglerTrees);

	placePlayersNomad(clPlayer, avoidClasses(clForest, 1, clMetal, 4, clRock, 4, clHill, 4, clFood, 2));

	return g_Map;
}
