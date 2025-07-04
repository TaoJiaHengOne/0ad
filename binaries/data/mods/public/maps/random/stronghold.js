import { addAnimals, addBerries, addBluffs, addDecoration, addForests, addHills, addLayeredPatches,
	addMetal, addMountains, addPlateaus, addStone, addStragglerTrees, addValleys, createBluffsPassages,
	markPlayerAvoidanceArea } from "maps/random/rmgen2/gaia.js";
import { addElements, allAmounts, allMixes, allSizes, createBases, initTileClasses } from
	"maps/random/rmgen2/setup.js";

Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	const heightLand = 30;

	globalThis.g_Map = new RandomMap(heightLand, g_Terrains.mainTerrain);

	initTileClasses();

	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.land));

	yield 20;

	const [playerIDs, playerPosition] =
		createBases(
			...playerPlacementByPattern(
				"stronghold",
				fractionToTiles(randFloat(0.2, 0.35)),
				fractionToTiles(randFloat(0.05, 0.1)),
				randomAngle(),
				undefined),
			false);
	markPlayerAvoidanceArea(playerPosition, defaultPlayerBaseRadius());

	yield 30;

	addElements(shuffleArray([
		{
			"func": addBluffs,
			"baseHeight": heightLand,
			"avoid": [
				g_TileClasses.bluff, 20,
				g_TileClasses.hill, 5,
				g_TileClasses.mountain, 20,
				g_TileClasses.plateau, 20,
				g_TileClasses.player, 30,
				g_TileClasses.valley, 5,
				g_TileClasses.water, 7
			],
			"sizes": ["big", "huge"],
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addHills,
			"avoid": [
				g_TileClasses.bluff, 5,
				g_TileClasses.hill, 15,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 2,
				g_TileClasses.player, 20,
				g_TileClasses.valley, 2,
				g_TileClasses.water, 2
			],
			"sizes": ["normal", "big"],
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
				g_TileClasses.valley, 10,
				g_TileClasses.water, 15
			],
			"sizes": ["big", "huge"],
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addPlateaus,
			"avoid": [
				g_TileClasses.bluff, 20,
				g_TileClasses.mountain, 25,
				g_TileClasses.plateau, 25,
				g_TileClasses.player, 40,
				g_TileClasses.valley, 10,
				g_TileClasses.water, 15
			],
			"sizes": ["big", "huge"],
			"mixes": allMixes,
			"amounts": allAmounts
		},
		{
			"func": addValleys,
			"baseHeight": heightLand,
			"avoid": [
				g_TileClasses.bluff, 5,
				g_TileClasses.hill, 5,
				g_TileClasses.mountain, 25,
				g_TileClasses.plateau, 10,
				g_TileClasses.player, 40,
				g_TileClasses.valley, 15,
				g_TileClasses.water, 10
			],
			"sizes": ["normal", "big"],
			"mixes": allMixes,
			"amounts": allAmounts
		}
	]));

	if (!isNomad())
		createBluffsPassages(playerPosition);

	yield 60;

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
				g_TileClasses.valley, 5,
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
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["normal"],
			"amounts": ["normal"]
		}
	]);
	yield 70;

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
				g_TileClasses.metal, 20,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": allAmounts
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
				g_TileClasses.metal, 10,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": allAmounts
		},
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.berries,
				5, g_TileClasses.bluff,
				5, g_TileClasses.forest,
				18, g_TileClasses.metal, 3,
				g_TileClasses.mountain, 5,
				g_TileClasses.plateau, 5,
				g_TileClasses.player, 20,
				g_TileClasses.rock, 3,
				g_TileClasses.water, 2
			],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["few", "normal", "many", "tons"]
		}
	]));
	yield 80;

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
	yield 90;

	placePlayersNomad(
		g_TileClasses.player,
		avoidClasses(
			g_TileClasses.bluff, 4,
			g_TileClasses.plateau, 4,
			g_TileClasses.forest, 1,
			g_TileClasses.metal, 4,
			g_TileClasses.rock, 4,
			g_TileClasses.mountain, 4,
			g_TileClasses.animals, 2));

	return g_Map;
}
