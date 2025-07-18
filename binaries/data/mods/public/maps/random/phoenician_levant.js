Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap()
{
	const tCity = "medit_city_pavement";
	const tCityPlaza = "medit_city_pavement";
	const tHill = [
		"medit_dirt",
		"medit_dirt_b",
		"medit_dirt_c",
		"medit_rocks_grass",
		"medit_rocks_grass"
	];
	const tMainDirt = "medit_dirt";
	const tCliff = "medit_cliff_aegean";
	const tForestFloor = "medit_rocks_shrubs";
	const tGrass = "medit_rocks_grass";
	const tRocksShrubs = "medit_rocks_shrubs";
	const tRocksGrass = "medit_rocks_grass";
	const tDirt = "medit_dirt_b";
	const tDirtB = "medit_dirt_c";
	const tShore = "medit_sand";
	const tWater = "medit_sand_wet";

	const oGrapeBush = "gaia/fruit/grapes";
	const oDeer = "gaia/fauna_deer";
	const oFish = "gaia/fish/generic";
	const oSheep = "gaia/fauna_sheep";
	const oGoat = "gaia/fauna_goat";
	const oStoneLarge = "gaia/rock/mediterranean_large";
	const oStoneSmall = "gaia/rock/mediterranean_small";
	const oMetalLarge = "gaia/ore/mediterranean_large";
	const oDatePalm = "gaia/tree/cretan_date_palm_short";
	const oSDatePalm = "gaia/tree/cretan_date_palm_tall";
	const oCarob = "gaia/tree/carob";
	const oFanPalm = "gaia/tree/medit_fan_palm";
	const oPoplar = "gaia/tree/poplar_lombardy";
	const oCypress = "gaia/tree/cypress";

	const aBush1 = "actor|props/flora/bush_medit_sm.xml";
	const aBush2 = "actor|props/flora/bush_medit_me.xml";
	const aBush3 = "actor|props/flora/bush_medit_la.xml";
	const aBush4 = "actor|props/flora/bush_medit_me.xml";
	const aDecorativeRock = "actor|geology/stone_granite_med.xml";

	const pForest = [
		tForestFloor + TERRAIN_SEPARATOR + oDatePalm,
		tForestFloor + TERRAIN_SEPARATOR + oSDatePalm,
		tForestFloor + TERRAIN_SEPARATOR + oCarob,
		tForestFloor,
		tForestFloor];

	const heightSeaGround = -3;
	const heightShore = -1.5;
	const heightLand = 1;
	const heightIsland = 6;
	const heightHill = 15;
	const heightOffsetBump = 2;

	globalThis.g_Map = new RandomMap(heightLand, tHill);

	const numPlayers = getNumPlayers();
	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();
	const mapBounds = g_Map.getBounds();

	const clPlayer = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clWater = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();
	const clGrass = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clIsland = g_Map.createTileClass();

	const startAngle = randIntInclusive(0, 3) * Math.PI / 2;

	placePlayerBases({
		"PlayerPlacement": [
			sortAllPlayers(),
			playerPlacementLine(Math.PI / 2,
				new Vector2D(fractionToTiles(0.76), mapCenter.y), fractionToTiles(0.2)).map(pos =>
				pos.rotateAround(startAngle, mapCenter))
		],
		"PlayerTileClass": clPlayer,
		"BaseResourceClass": clBaseResource,
		"CityPatch": {
			"outerTerrain": tCityPlaza,
			"innerTerrain": tCity
		},
		"StartingAnimal": {
		},
		"Berries": {
			"template": oGrapeBush
		},
		"Mines": {
			"types": [
				{ "template": oMetalLarge },
				{ "template": oStoneLarge }
			]
		},
		"Trees": {
			"template": oCarob,
			"count": 2
		},
		"Decoratives": {
			"template": aBush1
		}
	});
	yield 30;

	paintRiver({
		"parallel": true,
		"start": new Vector2D(mapBounds.left, mapBounds.top).rotateAround(startAngle, mapCenter),
		"end": new Vector2D(mapBounds.left, mapBounds.bottom).rotateAround(startAngle, mapCenter),
		"width": mapSize,
		"fadeDist": scaleByMapSize(6, 25),
		"deviation": 0,
		"heightRiverbed": heightSeaGround,
		"heightLand": heightLand,
		"meanderShort": 20,
		"meanderLong": 0
	});
	yield 40;

	paintTileClassBasedOnHeight(-Infinity, heightLand, Elevation_ExcludeMin_ExcludeMax, clWater);
	paintTerrainBasedOnHeight(-Infinity, heightShore, Elevation_ExcludeMin_ExcludeMax, tWater);
	paintTerrainBasedOnHeight(heightShore, heightLand, Elevation_ExcludeMin_ExcludeMax, tShore);

	g_Map.log("Creating bumps");
	createAreas(
		new ClumpPlacer(scaleByMapSize(20, 50), 0.3, 0.06, Infinity),
		new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetBump, 2),
		avoidClasses(clWater, 2, clPlayer, 20),
		scaleByMapSize(100, 200));

	g_Map.log("Creating hills");
	createAreas(
		new ChainPlacer(1, Math.floor(scaleByMapSize(4, 6)), Math.floor(scaleByMapSize(16, 40)), 0.5),
		[
			new LayeredPainter([tCliff, tHill], [2]),
			new SmoothElevationPainter(ELEVATION_SET, heightHill, 2),
			new TileClassPainter(clHill)
		],
		avoidClasses(clPlayer, 20, clForest, 1, clHill, 15, clWater, 0),
		scaleByMapSize(1, 4) * numPlayers * 3);

	g_Map.log("Creating forests");
	const [forestTrees, stragglerTrees] = getTreeCounts(500, 2500, 0.5);
	const num = scaleByMapSize(10, 42);
	createAreas(
		new ChainPlacer(1, Math.floor(scaleByMapSize(3, 5)),
			forestTrees / (num * Math.floor(scaleByMapSize(2, 5))), 0.5),
		[
			new TerrainPainter([tForestFloor, pForest]),
			new TileClassPainter(clForest)
		],
		avoidClasses(clPlayer, 20, clForest, 10, clWater, 1, clHill, 1, clBaseResource, 3),
		num,
		50);
	yield 50;

	g_Map.log("Creating grass patches");
	for (const size of [scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)])
		createAreas(
			new ChainPlacer(1, Math.floor(scaleByMapSize(3, 5)), size, 0.5),
			[
				new LayeredPainter(
					[[tGrass, tRocksShrubs], [tRocksShrubs, tRocksGrass], [tRocksGrass, tGrass]],
					[1, 1]),
				new TileClassPainter(clDirt)
			],
			avoidClasses(clForest, 0, clGrass, 5, clPlayer, 10, clWater, 4, clDirt, 5, clHill, 1),
			scaleByMapSize(15, 45));
	yield 55;

	g_Map.log("Creating dirt patches");
	for (const size of [scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)])
		createAreas(
			new ChainPlacer(1, Math.floor(scaleByMapSize(3, 5)), size, 0.5),
			[
				new LayeredPainter(
					[[tDirt, tDirtB], [tDirt, tMainDirt], [tDirtB, tMainDirt]],
					[1, 1]),
				new TileClassPainter(clDirt)
			],
			avoidClasses(clForest, 0, clDirt, 5, clPlayer, 10, clWater, 4, clGrass, 5, clHill, 1),
			scaleByMapSize(15, 45));
	yield 60;

	g_Map.log("Creating cyprus");
	createAreas(
		new ClumpPlacer(diskArea(fractionToTiles(0.08)), 0.2, 0.1, 0.01),
		[
			new LayeredPainter([tShore, tHill], [12]),
			new SmoothElevationPainter(ELEVATION_SET, heightIsland, 8),
			new TileClassPainter(clIsland),
			new TileClassUnPainter(clWater)
		],
		[stayClasses (clWater, 8)],
		1,
		100);

	g_Map.log("Creating cyprus mines");
	const mines = [
		new SimpleGroup(
			[
				new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
				new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
			], true, clRock),
		new SimpleGroup([new SimpleObject(oMetalLarge, 1, 1, 0, 4)], true, clMetal),
		new SimpleGroup([new SimpleObject(oStoneSmall, 2, 5, 1, 3)], true, clRock)
	];
	for (const mine of mines)
		createObjectGroups(
			mine,
			0,
			[
				stayClasses(clIsland, 9),
				avoidClasses(clForest, 1, clRock, 8, clMetal, 8)
			],
			scaleByMapSize(4, 16));

	g_Map.log("Creating stone mines");
	let group = new SimpleGroup(
		[
			new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
			new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
		], true, clRock);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 1, clPlayer, 20, clRock, 10, clWater, 3, clHill, 1),
		scaleByMapSize(4, 16), 100
	);

	g_Map.log("Creating small stone quarries");
	group = new SimpleGroup([new SimpleObject(oStoneSmall, 2, 5, 1, 3)], true, clRock);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 1, clPlayer, 20, clRock, 10, clWater, 3, clHill, 1),
		scaleByMapSize(4, 16), 100
	);

	g_Map.log("Creating metal mines");
	group = new SimpleGroup([new SimpleObject(oMetalLarge, 1, 1, 0, 4)], true, clMetal);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 1, clPlayer, 20, clMetal, 10, clRock, 5, clWater, 3, clHill, 1),
		scaleByMapSize(4, 16), 100
	);

	yield 65;

	g_Map.log("Creating small decorative rocks");
	group = new SimpleGroup(
		[new SimpleObject(aDecorativeRock, 1, 3, 0, 1)],
		true
	);
	createObjectGroupsDeprecated(
		group, 0,
		avoidClasses(clWater, 1, clForest, 0, clPlayer, 0, clHill, 1),
		scaleByMapSize(16, 262), 50
	);

	g_Map.log("Creating shrubs");
	group = new SimpleGroup(
		[
			new SimpleObject(aBush2, 1, 2, 0, 1),
			new SimpleObject(aBush1, 1, 3, 0, 2),
			new SimpleObject(aBush4, 1, 2, 0, 1),
			new SimpleObject(aBush3, 1, 3, 0, 2)
		],
		true
	);
	createObjectGroupsDeprecated(
		group, 0,
		avoidClasses(clWater, 3, clPlayer, 0, clHill, 1),
		scaleByMapSize(40, 360), 50
	);
	yield 70;

	g_Map.log("Creating fish");
	group = new SimpleGroup([new SimpleObject(oFish, 1, 3, 2, 6)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		[avoidClasses(clIsland, 2, clFood, 10), stayClasses(clWater, 5)],
		20*scaleByMapSize(15, 20), 50
	);

	g_Map.log("Creating sheeps");
	group = new SimpleGroup([new SimpleObject(oSheep, 5, 7, 0, 4)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 7, clWater, 3, clFood, 10, clHill, 1),
		scaleByMapSize(5, 20), 50
	);

	g_Map.log("Creating goats");
	group = new SimpleGroup([new SimpleObject(oGoat, 2, 4, 0, 3)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 7, clWater, 3, clFood, 10, clHill, 1),
		scaleByMapSize(5, 20), 50
	);

	g_Map.log("Creating deers");
	group = new SimpleGroup([new SimpleObject(oDeer, 2, 4, 0, 2)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clForest, 0, clPlayer, 7, clWater, 3, clFood, 10, clHill, 1),
		scaleByMapSize(5, 20), 50
	);

	g_Map.log("Creating grape bushes");
	group = new SimpleGroup(
		[new SimpleObject(oGrapeBush, 5, 7, 0, 4)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 3, clForest, 0, clPlayer, 15, clHill, 1, clFood, 7),
		randIntInclusive(1, 4) * numPlayers + 2, 50
	);
	yield 90;

	const stragglerTreeConfig = [
		[1, avoidClasses(clForest, 0, clWater, 4, clPlayer, 8, clMetal, 6, clHill, 1)],
		[3, [stayClasses(clIsland, 9), avoidClasses(clRock, 4, clMetal, 4)]]
	];

	for (const [amount, constraint] of stragglerTreeConfig)
		createStragglerTrees(
			[oDatePalm, oSDatePalm, oCarob, oFanPalm, oPoplar, oCypress],
			constraint,
			clForest,
			amount * stragglerTrees);

	setSkySet("sunny");
	setSunColor(0.917, 0.828, 0.734);
	setWaterColor(0.263, 0.314, 0.631);
	setWaterTint(0.133, 0.725, 0.855);
	setWaterWaviness(2.0);
	setWaterType("ocean");
	setWaterMurkiness(0.8);

	setAmbientColor(0.447059, 0.509804, 0.54902);

	setSunElevation(0.671884);
	setSunRotation(-0.582913);

	setFogFactor(0.2);
	setFogThickness(0.15);
	setFogColor(0.8, 0.7, 0.6);

	setPPEffect("hdr");
	setPPContrast(0.53);
	setPPSaturation(0.47);
	setPPBloom(0.52);

	placePlayersNomad(clPlayer,
		avoidClasses(clWater, 4, clForest, 1, clMetal, 4, clRock, 4, clHill, 4, clFood, 2));

	return g_Map;
}
