Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("heightmap");

export function* generateMap()
{
	setSkySet("fog");
	setFogFactor(0.35);
	setFogThickness(0.19);

	setWaterColor(0.501961, 0.501961, 0.501961);
	setWaterTint(0.25098, 0.501961, 0.501961);
	setWaterWaviness(0.5);
	setWaterType("clap");
	setWaterMurkiness(0.75);

	setPPSaturation(0.37);
	setPPContrast(0.4);
	setPPBrightness(0.4);
	setPPEffect("hdr");
	setPPBloom(0.4);

	const oStoneLarge = 'gaia/rock/alpine_large';
	const oMetalLarge = 'gaia/ore/alpine_large';
	const oFish = "gaia/fish/generic";

	const aGrass = 'actor|props/flora/grass_soft_small_tall.xml';
	const aGrassShort = 'actor|props/flora/grass_soft_large.xml';
	const aRockLarge = 'actor|geology/stone_granite_med.xml';
	const aRockMedium = 'actor|geology/stone_granite_med.xml';
	const aBushMedium = 'actor|props/flora/bush_medit_me.xml';
	const aBushSmall = 'actor|props/flora/bush_medit_sm.xml';
	const aReeds = 'actor|props/flora/reeds_pond_lush_b.xml';

	const terrainPrimary = ["temp_grass_plants", "temp_plants_bog"];
	const terrainWood = ['alpine_forrestfloor|gaia/tree/oak', 'alpine_forrestfloor|gaia/tree/pine'];
	const terrainWoodBorder = [
		'new_alpine_grass_mossy|gaia/tree/oak',
		'alpine_forrestfloor|gaia/tree/pine',
		'temp_grass_long|gaia/tree/bush_temperate',
		'temp_grass_clovers|gaia/fruit/berry_01',
		'temp_grass_clovers_2|gaia/fruit/grapes',
		'temp_grass_plants|gaia/fauna_deer',
		'temp_grass_plants|gaia/fauna_rabbit',
		'new_alpine_grass_dirt_a'
	];
	const terrainBase = [
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_grass_plants|gaia/fauna_sheep'
	];
	const terrainBaseBorder = [
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_d',
		'temp_grass_plants',
		'temp_plants_bog',
		'temp_grass_plants',
		'temp_grass_plants'
	];
	const baseTex = ['temp_road', 'temp_road_overgrown'];
	const terrainPath = ['temp_road', 'temp_road_overgrown'];
	const tWater = ['dirt_brown_d'];
	const tWaterBorder = ['dirt_brown_d'];

	const heightLand = 1;
	const heightOffsetPath = -0.1;

	globalThis.g_Map = new RandomMap(heightLand, terrainPrimary);

	const clPlayer = g_Map.createTileClass();
	const clPath = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clWater = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();
	const clOpen = g_Map.createTileClass();

	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();
	const mapRadius = mapSize/2;

	const numPlayers = getNumPlayers();
	const baseRadius = 15;
	const minPlayerRadius = Math.min(mapRadius - 1.5 * baseRadius, 5/8 * mapRadius);
	const maxPlayerRadius = Math.min(mapRadius - baseRadius, 3/4 * mapRadius);

	const playerPosition = [];
	const playerAngleStart = randomAngle();
	const playerAngleAddAvrg = 2 * Math.PI / numPlayers;
	const playerAngleMaxOff = playerAngleAddAvrg/4;

	const resourceRadius = fractionToTiles(1/3);

	// Set up woods.
	// For large maps there are memory errors with too many trees.
	// A density of 256x192/mapArea works with 0 players.
	// Around each player there is an area without trees
	// so with more players the max density can increase a bit.
	// Has to be tweeked but works ok.
	const maxTreeDensity = Math.min(256 * (192 + 8 * numPlayers) / Math.square(mapSize), 1);
	// 1 means 50% chance in deepest wood, 0.5 means 25% chance in deepest
	// wood.
	const bushChance = 1/3;

	// Set height limits and water level by map size.

	// Set target min and max height depending on map size
	// to make average steepness about the same on all map sizes.
	const heightRange = {
		'min': MIN_HEIGHT * (g_Map.size + 512) / 8192,
		'max': MAX_HEIGHT * (g_Map.size + 512) / 8192,
		'avg': (MIN_HEIGHT * (g_Map.size + 512) + MAX_HEIGHT * (g_Map.size + 512)) / 16384
	};

	// Set average water coverage.
	// NOTE: Since erosion is not predictable actual water coverage might vary
	//	much with the same values.
	const averageWaterCoverage = 1/5;
	const heightSeaGround = -MIN_HEIGHT + heightRange.min +
		averageWaterCoverage * (heightRange.max - heightRange.min);
	const heightSeaGroundAdjusted = heightSeaGround + MIN_HEIGHT;
	setWaterHeight(heightSeaGround);

	// Setting a 3x3 grid as initial heightmap.
	const initialReliefmap = [
		[heightRange.max, heightRange.max, heightRange.max],
		[heightRange.max, heightRange.min, heightRange.max],
		[heightRange.max, heightRange.max, heightRange.max]
	];

	setBaseTerrainDiamondSquare(heightRange.min, heightRange.max, initialReliefmap);

	g_Map.log("Smoothing map");
	createArea(
		new MapBoundsPlacer(),
		new SmoothingPainter(1, 0.8, 5));

	rescaleHeightmap(heightRange.min, heightRange.max);

	const heighLimits = [
		// 0 Deep water
		heightRange.min + 1/3 * (heightSeaGroundAdjusted - heightRange.min),
		// 1 Medium Water
		heightRange.min + 2/3 * (heightSeaGroundAdjusted - heightRange.min),
		// 2 Shallow water
		heightRange.min + (heightSeaGroundAdjusted - heightRange.min),
		// 3 Shore
		heightSeaGroundAdjusted + 1/8 * (heightRange.max - heightSeaGroundAdjusted),
		// 4 Low ground
		heightSeaGroundAdjusted + 2/8 * (heightRange.max - heightSeaGroundAdjusted),
		// 5 Player and path height
		heightSeaGroundAdjusted + 3/8 * (heightRange.max - heightSeaGroundAdjusted),
		// 6 High ground
		heightSeaGroundAdjusted + 4/8 * (heightRange.max - heightSeaGroundAdjusted),
		// 7 Lower forest border
		heightSeaGroundAdjusted + 5/8 * (heightRange.max - heightSeaGroundAdjusted),
		// 8 Forest
		heightSeaGroundAdjusted + 6/8 * (heightRange.max - heightSeaGroundAdjusted),
		// 9 Upper forest border
		heightSeaGroundAdjusted + 7/8 * (heightRange.max - heightSeaGroundAdjusted),
		// 10 Hilltop
		heightSeaGroundAdjusted + (heightRange.max - heightSeaGroundAdjusted)];

	g_Map.log("Locating and smoothing playerbases");
	for (let i = 0; i < numPlayers; ++i)
	{
		playerPosition[i] = Vector2D.add(
			mapCenter,
			new Vector2D(randFloat(minPlayerRadius, maxPlayerRadius), 0).rotate(
				-((playerAngleStart + i * playerAngleAddAvrg + randFloat(0, playerAngleMaxOff)) %
					(2 * Math.PI)))).round();

		createArea(
			new ClumpPlacer(diskArea(20), 0.8, 0.8, Infinity, playerPosition[i]),
			new SmoothElevationPainter(ELEVATION_SET, g_Map.getHeight(playerPosition[i]), 20));
	}

	placePlayerBases({
		"PlayerPlacement": [sortAllPlayers(), playerPosition],
		"BaseResourceClass": clBaseResource,
		"Walls": false,
		// player class painted below
		"CityPatch": {
			"radius": 0.8 * baseRadius,
			"smoothness": 1/8,
			"painters": [
				new TerrainPainter([baseTex], [baseRadius/4, baseRadius/4]),
				new TileClassPainter(clPlayer)
			]
		},
		// No chicken
		"Berries": {
			"template": "gaia/fruit/berry_01",
			"minCount": 2,
			"maxCount": 2
		},
		"Mines": {
			"types": [
				{ "template": oMetalLarge },
				{ "template": oStoneLarge }
			],
			"distance": 15,
			"minAngle": Math.PI / 2,
			"maxAngle": Math.PI
		},
		"Trees": {
			"template": "gaia/tree/oak_large",
			"count": 2
		}
	});

	g_Map.log("Creating mines");
	for (const [minHeight, maxHeight] of
		[
			[heighLimits[3], (heighLimits[4] + heighLimits[3]) / 2],
			[(heighLimits[5] + heighLimits[6]) / 2, heighLimits[7]]
		])
	{
		for (const [template, tileClass] of [[oStoneLarge, clRock], [oMetalLarge, clMetal]])
			createObjectGroups(
				new SimpleGroup([new SimpleObject(template, 1, 1, 0, 4)], true, tileClass),
				0,
				[
					new HeightConstraint(minHeight, maxHeight),
					avoidClasses(clForest, 4, clPlayer, 20, clMetal, 40, clRock, 40)
				],
				scaleByMapSize(2, 8),
				100,
				false);
	}

	yield 50;

	g_Map.log("Painting textures");
	const betweenShallowAndShore = (heighLimits[3] + heighLimits[2]) / 2;
	createArea(
		new HeightPlacer(Elevation_IncludeMin_IncludeMax, heighLimits[2], betweenShallowAndShore),
		new LayeredPainter([terrainBase, terrainBaseBorder], [5]));

	paintTileClassBasedOnHeight(heighLimits[2], betweenShallowAndShore, 1, clOpen);

	createArea(
		new HeightPlacer(Elevation_IncludeMin_IncludeMax, heightRange.min, heighLimits[2]),
		new LayeredPainter([tWaterBorder, tWater], [2]));

	paintTileClassBasedOnHeight(heightRange.min, heighLimits[2], 1, clWater);
	yield 60;

	g_Map.log("Painting paths");
	const pathBlending = numPlayers <= 4;
	for (let i = 0; i < numPlayers + (pathBlending ? 1 : 0); ++i)
		for (let j = pathBlending ? 0 : i + 1; j < numPlayers + 1; ++j)
		{
			const pathStart = i < numPlayers ? playerPosition[i] : mapCenter;
			const pathEnd = j < numPlayers ? playerPosition[j] : mapCenter;

			createArea(
				new RandomPathPlacer(pathStart, pathEnd, 1.75, baseRadius / 2, pathBlending),
				[
					new TerrainPainter(terrainPath),
					new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetPath, 1),
					new TileClassPainter(clPath)
				],
				avoidClasses(clPath, 0, clOpen, 0, clWater, 4, clBaseResource, 4));
		}
	yield 75;

	g_Map.log("Creating decoration");
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
		avoidClasses(clForest, 1, clPlayer, 0, clPath, 3, clWater, 3));

	yield 80;

	g_Map.log("Growing fish");
	createFood(
		[
			[new SimpleObject(oFish, 2, 3, 0, 2)]
		],
		[
			100 * numPlayers
		],
		[avoidClasses(clFood, 5), stayClasses(clWater, 4)],
		clFood);

	yield 85;

	g_Map.log("Planting reeds");
	const types = [aReeds];
	for (const type of types)
		createObjectGroupsDeprecated(
			new SimpleGroup([new SimpleObject(type, 1, 1, 0, 0)], true),
			0,
			borderClasses(clWater, 0, 6),
			scaleByMapSize(1, 2) * 1000,
			1000);

	yield 90;

	g_Map.log("Planting trees");
	for (let x = 0; x < mapSize; x++)
		for (let z = 0; z < mapSize; z++)
		{
			const position = new Vector2D(x, z);

			if (!g_Map.validTile(position))
				continue;

			// The 0.5 is a correction for the entities placed on the
			// center of tiles
			const radius = Vector2D.add(position, new Vector2D(0.5, 0.5)).distanceTo(mapCenter);
			let minDistToSL = mapSize;
			for (let i = 0; i < numPlayers; ++i)
				minDistToSL = Math.min(minDistToSL, position.distanceTo(playerPosition[i]));

			// Woods tile based
			const tDensFactSL = Math.max(Math.min((minDistToSL - baseRadius) / baseRadius, 1), 0);
			const tDensFactRad = Math.abs((resourceRadius - radius) / resourceRadius);
			const tDensActual = (maxTreeDensity * tDensFactSL * tDensFactRad)*0.75;

			if (!randBool(tDensActual))
				continue;

			const border = tDensActual < randFloat(0, bushChance * maxTreeDensity);

			const constraint = border ?
				avoidClasses(clPath, 1, clOpen, 2, clWater, 3, clMetal, 4, clRock, 4) :
				avoidClasses(clPath, 2, clOpen, 3, clWater, 4, clMetal, 4, clRock, 4);

			if (constraint.allows(position))
			{
				clForest.add(position);
				createTerrain(border ? terrainWoodBorder : terrainWood).place(position);
			}
		}

	placePlayersNomad(clPlayer, avoidClasses(clWater, 4, clForest, 1, clFood, 2, clMetal, 4, clRock, 4));

	yield 100;

	return g_Map;
}
