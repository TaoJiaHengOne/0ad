Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap()
{
	TILE_CENTERED_HEIGHT_MAP = true;

	const tCity = "medit_city_pavement";
	const tCityPlaza = "medit_city_pavement";
	const tHill = [
		"medit_grass_shrubs",
		"medit_rocks_grass_shrubs",
		"medit_rocks_shrubs",
		"medit_rocks_grass",
		"medit_shrubs"
	];
	const tMainDirt = "medit_dirt";
	const tCliff = "medit_cliff_aegean";
	const tForestFloor = "medit_grass_wild";
	const tPrimary = [
		"medit_grass_shrubs",
		"medit_grass_wild",
		"medit_rocks_grass_shrubs",
		"medit_dirt_b",
		"medit_plants_dirt",
		"medit_grass_flowers"
	];
	const tDirt = "medit_dirt_b";
	const tDirt2 = "medit_rocks_grass";
	const tDirt3 = "medit_rocks_shrubs";
	const tDirtCracks = "medit_dirt_c";
	const tShoreLower = "medit_sand_wet";
	const tShoreUpper = "medit_sand";
	const tCoralsLower = "medit_sea_coral_deep";
	const tCoralsUpper = "medit_sea_coral_plants";
	const tWater = "medit_sea_depths";
	const tLavaOuter = "LavaTest06";
	const tLavaInner = "LavaTest05";

	const oBerryBush = "gaia/fruit/berry_01";
	const oDeer = "gaia/fauna_deer";
	const oFish = "gaia/fish/generic";
	const oSheep = "gaia/fauna_sheep";
	const oGoat = "gaia/fauna_goat";
	const oRabbit = "gaia/fauna_rabbit";
	const oStoneLarge = "gaia/rock/mediterranean_large";
	const oStoneSmall = "gaia/rock/mediterranean_small";
	const oMetalLarge = "gaia/ore/mediterranean_large";
	const oMetalSmall = "gaia/ore/mediterranean_small";
	const oDatePalm = "gaia/tree/cretan_date_palm_short";
	const oSDatePalm = "gaia/tree/cretan_date_palm_tall";
	const oCarob = "gaia/tree/carob";
	const oFanPalm = "gaia/tree/medit_fan_palm";
	const oPoplar = "gaia/tree/poplar_lombardy";
	const oCypress = "gaia/tree/cypress";
	const oBush = "gaia/tree/bush_temperate";

	const aBush1 = actorTemplate("props/flora/bush_medit_sm");
	const aBush2 = actorTemplate("props/flora/bush_medit_me");
	const aBush3 = actorTemplate("props/flora/bush_medit_la");
	const aBush4 = actorTemplate("props/flora/bush_medit_me");
	const aDecorativeRock = actorTemplate("geology/stone_granite_med");
	const aBridge = actorTemplate("props/special/eyecandy/bridge_edge_wooden");
	const aSmokeBig = actorTemplate("particle/smoke_volcano");
	const aSmokeSmall = actorTemplate("particle/smoke_curved");

	const pForest1 = [
		tForestFloor,
		tForestFloor + TERRAIN_SEPARATOR + oCarob,
		tForestFloor + TERRAIN_SEPARATOR + oDatePalm,
		tForestFloor + TERRAIN_SEPARATOR + oSDatePalm,
		tForestFloor];

	const pForest2 = [
		tForestFloor,
		tForestFloor + TERRAIN_SEPARATOR + oFanPalm,
		tForestFloor + TERRAIN_SEPARATOR + oPoplar,
		tForestFloor + TERRAIN_SEPARATOR + oCypress];

	const heightSeaGround = -8;
	const heightCoralsLower = -6;
	const heightCoralsUpper = -4;
	const heightSeaBump = -2.5;
	const heightShoreLower = -2;
	const heightBridge = -0.5;
	const heightShoreUpper = 1;
	const heightLand = 3;
	const heightOffsetBump = 2;
	const heightHill = 8;
	const heightVolano = 25;

	globalThis.g_Map = new RandomMap(heightSeaGround, tWater);
	const numPlayers = getNumPlayers();

	const clIsland = g_Map.createTileClass();
	const clWater = g_Map.createTileClass();
	const clPlayer = g_Map.createTileClass();
	const clPlayerIsland = g_Map.createTileClass();
	const clShore = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();
	const clGrass = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clVolcano = g_Map.createTileClass();
	const clBridge = g_Map.createTileClass();

	const playerIslandRadius = scaleByMapSize(20, 29);
	const bridgeLength = 16;
	const maxBridges = scaleByMapSize(2, 12);

	const [playerIDs, playerPosition] = playerPlacementRandom(sortAllPlayers());

	g_Map.log("Creating player islands");
	for (const position of playerPosition)
		createArea(
			new ChainPlacer(2, 6, scaleByMapSize(15, 50), Infinity, position, 0,
				[playerIslandRadius]),
			[
				new TerrainPainter(tPrimary),
				new SmoothElevationPainter(ELEVATION_SET, heightLand, 4),
				new TileClassPainter(clIsland)
			].concat(isNomad() ? [] : [new TileClassPainter(clPlayerIsland)]));
	yield 10;

	g_Map.log("Creating islands");
	createAreas(
		new ChainPlacer(6, Math.floor(scaleByMapSize(8, 10)), Math.floor(scaleByMapSize(10, 35)), 0.2),
		[
			new TerrainPainter(tPrimary),
			new SmoothElevationPainter(ELEVATION_SET, heightLand, 4),
			new TileClassPainter(clIsland)
		],
		avoidClasses(clIsland, 6),
		scaleByMapSize(25, 80));
	yield 20;

	// Notice that the Constraints become much shorter when avoiding water
	// rather than staying on islands
	g_Map.log("Marking water");
	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(clWater),
		new HeightConstraint(-Infinity, heightShoreLower));
	yield 30;

	g_Map.log("Creating undersea bumps");
	createAreas(
		new ChainPlacer(1, Math.floor(scaleByMapSize(4, 6)), Math.floor(scaleByMapSize(16, 40)), 0.5),
		new SmoothElevationPainter(ELEVATION_SET, heightSeaBump, 3),
		avoidClasses(clIsland, 2),
		scaleByMapSize(10, 50));
	yield 35;

	g_Map.log("Creating volcano");
	const areasVolcano = createAreas(
		new ClumpPlacer(diskArea(scaleByMapSize(4, 8)), 0.5, 0.5, 0.1),
		[
			new LayeredPainter([tLavaOuter, tLavaInner], [4]),
			new SmoothElevationPainter(ELEVATION_SET, heightVolano, 6),
			new TileClassPainter(clVolcano)
		],
		[
			new NearTileClassConstraint(clIsland, 8),
			avoidClasses(clHill, 5, clPlayerIsland, 0),
		],
		1,
		200);

	createBumps(avoidClasses(clWater, 0, clPlayer, 10, clVolcano, 0));
	yield 40;

	g_Map.log("Creating large bumps");
	createAreas(
		new ClumpPlacer(scaleByMapSize(20, 50), 0.3, 0.06, 1),
		new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetBump, 3),
		avoidClasses(clWater, 2, clVolcano, 0, clPlayer, 10),
		scaleByMapSize(20, 200));
	yield 45;

	g_Map.log("Creating hills");
	createAreas(
		new ChainPlacer(1, Math.floor(scaleByMapSize(4, 6)), Math.floor(scaleByMapSize(16, 40)), 0.5),
		[
			new LayeredPainter([tCliff, tHill], [2]),
			new SmoothElevationPainter(ELEVATION_SET, heightHill, 2),
			new TileClassPainter(clHill)
		],
		avoidClasses(clWater, 1, clPlayer, 12, clVolcano, 0, clHill, 15),
		scaleByMapSize(4, 13));
	yield 50;

	g_Map.log("Painting corals");
	paintTerrainBasedOnHeight(-Infinity, heightCoralsLower, Elevation_IncludeMin_ExcludeMax, tWater);
	paintTerrainBasedOnHeight(heightCoralsLower, heightCoralsUpper, Elevation_IncludeMin_ExcludeMax,
		tCoralsLower);
	paintTerrainBasedOnHeight(heightCoralsUpper, heightShoreLower, Elevation_IncludeMin_ExcludeMax,
		tCoralsUpper);

	g_Map.log("Painting shoreline");
	const areaShoreline = createArea(
		new HeightPlacer(Elevation_IncludeMin_ExcludeMax, heightShoreLower, heightShoreUpper),
		[
			new TerrainPainter(tShoreLower),
			new TileClassPainter(clShore)
		],
		avoidClasses(clVolcano, 0));

	createArea(
		new HeightPlacer(Elevation_IncludeMin_ExcludeMax, heightShoreUpper, heightLand),
		new TerrainPainter(tShoreUpper),
		avoidClasses(clVolcano, 0));
	yield 60;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[tDirt3, tDirt2, [tDirt, tMainDirt], [tDirtCracks, tMainDirt]],
		[1, 1, 1],
		avoidClasses(clWater, 4, clVolcano, 2, clForest, 1, clDirt, 2, clGrass, 2, clHill, 1),
		scaleByMapSize(15, 45),
		clDirt);
	yield 65;

	placePlayerBases({
		"PlayerPlacement": [playerIDs, playerPosition],
		"BaseResourceClass": clBaseResource,
		"PlayerTileClass": clPlayer,
		"Walls": "towers",
		"CityPatch": {
			"radius": playerIslandRadius / 4,
			"outerTerrain": tCityPlaza,
			"innerTerrain": tCity
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
		// sufficient trees around
		"Decoratives": {
			"template": aBush1
		}
	});
	yield 70;

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
			clWater, 4,
			clVolcano, 4,
			clPlayerIsland, 0,
			clBaseResource, 4,
			clForest, 3,
			clMetal, 4,
			clRock, 4),
		clRock,
		scaleByMapSize(4, 16));
	yield 75;

	g_Map.log("Creating metal mines");
	createMines(
		[
			[new SimpleObject(oMetalSmall, 0, 1, 0, 4), new SimpleObject(oMetalLarge, 1, 1, 0, 4)],
			[new SimpleObject(oMetalSmall, 2, 5, 1, 3)]
		],
		avoidClasses(
			clWater, 4,
			clPlayerIsland, 0,
			clVolcano, 4,
			clBaseResource, 4,
			clForest, 3,
			clMetal, 4,
			clRock, 4),
		clMetal,
		scaleByMapSize(4, 16));
	yield 80;

	placePlayersNomad(clPlayer,
		avoidClasses(clWater, 12, clVolcano, 4, clMetal, 4, clRock, 4, clHill, 4));

	const [forestTrees, stragglerTrees] = getTreeCounts(800, 4000, 0.7);
	createForests(
		[tForestFloor, tForestFloor, tForestFloor, pForest1, pForest2],
		avoidClasses(
			clWater, 2,
			clPlayer, 4,
			clVolcano, 2,
			clForest, 1,
			clBaseResource, 4,
			clMetal, 4,
			clRock, 4),
		clForest,
		forestTrees,
		200);
	yield 85;

	createFood(
		[
			[new SimpleObject(oSheep, 5, 7, 0, 4)],
			[new SimpleObject(oGoat, 2, 4, 0, 3)],
			[new SimpleObject(oDeer, 2, 4, 0, 2)],
			[new SimpleObject(oRabbit, 3, 9, 0, 4)],
			[new SimpleObject(oBerryBush, 3, 5, 0, 4)]
		],
		[
			scaleByMapSize(5, 20),
			scaleByMapSize(5, 20),
			scaleByMapSize(5, 20),
			scaleByMapSize(5, 20),
			3 * numPlayers
		],
		avoidClasses(
			clWater, 1,
			clPlayer, 15,
			clVolcano, 4,
			clBaseResource, 4,
			clHill, 2,
			clMetal, 4,
			clRock, 4),
		clFood);

	yield 87;

	createFood(
		[
			[new SimpleObject(oFish, 2, 3, 0, 2)]
		],
		[
			35 * numPlayers
		],
		avoidClasses(clIsland, 2, clFood, 8, clVolcano, 2),
		clFood);

	createStragglerTrees(
		[oPoplar, oCypress, oFanPalm, oDatePalm, oSDatePalm],
		avoidClasses(clWater, 1, clVolcano, 4, clPlayer, 12, clForest, 1, clMetal, 4, clRock, 4),
		clForest,
		stragglerTrees,
		200);

	g_Map.log("Creating bushes");
	createObjectGroupsDeprecated(
		new SimpleGroup([new SimpleObject(oBush, 3, 5, 0, 4)], true),
		0,
		[avoidClasses(
			clWater, 1,
			clVolcano, 4,
			clPlayer, 5,
			clForest, 1,
			clBaseResource, 4,
			clMetal, 4,
			clRock, 4)],
		scaleByMapSize(20, 50));

	createDecoration(
		[
			[
				new SimpleObject(aDecorativeRock, 1, 3, 0, 1)
			],
			[
				new SimpleObject(aBush2, 1, 2, 0, 1),
				new SimpleObject(aBush1, 1, 3, 0, 2),
				new SimpleObject(aBush4, 1, 2, 0, 1),
				new SimpleObject(aBush3, 1, 3, 0, 2)
			]
		],
		[
			scaleByMapAreaAbsolute(16),
			scaleByMapSize(40, 360)
		],
		avoidClasses(
			clWater, 4,
			clPlayer, 5,
			clVolcano, 4,
			clForest, 1,
			clBaseResource, 4,
			clRock, 4,
			clMetal, 4,
			clHill, 1));

	g_Map.log("Creating bridges");
	let bridges = 0;
	for (const bridgeStart of shuffleArray(areaShoreline.getPoints()))
	{
		if (new NearTileClassConstraint(clBridge, bridgeLength * 8).allows(bridgeStart))
			continue;

		for (let direction = 0; direction < 4; ++direction)
		{
			const bridgeAngle = direction * Math.PI / 2;
			const bridgeDirection = new Vector2D(1, 0).rotate(bridgeAngle);

			const areaOffset = new Vector2D(1, 1);

			const bridgeOffset = new Vector2D(direction % 2 ? 2 : 0, direction % 2 ? 0 : 2);
			const bridgeCenter1 =
				Vector2D.add(bridgeStart, Vector2D.mult(bridgeDirection, bridgeLength / 2));
			const bridgeCenter2 = Vector2D.add(bridgeCenter1, bridgeOffset);
			if (avoidClasses(clWater, 0).allows(bridgeCenter1) &&
				avoidClasses(clWater, 0).allows(bridgeCenter2))
				continue;

			const bridgeEnd1 =
				Vector2D.add(bridgeStart, Vector2D.mult(bridgeDirection, bridgeLength));
			const bridgeEnd2 = Vector2D.add(bridgeEnd1, bridgeOffset);
			if (avoidClasses(clShore, 0).allows(bridgeEnd1) &&
				avoidClasses(clShore, 0).allows(bridgeEnd2))
				continue;

			const bridgePerpendicular = bridgeDirection.perpendicular();
			const bridgeP = Vector2D.mult(bridgePerpendicular, bridgeLength / 2).round();
			if (avoidClasses(clWater, 0).allows(Vector2D.add(bridgeCenter1, bridgeP)) ||
			    avoidClasses(clWater, 0).allows(Vector2D.sub(bridgeCenter2, bridgeP)))
				continue;

			++bridges;

			// This bridge model is not centered on the horizontal plane,
			// so the angle is messy TILE_CENTERED_HEIGHT_MAP also
			// influences the outcome of the placement.
			const bridgeOrientation = direction % 2 ? 0 : Math.PI / 2;
			bridgeCenter1[direction % 2 ? "y" : "x"] += 0.25;
			bridgeCenter2[direction % 2 ? "y" : "x"] -= 0.25;

			g_Map.placeEntityAnywhere(aBridge, 0, bridgeCenter1, bridgeOrientation);
			g_Map.placeEntityAnywhere(aBridge, 0, bridgeCenter2, bridgeOrientation + Math.PI);

			createArea(
				new RectPlacer(Vector2D.sub(bridgeStart, areaOffset),
					Vector2D.add(bridgeEnd1, areaOffset)),
				[
					new ElevationPainter(heightBridge),
					new TileClassPainter(clBridge)
				]);

			for (const center of [bridgeStart, bridgeEnd2])
				createArea(
					new DiskPlacer(2, center),
					new SmoothingPainter(1, 1, 1));

			break;
		}

		if (bridges >= maxBridges)
			break;
	}

	g_Map.log("Creating smoke");
	if (areasVolcano.length)
	{
		createObjectGroupsByAreas(
			new SimpleGroup([new SimpleObject(aSmokeBig, 1, 1, 0, 4)], false),
			0,
			stayClasses(clVolcano, 6),
			scaleByMapSize(4, 12),
			20,
			areasVolcano);

		createObjectGroupsByAreas(
			new SimpleGroup([new SimpleObject(aSmokeSmall, 2, 2, 0, 4)], false),
			0,
			stayClasses(clVolcano, 4),
			scaleByMapSize(4, 12),
			20,
			areasVolcano);
	}
	yield 90;

	setSkySet("cumulus");
	setSunColor(0.87, 0.78, 0.49);
	setWaterColor(0, 0.501961, 1);
	setWaterTint(0.5, 1, 1);
	setWaterWaviness(4.0);
	setWaterType("ocean");
	setWaterMurkiness(0.49);

	setFogFactor(0.3);
	setFogThickness(0.25);

	setPPEffect("hdr");
	setPPContrast(0.62);
	setPPSaturation(0.51);
	setPPBloom(0.12);

	return g_Map;
}
