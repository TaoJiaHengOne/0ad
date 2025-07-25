import { addAnimals, addBerries, addDecoration, addForests, addLayeredPatches, addMetal, addProps, addStone,
	addStragglerTrees } from "maps/random/rmgen2/gaia.js";
import { addElements, allAmounts, allMixes, allSizes, createBases, initTileClasses } from
	"maps/random/rmgen2/setup.js";

Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	const topTerrain = g_Terrains.tier2Terrain;

	const heightValley = 0;
	const heightPath = 10;
	const heightDen = 15;
	const heightHill = 50;

	globalThis.g_Map = new RandomMap(heightHill, topTerrain);

	const mapCenter = g_Map.getCenter();
	const numPlayers = getNumPlayers();
	const startAngle = randomAngle();

	initTileClasses(["step"]);
	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.land));

	yield 10;

	createBases(
		...playerPlacementByPattern(
			"circle",
			fractionToTiles(0.4),
			fractionToTiles(randFloat(0.05, 0.1)),
			startAngle,
			undefined),
		true);
	yield 20;

	createSunkenTerrain();
	yield 30;

	addElements([
		{
			"func": addLayeredPatches,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.dirt, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12,
				g_TileClasses.step, 5
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		},
		{
			"func": addLayeredPatches,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.dirt, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		},
		{
			"func": addLayeredPatches,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.dirt, 5,
				g_TileClasses.forest, 2
			],
			"stay": [g_TileClasses.player, 1],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		},
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.forest, 2
			],
			"stay": [g_TileClasses.player, 1],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		},
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12,
				g_TileClasses.step, 2
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		},
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		},
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12
			],
			"stay": [g_TileClasses.step, 7],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["scarce"]
		}
	]);
	yield 40;

	addElements(shuffleArray([
		{
			"func": addMetal,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 10,
				g_TileClasses.metal, 20
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["tons"]
		},
		{
			"func": addMetal,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.player, 10,
				g_TileClasses.rock, 10,
				g_TileClasses.metal, 20,
				g_TileClasses.mountain, 5,
				g_TileClasses.step, 5
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": allAmounts
		},
		{
			"func": addStone,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 20,
				g_TileClasses.metal, 10
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["tons"]
		},
		{
			"func": addStone,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.player, 10,
				g_TileClasses.rock, 20,
				g_TileClasses.metal, 10,
				g_TileClasses.mountain, 5,
				g_TileClasses.step, 5
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": allAmounts
		},
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 18,
				g_TileClasses.metal, 3,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 3
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": ["normal", "big"],
			"mixes": ["same"],
			"amounts": ["tons"]
		},
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 3,
				g_TileClasses.forest, 18,
				g_TileClasses.metal, 3,
				g_TileClasses.mountain, 5,
				g_TileClasses.player, 5,
				g_TileClasses.rock, 3,
				g_TileClasses.step, 1
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": ["normal", "big"],
			"mixes": ["same"],
			"amounts": ["tons"]
		}
	]));
	yield 60;

	addElements(shuffleArray([
		{
			"func": addBerries,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 30,
				g_TileClasses.forest, 5,
				g_TileClasses.metal, 10,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 10
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["tons"]
		},
		{
			"func": addBerries,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 30,
				g_TileClasses.forest, 5,
				g_TileClasses.metal, 10,
				g_TileClasses.mountain, 5,
				g_TileClasses.player, 10,
				g_TileClasses.rock, 10,
				g_TileClasses.step, 5
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addAnimals,
			"avoid": [
				g_TileClasses.animals, 20,
				g_TileClasses.baseResource, 5,
				g_TileClasses.forest, 0,
				g_TileClasses.metal, 1,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 1
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["tons"]
		},
		{
			"func": addAnimals,
			"avoid": [
				g_TileClasses.animals, 20,
				g_TileClasses.baseResource, 5,
				g_TileClasses.forest, 0,
				g_TileClasses.metal, 1,
				g_TileClasses.mountain, 5,
				g_TileClasses.player, 10,
				g_TileClasses.rock, 1,
				g_TileClasses.step, 5
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addStragglerTrees,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 7,
				g_TileClasses.metal, 3,
				g_TileClasses.player, 12,
				g_TileClasses.rock, 3
			],
			"stay": [g_TileClasses.settlement, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["tons"]
		},
		{
			"func": addStragglerTrees,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 7,
				g_TileClasses.metal, 3,
				g_TileClasses.mountain, 5,
				g_TileClasses.player, 10,
				g_TileClasses.rock, 3,
				g_TileClasses.step, 5
			],
			"stay": [g_TileClasses.valley, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["normal", "many", "tons"]
		},
		{
			"func": addStragglerTrees,
			"avoid": [
				g_TileClasses.player, 10,
				g_TileClasses.baseResource, 5,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.metal, 5,
				g_TileClasses.rock, 5
			],
			"stay": [g_TileClasses.player, 1],
			"sizes": ["huge"],
			"mixes": ["same"],
			"amounts": ["tons"]
		}
	]));
	yield 75;

	addElements([
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.valley, 4,
				g_TileClasses.player, 4,
				g_TileClasses.settlement, 4,
				g_TileClasses.step, 4
			],
			"stay": [g_TileClasses.land, 2],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["tons"]
		}
	]);
	yield 80;

	addElements([
		{
			"func": addProps,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.valley, 4,
				g_TileClasses.player, 4,
				g_TileClasses.settlement, 4,
				g_TileClasses.step, 4
			],
			"stay": [g_TileClasses.land, 2],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["scarce"]
		}
	]);
	yield 85;

	addElements([
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.player, 4,
				g_TileClasses.settlement, 4,
				g_TileClasses.step, 4
			],
			"stay": [g_TileClasses.mountain, 2],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["tons"]
		}
	]);
	yield 90;

	addElements([
		{
			"func": addProps,
			"avoid": [
				g_TileClasses.baseResource, 5,
				g_TileClasses.player, 4,
				g_TileClasses.settlement, 4,
				g_TileClasses.step, 4
			],
			"stay": [g_TileClasses.mountain, 2],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["scarce"]
		}
	]);
	yield 95;

	placePlayersNomad(
		g_TileClasses.player,
		[
			new HeightConstraint(heightValley, heightPath),
			avoidClasses(
				g_TileClasses.forest, 1,
				g_TileClasses.metal, 4,
				g_TileClasses.rock, 4,
				g_TileClasses.animals, 2)
		]);

	return g_Map;

	function createSunkenTerrain()
	{
		const base = g_Terrains.mainTerrain;
		let middle = g_Terrains.dirt;
		let lower = g_Terrains.tier2Terrain;
		let road = g_Terrains.road;

		if (currentBiome() == "generic/arctic")
		{
			middle = g_Terrains.tier2Terrain;
			lower = g_Terrains.tier1Terrain;
		}

		if (currentBiome() == "generic/alpine")
		{
			middle = g_Terrains.shore;
			lower = g_Terrains.tier4Terrain;
		}

		if (currentBiome() == "generic/aegean")
		{
			middle = g_Terrains.tier1Terrain;
			lower = g_Terrains.forestFloor1;
		}

		if (currentBiome() == "generic/savanna")
		{
			middle = g_Terrains.tier2Terrain;
			lower = g_Terrains.tier4Terrain;
		}

		if (currentBiome() == "generic/india" || currentBiome() == "generic/autumn")
			road = g_Terrains.roadWild;

		if (currentBiome() == "generic/autumn")
			middle = g_Terrains.shore;

		let expSize = diskArea(fractionToTiles(0.14)) / numPlayers;
		const expDist = 0.1 + numPlayers / 200;
		let expAngle = 0.75;

		if (numPlayers <= 2)
		{
			expSize = diskArea(fractionToTiles(0.075));
			expAngle = 0.72;
		}

		let nRoad = 0.44;
		let nExp = 0.425;

		if (numPlayers < 4)
		{
			nRoad = 0.42;
			nExp = 0.4;
		}

		g_Map.log("Creating central valley");
		createArea(
			new DiskPlacer(fractionToTiles(0.29), mapCenter),
			[
				new LayeredPainter([g_Terrains.cliff, lower], [3]),
				new SmoothElevationPainter(ELEVATION_SET, heightValley, 3),
				new TileClassPainter(g_TileClasses.valley)
			]);

		g_Map.log("Creating central hill");
		createArea(
			new DiskPlacer(fractionToTiles(0.21), mapCenter),
			[
				new LayeredPainter([g_Terrains.cliff, topTerrain], [3]),
				new SmoothElevationPainter(ELEVATION_SET, heightHill, 3),
				new TileClassPainter(g_TileClasses.mountain)
			]);

		const getCoords = (distance, playerID, playerIDOffset) => {
			const angle = startAngle + (playerID + playerIDOffset) * 2 * Math.PI / numPlayers;
			return Vector2D.add(mapCenter, new Vector2D(fractionToTiles(distance), 0).rotate(-angle))
				.round();
		};

		for (let i = 0; i < numPlayers; ++i)
		{
			const playerPosition = getCoords(0.4, i, 0);

			// Path from player to expansion
			const expansionPosition = getCoords(expDist, i, expAngle);
			createArea(
				new PathPlacer(playerPosition, expansionPosition, 12, 0.7, 0.5, 0.1, -1),
				[
					new LayeredPainter([g_Terrains.cliff, middle, road], [3, 4]),
					new SmoothElevationPainter(ELEVATION_SET, heightPath, 3),
					new TileClassPainter(g_TileClasses.step)
				]);

			// Path from player to neighbor
			for (const neighborOffset of [-0.5, 0.5])
			{
				const neighborPosition = getCoords(nRoad, i, neighborOffset);
				const pathPosition = getCoords(0.47, i, 0);
				createArea(
					new PathPlacer(pathPosition, neighborPosition, 19, 0.4, 0.5, 0.1, -0.6),
					[
						new LayeredPainter([g_Terrains.cliff, middle, road], [3, 6]),
						new SmoothElevationPainter(ELEVATION_SET, heightPath, 3),
						new TileClassPainter(g_TileClasses.step)
					]);
			}

			// Den
			createArea(
				new ClumpPlacer(diskArea(fractionToTiles(0.1)) / (isNomad() ? 2 : 1), 0.9, 0.3,
					Infinity, playerPosition),
				[
					new LayeredPainter([g_Terrains.cliff, base], [3]),
					new SmoothElevationPainter(ELEVATION_SET, heightDen, 3),
					new TileClassPainter(g_TileClasses.valley)
				]);

			// Expansion
			createArea(
				new ClumpPlacer(expSize, 0.9, 0.3, Infinity, expansionPosition),
				[
					new LayeredPainter([g_Terrains.cliff, base], [3]),
					new SmoothElevationPainter(ELEVATION_SET, heightDen, 3),
					new TileClassPainter(g_TileClasses.settlement)
				],
				[avoidClasses(g_TileClasses.settlement, 2)]);
		}

		g_Map.log("Creating the expansions between players");
		for (let i = 0; i < numPlayers; ++i)
		{
			const position = getCoords(nExp, i, 0.5);
			createArea(
				new ClumpPlacer(expSize, 0.9, 0.3, Infinity, position),
				[
					new LayeredPainter([g_Terrains.cliff, lower], [3]),
					new SmoothElevationPainter(ELEVATION_SET, heightValley, 3),
					new TileClassPainter(g_TileClasses.settlement)
				]);
		}
	}
}
