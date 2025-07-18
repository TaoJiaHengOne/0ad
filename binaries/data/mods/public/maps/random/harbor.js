import { addAnimals, addBerries, addBluffs, addDecoration, addFish, addForests, addHills, addLayeredPatches,
	addMetal, addMountains, addPlateaus, addProps, addStone, addStragglerTrees } from
	"maps/random/rmgen2/gaia.js";
import { addElements, allAmounts, allMixes, allSizes, createBases, initTileClasses } from
	"maps/random/rmgen2/setup.js";

Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	const heightSeaGround = -18;
	const heightLand = 2;
	const heightOffsetHarbor = -11;

	globalThis.g_Map = new RandomMap(heightLand, g_Terrains.mainTerrain);

	initTileClasses();

	setFogFactor(0.04);

	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.land));

	yield 10;

	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();

	const startAngle = randomAngle();
	const [playerIDs, playerPosition] =
		createBases(
			...playerPlacementByPattern(
				"circle",
				fractionToTiles(0.38),
				fractionToTiles(0.05),
				startAngle,
				undefined),
			true);
	yield 20;

	addCenterLake();
	yield 30;

	if (mapSize >= 192)
	{
		addHarbors();
		yield 40;
	}

	addSpines();
	yield 50;

	addElements([
		{
			"func": addFish,
			"avoid": [
				g_TileClasses.fish, 12,
				g_TileClasses.hill, 8,
				g_TileClasses.mountain, 8,
				g_TileClasses.player, 8,
				g_TileClasses.spine, 4
			],
			"stay": [g_TileClasses.water, 7],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["tons"]
		}
	]);

	addElements(shuffleArray([
		{
			"func": addHills,
			"avoid": [
				g_TileClasses.bluff, 5,
				g_TileClasses.hill, 15,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 5,
				g_TileClasses.player, 20,
				g_TileClasses.spine, 5,
				g_TileClasses.valley, 2,
				g_TileClasses.water, 2
			],
			"sizes": ["tiny", "small"],
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addMountains,
			"avoid": [
				g_TileClasses.bluff, 20,
				g_TileClasses.mountain, 25,
				g_TileClasses.plateau, 20,
				g_TileClasses.player, 20,
				g_TileClasses.spine, 20,
				g_TileClasses.valley, 10,
				g_TileClasses.water, 15
			],
			"sizes": ["small"],
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addPlateaus,
			"avoid": [
				g_TileClasses.bluff, 20,
				g_TileClasses.mountain, 25,
				g_TileClasses.plateau, 20,
				g_TileClasses.player, 40,
				g_TileClasses.spine, 20,
				g_TileClasses.valley, 10,
				g_TileClasses.water, 15
			],
			"sizes": ["small"],
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addBluffs,
			"baseHeight": heightLand,
			"avoid": [
				g_TileClasses.bluff, 20,
				g_TileClasses.mountain, 25,
				g_TileClasses.plateau, 20,
				g_TileClasses.player, 40,
				g_TileClasses.spine, 20,
				g_TileClasses.valley, 10,
				g_TileClasses.water, 15
			],
			"sizes": ["normal"],
			"mixes": allMixes,
			"amounts": allAmounts
		}
	]));
	yield 60;

	addElements(shuffleArray([
		{
			"func": addMetal,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 10,
				g_TileClasses.spine, 5,
				g_TileClasses.metal, 20,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["normal", "many"]
		},
		{
			"func": addStone,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 20,
				g_TileClasses.spine, 5,
				g_TileClasses.metal, 10,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["normal", "many"]
		},
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 8,
				g_TileClasses.metal, 3,
				g_TileClasses.mountain, 5,
				g_TileClasses.plateau, 5,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 3,
				g_TileClasses.spine, 5,
				g_TileClasses.water, 2
			],
			"sizes": ["normal"],
			"mixes": ["similar"],
			"amounts": ["many"]
		}
	]));

	yield 70;

	addElements(shuffleArray([
		{
			"func": addBerries,
			"avoid": [
				g_TileClasses.berries, 30,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 5,
				g_TileClasses.metal, 10,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 10,
				g_TileClasses.spine, 2,
				g_TileClasses.water, 3
			],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addAnimals,
			"avoid": [
				g_TileClasses.animals, 20,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.metal, 2,
				g_TileClasses.mountain, 1,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 2,
				g_TileClasses.spine, 2,
				g_TileClasses.water, 3
			],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addStragglerTrees,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 7,
				g_TileClasses.metal, 2,
				g_TileClasses.mountain, 1,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 12,
				g_TileClasses.rock, 2,
				g_TileClasses.spine, 2,
				g_TileClasses.water, 5
			],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": allAmounts
		}
	]));

	yield 80;

	addElements([
		{
			"func": addLayeredPatches,
			"avoid": [
				g_TileClasses.bluff, 2,
				g_TileClasses.dirt, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 12,
				g_TileClasses.spine, 5,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		},
		{
			"func": addDecoration,
			"avoid": [
				g_TileClasses.bluff, 2,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 12,
				g_TileClasses.spine, 5,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		}
	]);

	yield 90;

	placePlayersNomad(
		g_TileClasses.player,
		avoidClasses(
			g_TileClasses.bluff, 4,
			g_TileClasses.water, 4,
			g_TileClasses.spine, 4,
			g_TileClasses.plateau, 4,
			g_TileClasses.forest, 1,
			g_TileClasses.metal, 4,
			g_TileClasses.rock, 4,
			g_TileClasses.mountain, 4,
			g_TileClasses.animals, 2));

	return g_Map;

	function addCenterLake()
	{
		createArea(
			new ChainPlacer(
				2,
				Math.floor(scaleByMapSize(2, 12)),
				Math.floor(scaleByMapSize(35, 160)),
				Infinity,
				mapCenter,
				0,
				[Math.floor(fractionToTiles(0.2))]),
			[
				new LayeredPainter([g_Terrains.shore, g_Terrains.water], [1]),
				new SmoothElevationPainter(ELEVATION_SET, heightSeaGround, 10),
				new TileClassPainter(g_TileClasses.water)
			],
			avoidClasses(g_TileClasses.player, 20));

		let fDist = 50;
		if (mapSize <= 192)
			fDist = 20;
	}

	function addHarbors()
	{
		for (const position of playerPosition)
		{
			const harborPosition =
				Vector2D.add(position, Vector2D.sub(mapCenter, position).div(2.5).round());
			createArea(
				new ClumpPlacer(1200, 0.5, 0.5, Infinity, harborPosition),
				[
					new LayeredPainter([g_Terrains.shore, g_Terrains.water], [2]),
					new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetHarbor, 3),
					new TileClassPainter(g_TileClasses.water)
				],
				avoidClasses(
					g_TileClasses.player, 15,
					g_TileClasses.hill, 1
				)
			);
		}
	}

	function addSpines()
	{
		const smallSpines = mapSize <= 192;
		const spineSize = smallSpines ? 0.02 : 0.5;
		const spineTapering = smallSpines ? -0.1 : -1.4;
		const heightOffsetSpine = smallSpines ? 20 : 35;

		const numPlayers = getNumPlayers();
		let spineTile = g_Terrains.dirt;

		if (currentBiome() == "generic/arctic")
			spineTile = g_Terrains.tier1Terrain;

		if (currentBiome() == "generic/alpine" || currentBiome() == "generic/savanna")
			spineTile = g_Terrains.tier2Terrain;

		if (currentBiome() == "generic/autumn")
			spineTile = g_Terrains.tier4Terrain;

		let split = 1;
		if (numPlayers <= 3 || mapSize >= 320 && numPlayers <= 4)
			split = 2;

		for (let i = 0; i < numPlayers * split; ++i)
		{
			const tang = startAngle + (i + 0.5) * 2 * Math.PI / (numPlayers * split);
			const start =
				Vector2D.add(mapCenter, new Vector2D(fractionToTiles(0.12), 0).rotate(-tang));
			const end = Vector2D.add(mapCenter, new Vector2D(fractionToTiles(0.4), 0).rotate(-tang));

			createArea(
				new PathPlacer(start, end, scaleByMapSize(14, spineSize), 0.6, 0.1, 0.4,
					spineTapering),
				[
					new LayeredPainter([g_Terrains.cliff, spineTile], [3]),
					new SmoothElevationPainter(ELEVATION_MODIFY, heightOffsetSpine, 3),
					new TileClassPainter(g_TileClasses.spine)
				],
				avoidClasses(g_TileClasses.player, 5)
			);
		}

		addElements([
			{
				"func": addDecoration,
				"avoid": [
					g_TileClasses.bluff, 2,
					g_TileClasses.forest, 2,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 12,
					g_TileClasses.water, 3
				],
				"stay": [g_TileClasses.spine, 5],
				"sizes": ["normal"],
				"mixes": ["normal"],
				"amounts": ["normal"]
			}
		]);

		addElements([
			{
				"func": addProps,
				"avoid": [
					g_TileClasses.forest, 2,
					g_TileClasses.player, 2,
					g_TileClasses.prop, 20,
					g_TileClasses.water, 3
				],
				"stay": [g_TileClasses.spine, 8],
				"sizes": ["normal"],
				"mixes": ["normal"],
				"amounts": ["scarce"]
			}
		]);
	}
}
