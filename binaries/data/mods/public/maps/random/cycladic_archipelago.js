Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap()
{
	const tOceanRockDeep = "medit_sea_coral_deep";
	const tOceanCoral = "medit_sea_coral_plants";
	const tBeachWet = "medit_sand_wet";
	const tBeachDry = "medit_sand";
	const tBeach = ["medit_rocks_grass", "medit_sand", "medit_rocks_grass_shrubs"];
	const tBeachBlend = ["medit_rocks_grass", "medit_rocks_grass_shrubs"];
	const tCity = "medit_city_tile";
	const tGrassDry = ["medit_grass_field_dry", "medit_grass_field_b"];
	const tGrass = ["medit_rocks_grass", "medit_rocks_grass", "medit_dirt", "medit_rocks_grass_shrubs"];
	const tGrassShrubs = "medit_shrubs";
	const tCliffShrubs = ["medit_cliff_aegean_shrubs", "medit_cliff_italia_grass", "medit_cliff_italia"];
	const tCliff = ["medit_cliff_italia", "medit_cliff_italia", "medit_cliff_italia_grass"];
	const tForestFloor = "medit_forestfloor_a";
	const tWater = "medit_sea_depths";

	const oBeech = "gaia/tree/euro_beech";
	const oBerryBush = "gaia/fruit/berry_01";
	const oCarob = "gaia/tree/carob";
	const oCypress1 = "gaia/tree/cypress";
	const oCypress2 = "gaia/tree/cypress";
	const oLombardyPoplar = "gaia/tree/poplar_lombardy";
	const oPalm = "gaia/tree/medit_fan_palm";
	const oPine = "gaia/tree/aleppo_pine";
	const oDateT = "gaia/tree/cretan_date_palm_tall";
	const oDateS = "gaia/tree/cretan_date_palm_short";
	const oDeer = "gaia/fauna_deer";
	const oFish = "gaia/fish/generic";
	const oWhale = "gaia/fauna_whale_humpback";
	const oStoneLarge = "gaia/rock/mediterranean_large";
	const oStoneSmall = "gaia/rock/mediterranean_small";
	const oMetalLarge = "gaia/ore/mediterranean_large";
	const oShipwreck = "gaia/treasure/shipwreck";
	const oShipDebris = "gaia/treasure/shipwreck_debris";

	const aRockLarge = "actor|geology/stone_granite_large.xml";
	const aRockMed = "actor|geology/stone_granite_med.xml";
	const aRockSmall = "actor|geology/stone_granite_small.xml";

	const pPalmForest = [tForestFloor+TERRAIN_SEPARATOR+oPalm, tGrass];
	const pPineForest = [tForestFloor+TERRAIN_SEPARATOR+oPine, tGrass];
	const pPoplarForest = [tForestFloor+TERRAIN_SEPARATOR+oLombardyPoplar, tGrass];
	const pMainForest = [
		tForestFloor + TERRAIN_SEPARATOR + oCarob,
		tForestFloor + TERRAIN_SEPARATOR + oBeech,
		tGrass,
		tGrass
	];

	const heightSeaGround = -5;
	const heightLand = 3;
	const heightHill = 12;
	const heightOffsetBump = 2;

	globalThis.g_Map = new RandomMap(heightSeaGround, tWater);

	const numPlayers = getNumPlayers();
	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();

	const clCoral = g_Map.createTileClass();
	const clPlayer = g_Map.createTileClass();
	const clIsland = g_Map.createTileClass();
	const clCity = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clWater = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();

	// array holding starting islands based on number of players
	const startingPlaces=[[0], [0, 3], [0, 2, 4], [0, 1, 3, 4], [0, 1, 2, 3, 4], [0, 1, 2, 3, 4, 5]];

	const startAngle = randomAngle();

	const islandRadius = scaleByMapSize(15, 40);
	const islandCount = Math.max(6, numPlayers);
	const islandPosition =
		distributePointsOnCircle(islandCount, startAngle, fractionToTiles(0.39), mapCenter)[0]
			.map(position => position.round());

	const centralIslandRadius = scaleByMapSize(15, 30);
	const centralIslandCount = Math.floor(scaleByMapSize(1, 4));
	const centralIslandPosition = new Array(numPlayers).fill(0).map((v, i) =>
		Vector2D.add(mapCenter, new Vector2D(fractionToTiles(randFloat(0.1, 0.16)), 0).rotate(
			-startAngle - Math.PI * (i * 2 / centralIslandCount + randFloat(-1, 1) / 8)).round()));

	const areas = [];

	let nPlayer = 0;
	const playerPosition = [];

	function createCycladicArchipelagoIsland(position, tileClass, radius, coralRadius)
	{
		// Deep ocean rocks
		createArea(
			new ClumpPlacer(diskArea(radius + coralRadius), 0.7, 0.1, Infinity, position),
			[
				new LayeredPainter([tOceanRockDeep, tOceanCoral], [5]),
				new TileClassPainter(clCoral)
			],
			avoidClasses(clCoral, 0, clPlayer, 0));

		// Island
		areas.push(
			createArea(
				new ClumpPlacer(diskArea(radius), 0.7, 0.1, Infinity, position),
				[
					new LayeredPainter(
						[tOceanCoral, tBeachWet, tBeachDry, tBeach, tBeachBlend, tGrass],
						[1, 3, 1, 1, 2]),
					new SmoothElevationPainter(ELEVATION_SET, heightLand, 5),
					new TileClassPainter(tileClass)
				],
				avoidClasses(clPlayer, 0)));
	}

	g_Map.log("Creating player islands");
	for (let i = 0; i < islandCount; ++i)
	{
		const isPlayerIsland = numPlayers >= 6 || i == startingPlaces[numPlayers - 1][nPlayer];
		if (isPlayerIsland)
		{
			playerPosition[nPlayer] = islandPosition[i];
			++nPlayer;
		}
		createCycladicArchipelagoIsland(islandPosition[i], isPlayerIsland ? clPlayer : clIsland,
			islandRadius, scaleByMapSize(1, 5));
	}

	g_Map.log("Creating central islands");
	for (const position of centralIslandPosition)
		createCycladicArchipelagoIsland(position, clIsland, centralIslandRadius, 2);

	placePlayerBases({
		"PlayerPlacement": [sortAllPlayers(), playerPosition],
		// PlayerTileClass is clCity here and painted below
		"BaseResourceClass": clBaseResource,
		"Walls": "towers",
		"CityPatch": {
			"radius": 6,
			"outerTerrain": tGrass,
			"innerTerrain": tCity,
			"painters": [
				new TileClassPainter(clCity)
			]
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
			"template": oPalm,
			"count": 2
		}
		// No decoratives
	});
	yield 20;

	g_Map.log("Creating bumps");
	createAreasInAreas(
		new ClumpPlacer(scaleByMapSize(20, 60), 0.3, 0.06, Infinity),
		new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetBump, 3),
		avoidClasses(clCity, 0),
		scaleByMapSize(25, 75), 15,
		areas);

	yield 34;

	g_Map.log("Creating hills");
	createAreasInAreas(
		new ClumpPlacer(scaleByMapSize(20, 150), 0.2, 0.1, Infinity),
		[
			new LayeredPainter([tCliff, tCliffShrubs], [2]),
			new SmoothElevationPainter(ELEVATION_SET, heightHill, 2),
			new TileClassPainter(clHill)
		],
		avoidClasses(clCity, 15, clHill, 15),
		scaleByMapSize(5, 30), 15,
		areas);

	yield 38;

	paintTileClassBasedOnHeight(-Infinity, 0, Elevation_ExcludeMin_ExcludeMax, clWater);

	g_Map.log("Creating forests");
	const forestTypes = [
		[[tForestFloor, tGrass, pPalmForest], [tForestFloor, pPalmForest]],
		[[tForestFloor, tGrass, pPineForest], [tForestFloor, pPineForest]],
		[[tForestFloor, tGrass, pPoplarForest], [tForestFloor, pPoplarForest]],
		[[tForestFloor, tGrass, pMainForest], [tForestFloor, pMainForest]]
	];

	for (const type of forestTypes)
		createAreasInAreas(
			new ClumpPlacer(randIntInclusive(6, 17), 0.1, 0.1, Infinity),
			[
				new LayeredPainter(type, [2]),
				new TileClassPainter(clForest)
			],
			avoidClasses(clCity, 1, clWater, 3, clForest, 3, clHill, 1, clBaseResource, 4),
			scaleByMapSize(10, 64),
			20,
			areas);
	yield 42;

	g_Map.log("Creating stone mines");
	let group = new SimpleGroup(
		[
			new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
			new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
		], true, clRock);
	createObjectGroupsByAreasDeprecated(group, 0,
		[avoidClasses(clWater, 1, clForest, 1, clHill, 1, clPlayer, 5, clRock, 6)],
		scaleByMapSize(4, 16), 200, areas
	);
	yield 46;

	g_Map.log("Creating small stone mines");
	group = new SimpleGroup([new SimpleObject(oStoneSmall, 2, 5, 1, 3)], true, clRock);
	createObjectGroupsByAreasDeprecated(group, 0,
		[avoidClasses(clWater, 1, clForest, 1, clHill, 1, clPlayer, 5, clRock, 6)],
		scaleByMapSize(4, 16), 200, areas
	);
	yield 50;

	g_Map.log("Creating metal mines");
	group = new SimpleGroup([new SimpleObject(oMetalLarge, 1, 1, 0, 4)], true, clMetal);
	createObjectGroupsByAreasDeprecated(group, 0,
		[avoidClasses(clWater, 1, clForest, 1, clHill, 1, clPlayer, 5, clMetal, 6, clRock, 6)],
		scaleByMapSize(4, 16), 200, areas
	);
	yield 54;

	g_Map.log("Creating shrub patches");
	for (const size of [scaleByMapSize(2, 32), scaleByMapSize(3, 48), scaleByMapSize(5, 80)])
		createAreasInAreas(
			new ClumpPlacer(size, 0.3, 0.06, 0.5),
			[
				new LayeredPainter([tBeachBlend, tGrassShrubs], [1]),
				new TileClassPainter(clDirt)
			],
			avoidClasses(clWater, 3, clHill, 0, clDirt, 6, clCity, 0, clBaseResource, 4),
			scaleByMapSize(4, 16),
			20,
			areas);
	yield 58;

	g_Map.log("Creating grass patches");
	for (const size of [scaleByMapSize(2, 32), scaleByMapSize(3, 48), scaleByMapSize(5, 80)])
		createAreasInAreas(
			new ClumpPlacer(size, 0.3, 0.06, 0.5),
			[
				new LayeredPainter([tGrassDry], []),
				new TileClassPainter(clDirt)
			],
			avoidClasses(clWater, 3, clHill, 0, clDirt, 6, clCity, 0, clBaseResource, 4),
			scaleByMapSize(4, 16),
			20,
			areas);
	yield 62;

	g_Map.log("Creating straggler trees");
	for (const tree of [oCarob, oBeech, oLombardyPoplar, oLombardyPoplar, oPine])
		createObjectGroupsByAreasDeprecated(
			new SimpleGroup([new SimpleObject(tree, 1, 1, 0, 1)], true, clForest),
			0,
			avoidClasses(
				clWater, 2,
				clForest, 2,
				clCity, 3,
				clBaseResource, 4,
				clRock, 6,
				clMetal, 6,
				clPlayer, 1,
				clHill, 1),
			scaleByMapSize(2, 38), 50, areas
		);
	yield 66;

	g_Map.log("Create straggler cypresses");
	group = new SimpleGroup(
		[new SimpleObject(oCypress2, 1, 3, 0, 3), new SimpleObject(oCypress1, 0, 2, 0, 2)],
		true
	);
	createObjectGroupsByAreasDeprecated(group, 0,
		avoidClasses(
			clWater, 2,
			clForest, 2,
			clCity, 3,
			clBaseResource, 4,
			clRock, 6,
			clMetal, 6,
			clPlayer, 1,
			clHill, 1),
		scaleByMapSize(5, 75), 50, areas
	);
	yield 70;

	g_Map.log("Create straggler date palms");
	group = new SimpleGroup(
		[new SimpleObject(oDateS, 1, 3, 0, 3), new SimpleObject(oDateT, 0, 2, 0, 2)],
		true
	);
	createObjectGroupsByAreasDeprecated(group, 0,
		avoidClasses(
			clWater, 2,
			clForest, 1,
			clCity, 0,
			clBaseResource, 4,
			clRock, 6,
			clMetal, 6,
			clPlayer, 1,
			clHill, 1),
		scaleByMapSize(5, 75), 50, areas
	);
	yield 74;

	g_Map.log("Creating rocks");
	group = new SimpleGroup(
		[
			new SimpleObject(aRockSmall, 0, 3, 0, 2),
			new SimpleObject(aRockMed, 0, 2, 0, 2),
			new SimpleObject(aRockLarge, 0, 1, 0, 2)
		]
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 0, clCity, 0),
		scaleByMapSize(30, 180), 50
	);
	yield 78;

	g_Map.log("Creating deer");
	group = new SimpleGroup(
		[new SimpleObject(oDeer, 5, 7, 0, 4)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 5, clForest, 1, clHill, 1, clCity, 10, clMetal, 6, clRock, 4, clFood, 8),
		3 * numPlayers, 50
	);
	yield 82;

	g_Map.log("Creating berry bushes");
	group = new SimpleGroup([new SimpleObject(oBerryBush, 5, 7, 0, 3)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 2, clForest, 1, clHill, 1, clCity, 10, clMetal, 6, clRock, 4, clFood, 8),
		1.5 * numPlayers, 100
	);
	yield 86;

	g_Map.log("Creating Fish");
	group = new SimpleGroup([new SimpleObject(oFish, 1, 1, 0, 3)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		[stayClasses(clWater, 1), avoidClasses(clFood, 8)],
		scaleByMapSize(100, 250), 100
	);
	yield 90;

	g_Map.log("Creating Whales");
	group = new SimpleGroup([new SimpleObject(oWhale, 1, 1, 0, 3)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		[stayClasses(clWater, 1), avoidClasses(clFood, 8, clPlayer, 4, clIsland, 4)],
		scaleByMapSize(10, 40), 100
	);
	yield 94;

	g_Map.log("Creating shipwrecks");
	group = new SimpleGroup([new SimpleObject(oShipwreck, 1, 1, 0, 3)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		[stayClasses(clWater, 1), avoidClasses(clFood, 8)],
		scaleByMapSize(6, 16), 100
	);
	yield 98;

	g_Map.log("Creating shipwreck debris");
	group = new SimpleGroup([new SimpleObject(oShipDebris, 1, 2, 0, 4)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		[stayClasses(clWater, 1), avoidClasses(clFood, 8)],
		scaleByMapSize(10, 20), 100
	);
	yield 99;

	placePlayersNomad(clPlayer,
		avoidClasses(
			clWater, 4,
			clForest, 1,
			clBaseResource, 4,
			clHill, 4,
			clMetal, 4,
			clRock, 4,
			clFood, 1));

	setSkySet("sunny");
	setWaterColor(0.2, 0.294, 0.49);
	setWaterTint(0.208, 0.659, 0.925);
	setWaterMurkiness(0.72);
	setWaterWaviness(3.0);
	setWaterType("ocean");

	return g_Map;
}
