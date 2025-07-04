Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	// Pick some biome defaults and overload a few settings.
	const tPrimary = g_Terrains.mainTerrain;
	const tForestFloor = g_Terrains.tier3Terrain;
	const tCliff = ["savanna_cliff_a", "savanna_cliff_a_red", "savanna_cliff_b", "savanna_cliff_b_red"];
	const tSecondary = g_Terrains.tier1Terrain;
	const tGrassShrubs = g_Terrains.tier2Terrain;
	const tDirt = g_Terrains.tier3Terrain;
	const tDirt2 = g_Terrains.tier4Terrain;
	const tDirt3 = g_Terrains.dirt;
	const tDirt4 = g_Terrains.dirt;
	const tCitytiles = "savanna_tile_a";
	const tShore = g_Terrains.shore;
	const tWater = g_Terrains.water;

	const oBaobab = pickRandom(["gaia/tree/baobab", "gaia/tree/baobab_3_mature", "gaia/tree/acacia"]);
	const oPalm = "gaia/tree/bush_tropic";
	const oPalm2 = "gaia/tree/cretan_date_palm_short";
	const oBerryBush = "gaia/fruit/berry_01";
	const oWildebeest = "gaia/fauna_wildebeest";
	const oZebra = "gaia/fauna_zebra";
	const oRhino = "gaia/fauna_rhinoceros_white";
	const oLion = "gaia/fauna_lion";
	const oLioness = "gaia/fauna_lioness";
	const oHawk = "birds/buzzard";
	const oGiraffe = "gaia/fauna_giraffe";
	const oGiraffe2 = "gaia/fauna_giraffe_infant";
	const oGazelle = "gaia/fauna_gazelle";
	const oElephant = "gaia/fauna_elephant_african_bush";
	const oElephant2 = "gaia/fauna_elephant_african_infant";
	const oCrocodile = "gaia/fauna_crocodile_nile";
	const oFish = g_Gaia.fish;
	const oStoneLarge = g_Gaia.stoneLarge;
	const oStoneSmall = g_Gaia.stoneSmall;
	const oMetalLarge = g_Gaia.metalLarge;
	const oMetalSmall = g_Gaia.metalSmall;

	const aBush = g_Decoratives.bushMedium;
	const aRock = g_Decoratives.rockMedium;

	const pForest = [
		tForestFloor + TERRAIN_SEPARATOR + oPalm,
		tForestFloor + TERRAIN_SEPARATOR + oPalm2,
		tForestFloor
	];

	const heightSeaGround = -5;
	const heightLand = 2;
	const heightCliff = 3;

	globalThis.g_Map = new RandomMap(heightLand, tPrimary);

	const numPlayers = getNumPlayers();
	const mapSize = g_Map.getSize();

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
			"innerTerrain": tCitytiles
		},
		"StartingAnimal": {
		},
		"Berries": {
			"template": oBerryBush
		},
		"Mines": {
			"types": [
				{
					"template": oMetalLarge
				},
				{
					"type": "stone_formation",
					"template": oStoneSmall,
					"terrain": tDirt4
				}
			]
		},
		"Trees": {
			"template": oBaobab,
			"count": scaleByMapSize(3, 12),
			"minDistGroup": 2,
			"maxDistGroup": 6,
			"minDist": 15,
			"maxDist": 16
		}
		// No decoratives
	});
	yield 20;

	// The specificity of this map is a bunch of watering holes & hills making
	// it somewhat cluttered.
	const nbHills = scaleByMapSize(6, 16);
	const nbWateringHoles = scaleByMapSize(4, 10);
	{
		g_Map.log("Creating hills");
		createHills([tDirt2, tCliff, tGrassShrubs], avoidClasses(clPlayer, 30, clHill, 15), clHill,
			nbHills);
		yield 30;

		g_Map.log("Creating water holes");
		createAreas(
			new ChainPlacer(1,
				Math.floor(scaleByMapSize(3, 5)),
				Math.floor(scaleByMapSize(60, 100)),
				Infinity),
			[
				new LayeredPainter([tShore, tWater], [1]),
				new SmoothElevationPainter(ELEVATION_SET, heightSeaGround, 7),
				new TileClassPainter(clWater)
			],
			avoidClasses(clPlayer, 22, clWater, 8, clHill, 2),
			nbWateringHoles);

		yield 45;

		paintTerrainBasedOnHeight(heightCliff, Infinity, 0, tCliff);
	}

	createBumps(avoidClasses(clWater, 2, clPlayer, 20));

	g_Map.log("Creating forests");

	createDefaultForests(
		[tPrimary, pForest, tForestFloor, pForest, pForest],
		avoidClasses(clPlayer, 20, clForest, 15, clHill, 0, clWater, 2),
		clForest,
		scaleByMapSize(200, 1000));
	yield 50;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[[tDirt, tDirt3], [tDirt2, tDirt4]],
		[2],
		avoidClasses(clWater, 3, clForest, 0, clHill, 0, clDirt, 5, clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);
	yield 55;

	g_Map.log("Creating shrubs");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tGrassShrubs,
		avoidClasses(clWater, 3, clForest, 0, clHill, 0, clDirt, 5, clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);
	yield 60;

	g_Map.log("Creating grass patches");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tSecondary,
		avoidClasses(clWater, 3, clForest, 0, clHill, 0, clDirt, 5, clPlayer, 12),
		scaleByMapSize(15, 45),
		clDirt);
	yield 65;

	g_Map.log("Creating metal mines");
	createBalancedMetalMines(
		oMetalSmall,
		oMetalLarge,
		clMetal,
		avoidClasses(clWater, 4, clForest, 1, clPlayer, scaleByMapSize(20, 35), clHill, 1)
	);

	g_Map.log("Creating stone mines");
	createBalancedStoneMines(
		oStoneSmall,
		oStoneLarge,
		clRock,
		avoidClasses(clWater, 4, clForest, 1, clPlayer, scaleByMapSize(20, 35), clHill, 1, clMetal, 10)
	);

	yield 70;

	createDecoration(
		[
			[new SimpleObject(aBush, 1, 3, 0, 1)],
			[new SimpleObject(aRock, 1, 2, 0, 1)]
		],
		[
			scaleByMapAreaAbsolute(8),
			scaleByMapAreaAbsolute(8)
		],
		avoidClasses(clWater, 0, clForest, 0, clPlayer, 0, clHill, 0));
	yield 75;

	// Roaming animals
	{
		const placeRoaming = function(name, objs)
		{
			g_Map.log("Creating roaming " + name);
			const group = new SimpleGroup(objs, true, clFood);
			createObjectGroups(group, 0,
				avoidClasses(clWater, 3, clPlayer, 20, clFood, 11, clHill, 4),
				scaleByMapSize(3, 9), 50
			);
		};

		placeRoaming("giraffes",
			[
				new SimpleObject(oGiraffe, 2, 4, 0, 4),
				new SimpleObject(oGiraffe2, 0, 2, 0, 4)
			]);
		placeRoaming("elephants",
			[
				new SimpleObject(oElephant, 2, 4, 0, 4),
				new SimpleObject(oElephant2, 0, 2, 0, 4)
			]);
		placeRoaming("lions",
			[
				new SimpleObject(oLion, 0, 1, 0, 4),
				new SimpleObject(oLioness, 2, 3, 0, 4)
			]);

		// Other roaming animals
		createFood(
			[
				[new SimpleObject(oHawk, 1, 1, 0, 3)],
				[new SimpleObject(oGazelle, 3, 5, 0, 3)],
				[new SimpleObject(oZebra, 3, 5, 0, 3)],
				[new SimpleObject(oWildebeest, 4, 6, 0, 3)],
				[new SimpleObject(oRhino, 1, 1, 0, 3)]
			],
			[
				3 * numPlayers,
				3 * numPlayers,
				3 * numPlayers,
				3 * numPlayers,
				3 * numPlayers,
			],
			avoidClasses(clFood, 20, clWater, 5, clHill, 2, clPlayer, 16),
			clFood);
	}

	// Animals that hang around watering holes
	{
		// TODO: these have a high retry factor because our mapgen
		//	constraint logic is bad.
		const placeWateringHoleAnimals = function(name, objs, numberOfGroups)
		{
			g_Map.log("Creating " + name + " around watering holes");
			const group = new SimpleGroup(objs, true, clFood);
			createObjectGroups(group, 0,
				borderClasses(clWater, 6, 3),
				numberOfGroups, 50
			);
		};
		placeWateringHoleAnimals(
			"crocodiles",
			[new SimpleObject(oCrocodile, 2, 3, 0, 3)],
			nbWateringHoles * 0.8
		);
		placeWateringHoleAnimals(
			"zebras",
			[new SimpleObject(oZebra, 2, 5, 0, 3)],
			nbWateringHoles
		);
		placeWateringHoleAnimals(
			"gazelles",
			[new SimpleObject(oGazelle, 2, 5, 0, 3)],
			nbWateringHoles
		);
	}


	g_Map.log("Creating other food sources");
	createFood(
		[
			[new SimpleObject(oBerryBush, 5, 7, 0, 4)]
		],
		[
			randIntInclusive(1, 4) * numPlayers + 2
		],
		avoidClasses(clWater, 3, clForest, 2, clPlayer, 20, clHill, 3, clFood, 10),
		clFood);

	createFood(
		[
			[new SimpleObject(oFish, 2, 3, 0, 2)]
		],
		[
			15 * numPlayers
		],
		[avoidClasses(clFood, 20), stayClasses(clWater, 6)],
		clFood);
	yield 85;


	g_Map.log("Creating straggler baobabs");
	const group = new SimpleGroup([new SimpleObject(oBaobab, 1, 1, 0, 3)], true, clForest);
	createObjectGroups(
		group,
		0,
		avoidClasses(clWater, 0, clForest, 2, clHill, 3, clPlayer, 12, clMetal, 4, clRock, 4),
		scaleByMapSize(15, 75)
	);


	placePlayersNomad(clPlayer,
		avoidClasses(clWater, 4,
			clForest, 1,
			clMetal, 4,
			clRock, 4,
			clHill, 4,
			clFood, 2));

	// Adjust some biome settings;

	setSkySet("sunny");
	setWaterType("clap");

	return g_Map;
}
