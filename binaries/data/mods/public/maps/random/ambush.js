import { addAnimals, addBerries, addBluffs, addDecoration, addForests, addHills, addLayeredPatches,
	addMetal, addStone, addStragglerTrees, createBluffsPassages, markPlayerAvoidanceArea } from
	"maps/random/rmgen2/gaia.js";
import { addElements, createBases, initTileClasses, playerbaseTypes } from "maps/random/rmgen2/setup.js";


Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	const heightLand = 2;

	globalThis.g_Map = new RandomMap(heightLand, g_Terrains.mainTerrain);
	const mapCenter = g_Map.getCenter();
	const mapSize = g_Map.getSize();

	initTileClasses(["bluffsPassage", "nomadArea"]);
	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.land));

	yield 10;

	const [playerIDs, playerPosition] =
		createBases(
			...playerPlacementByPattern(
				mapSettings.PlayerPlacement,
				fractionToTiles(randFloat(0.25, 0.35)),
				fractionToTiles(randFloat(0.08, 0.1)),
				randomAngle(),
				undefined),
			playerbaseTypes[mapSettings.PlayerPlacement].walls);

	if (!isNomad())
		markPlayerAvoidanceArea(playerPosition, defaultPlayerBaseRadius());

	yield 20;

	addElements([
		{
			"func": addBluffs,
			"baseHeight": heightLand,
			"avoid": [g_TileClasses.bluffIgnore, 0],
			"sizes": ["normal", "big", "huge"],
			"mixes": ["same"],
			"amounts": ["many"]
		},
		{
			"func": addHills,
			"avoid": [
				g_TileClasses.bluff, 5,
				g_TileClasses.hill, 15,
				g_TileClasses.player, 20
			],
			"sizes": ["normal", "big"],
			"mixes": ["normal"],
			"amounts": ["tons"]
		}
	]);
	yield 30;

	if (!isNomad())
		createBluffsPassages(playerPosition);

	addElements([
		{
			"func": addLayeredPatches,
			"avoid": [
				g_TileClasses.bluff, 2,
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.dirt, 5,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12,
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
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.forest, 2,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 12,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		}
	]);
	yield 50;

	addElements(shuffleArray([
		{
			"func": addMetal,
			"avoid": [
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 10,
				g_TileClasses.metal, 20,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 5],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["tons"]
		},
		{
			"func": addStone,
			"avoid": [
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.berries, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 20,
				g_TileClasses.metal, 10,
				g_TileClasses.water, 3
			],
			"stay": [g_TileClasses.bluff, 5],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["tons"]
		},
		// Forests on bluffs
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.forest, 6,
				g_TileClasses.metal, 3,
				g_TileClasses.mountain, 5,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 3,
				g_TileClasses.water, 2
			],
			"stay": [g_TileClasses.bluff, 5],
			"sizes": ["big"],
			"mixes": ["normal"],
			"amounts": ["tons"]
		},
		// Forests on mainland
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.bluff, 10,
				g_TileClasses.forest, 10,
				g_TileClasses.metal, 3,
				g_TileClasses.mountain, 5,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 3,
				g_TileClasses.water, 2
			],
			"sizes": ["small"],
			"mixes": ["same"],
			"amounts": ["normal"]
		}
	]));
	yield 70;

	addElements(shuffleArray([
		{
			"func": addBerries,
			"avoid": [
				g_TileClasses.bluff, 5,
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.forest, 5,
				g_TileClasses.metal, 10,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 10,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": ["few"]
		},
		{
			"func": addAnimals,
			"avoid": [
				g_TileClasses.bluff, 5,
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.forest, 2,
				g_TileClasses.metal, 2,
				g_TileClasses.mountain, 1,
				g_TileClasses.player, 12,
				g_TileClasses.rock, 2,
				g_TileClasses.water, 3
			],
			"sizes": ["small"],
			"mixes": ["similar"],
			"amounts": ["few"]
		},
		{
			"func": addStragglerTrees,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.bluffsPassage, 4,
				g_TileClasses.forest, 7,
				g_TileClasses.metal, 2,
				g_TileClasses.mountain, 1,
				g_TileClasses.player, 12,
				g_TileClasses.rock, 2,
				g_TileClasses.water, 5
			],
			"sizes": ["tiny"],
			"mixes": ["same"],
			"amounts": ["many"]
		}
	]));
	yield 90;

	if (isNomad())
	{
		g_Map.log("Preventing units to be spawned at the map border");
		createArea(
			new DiskPlacer(mapSize / 2 - scaleByMapSize(15, 35), mapCenter),
			new TileClassPainter(g_TileClasses.nomadArea));

		placePlayersNomad(
			g_TileClasses.player,
			[
				stayClasses(g_TileClasses.nomadArea, 0),
				avoidClasses(
					g_TileClasses.bluff, 2,
					g_TileClasses.water, 4,
					g_TileClasses.forest, 1,
					g_TileClasses.metal, 4,
					g_TileClasses.rock, 4,
					g_TileClasses.mountain, 4,
					g_TileClasses.animals, 2)
			]);
	}

	return g_Map;
}
