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
	const tHill = g_Terrains.hill;
	const tTier1Terrain = g_Terrains.tier1Terrain;
	const tTier2Terrain = g_Terrains.tier2Terrain;
	const tTier3Terrain = g_Terrains.tier3Terrain;
	const tTier4Terrain = g_Terrains.tier4Terrain;

	const oTree1 = g_Gaia.tree1;
	const oTree2 = g_Gaia.tree2;
	const oTree3 = g_Gaia.tree3;
	const oTree4 = g_Gaia.tree4;
	const oTree5 = g_Gaia.tree5;

	const aGrass = g_Decoratives.grass;
	const aGrassShort = g_Decoratives.grassShort;
	const aRockLarge = g_Decoratives.rockLarge;
	const aRockMedium = g_Decoratives.rockMedium;
	const aBushMedium = g_Decoratives.bushMedium;
	const aBushSmall = g_Decoratives.bushSmall;
	const aWaypointFlag = "actor|props/special/common/waypoint_flag_factions.xml";

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

	const oTreasureSeeker = "nonbuilder|undeletable|skirmish/units/default_support_female_citizen";

	const triggerPointAttacker = "trigger/trigger_point_A";
	const triggerPointTreasures = [
		"trigger/trigger_point_B",
		"trigger/trigger_point_C",
		"trigger/trigger_point_D"
	];

	const heightLand = 3;
	const heightHill = 30;

	globalThis.g_Map = new RandomMap(heightHill, tMainTerrain);

	const numPlayers = getNumPlayers();
	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();

	const clPlayer = g_Map.createTileClass();
	const clHill = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clDirt = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();
	const clLand = g_Map.createTileClass();
	const clWomen = g_Map.createTileClass();

	g_Map.log("Creating central area");
	createArea(
		new ClumpPlacer(diskArea(fractionToTiles(0.15)), 0.7, 0.1, Infinity, mapCenter),
		[
			new TerrainPainter(tMainTerrain),
			new SmoothElevationPainter(ELEVATION_SET, heightLand, 3),
			new TileClassPainter(clLand)
		]);
	yield 10;

	const [playerIDs, playerPosition, playerAngle, startAngle] =
		playerPlacementCircle(fractionToTiles(0.3));
	const halfway = distributePointsOnCircle(numPlayers, startAngle, fractionToTiles(0.375), mapCenter)[0]
		.map(v => v.round());
	const attacker = distributePointsOnCircle(numPlayers, startAngle, fractionToTiles(0.45), mapCenter)[0]
		.map(v => v.round());
	const passage = distributePointsOnCircle(numPlayers, startAngle + Math.PI / numPlayers,
		fractionToTiles(0.5), mapCenter)[0];

	g_Map.log("Creating player bases, passages, treasure seeker woman and attacker points");
	for (let i = 0; i < numPlayers; ++i)
	{
		placeStartingEntities(playerPosition[i], playerIDs[i],
			getStartingEntities(playerIDs[i]).filter(ent =>
				ent.Template.indexOf("civil_centre") != -1 ||
				ent.Template.indexOf("infantry") != -1));

		placePlayerBaseDecoratives({
			"playerPosition": playerPosition[i],
			"template": aGrassShort,
			"BaseResourceClass": clBaseResource
		});

		// Passage between player and neighbor
		createArea(
			new PathPlacer(mapCenter, passage[i], scaleByMapSize(14, 24), 0.4, scaleByMapSize(3, 9),
				0.2, 0.05),
			[
				new TerrainPainter(tMainTerrain),
				new SmoothElevationPainter(ELEVATION_SET, heightLand, 4)
			]);

		// Treasure seeker woman
		const femaleLocation = findLocationInDirectionBasedOnHeight(playerPosition[i], mapCenter, -3,
			3.5, 3).round();
		clWomen.add(femaleLocation);
		g_Map.placeEntityPassable(oTreasureSeeker, playerIDs[i], femaleLocation,
			playerAngle[i] + Math.PI);

		// Attacker spawn point
		g_Map.placeEntityAnywhere(aWaypointFlag, 0, attacker[i], Math.PI / 2);
		g_Map.placeEntityPassable(triggerPointAttacker, playerIDs[i], attacker[i], Math.PI / 2);

		// Preventing mountains in the area between player and attackers at
		// player
		addCivicCenterAreaToClass(playerPosition[i], clPlayer);
		clPlayer.add(attacker[i]);
		clPlayer.add(halfway[i]);
	}
	yield 20;

	paintTerrainBasedOnHeight(heightLand + 0.12, heightHill - 1, Elevation_IncludeMin_ExcludeMax, tCliff);
	paintTileClassBasedOnHeight(heightLand + 0.12, heightHill - 1, Elevation_IncludeMin_ExcludeMax,
		clHill);
	yield 30;

	const landConstraint = new StaticConstraint(stayClasses(clLand, 5));

	for (const triggerPointTreasure of triggerPointTreasures)
		createObjectGroupsDeprecated(
			new SimpleGroup([new SimpleObject(triggerPointTreasure, 1, 1, 0, 0)], true, clWomen),
			0,
			[avoidClasses(clPlayer, 5, clHill, 5), landConstraint],
			scaleByMapSize(40, 140),
			100);
	yield 35;

	createBumps(landConstraint);
	yield 40;

	const hillConstraint = new AndConstraint(
		[
			avoidClasses(clHill, 5),
			new StaticConstraint(avoidClasses(clPlayer, 20, clBaseResource, 3, clWomen, 5))
		]);
	if (randBool())
		createHills([tMainTerrain, tCliff, tHill],
			[hillConstraint, landConstraint],
			clHill,
			scaleByMapSize(10, 60) * numPlayers);
	else
		createMountains(tCliff,
			[hillConstraint, landConstraint],
			clHill,
			scaleByMapSize(10, 60) * numPlayers);
	yield 45;

	createHills(
		[tCliff, tCliff, tHill],
		[hillConstraint, avoidClasses(clLand, 5)],
		clHill,
		scaleByMapSize(15, 90) * numPlayers,
		undefined,
		undefined,
		undefined,
		undefined,
		55);
	yield 50;

	const [forestTrees, stragglerTrees] = getTreeCounts(...rBiomeTreeCount(1));
	createForests(
		[tMainTerrain, tForestFloor1, tForestFloor2, pForest1, pForest2],
		[avoidClasses(clForest, 5), new StaticConstraint(
			[
				avoidClasses(clPlayer, 20, clHill, 0, clBaseResource, 2, clWomen, 5),
				stayClasses(clLand, 4)
			])],
		clForest,
		forestTrees);

	yield 60;

	g_Map.log("Creating dirt patches");
	createLayeredPatches(
		[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
		[[tMainTerrain, tTier1Terrain], [tTier1Terrain, tTier2Terrain], [tTier2Terrain, tTier3Terrain]],
		[1, 1],
		[avoidClasses(clForest, 0, clHill, 0, clDirt, 5, clPlayer, 12, clWomen, 5), landConstraint],
		scaleByMapSize(15, 45),
		clDirt);
	yield 70;

	g_Map.log("Creating grass patches");
	createPatches(
		[scaleByMapSize(2, 4), scaleByMapSize(3, 7), scaleByMapSize(5, 15)],
		tTier4Terrain,
		[avoidClasses(clForest, 0, clHill, 0, clDirt, 5, clPlayer, 12, clWomen, 5), landConstraint],
		scaleByMapSize(15, 45),
		clDirt);
	yield 80;

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
		[avoidClasses(clForest, 0, clPlayer, 0, clHill, 0), landConstraint]);
	yield 90;

	createStragglerTrees(
		[oTree1, oTree2, oTree4, oTree3],
		[avoidClasses(clForest, 7, clHill, 1, clPlayer, 9), stayClasses(clLand, 7)],
		clForest,
		stragglerTrees);

	yield 95;

	return g_Map;
}
