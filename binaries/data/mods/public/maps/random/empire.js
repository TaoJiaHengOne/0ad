import { addAnimals, addBerries, addDecoration, addForests, addHills, addMetal, addMountains,
	addLayeredPatches, addPlateaus, addStone, addStragglerTrees } from "maps/random/rmgen2/gaia.js";
import { addElements, allAmounts, allMixes, allSizes, createBases, initTileClasses } from
	"maps/random/rmgen2/setup.js";

Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	setBiome(mapSettings.Biome);

	globalThis.g_Map = new RandomMap(2, g_Terrains.mainTerrain);

	initTileClasses();

	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.land));

	yield 10;

	const teamsArray = getTeamsArray();
	const startAngle = randomAngle();
	createBases(
		...playerPlacementByPattern(
			"stronghold",
			fractionToTiles(0.37),
			fractionToTiles(0.04),
			startAngle,
			undefined),
		false);
	yield 20;

	// Change the starting angle and add the players again
	let rotation = Math.PI;

	if (teamsArray.length == 2)
		rotation = Math.PI / 2;

	if (teamsArray.length == 4)
		rotation = 5/4 * Math.PI;

	createBases(
		...playerPlacementByPattern(
			"stronghold",
			fractionToTiles(0.15),
			fractionToTiles(0.04),
			startAngle + rotation,
			undefined),
		false);
	yield 40;

	addElements(shuffleArray([
		{
			"func": addHills,
			"avoid": [
				g_TileClasses.bluff, 5,
				g_TileClasses.hill, 15,
				g_TileClasses.mountain, 2,
				g_TileClasses.plateau, 5,
				g_TileClasses.player, 20,
				g_TileClasses.valley, 2,
				g_TileClasses.water, 2
			],
			"sizes": allSizes,
			"mixes": allMixes,
			"amounts": ["tons"]
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
			"sizes": ["huge"],
			"mixes": ["same", "similar"],
			"amounts": ["tons"]
		},
		{
			"func": addPlateaus,
			"avoid": [
				g_TileClasses.bluff, 20,
				g_TileClasses.mountain, 25,
				g_TileClasses.plateau, 20,
				g_TileClasses.player, 40,
				g_TileClasses.valley, 10,
				g_TileClasses.water, 15
			],
			"sizes": ["huge"],
			"mixes": ["same", "similar"],
			"amounts": ["tons"]
		}
	]));
	yield 50;

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
	yield 60;

	addElements(shuffleArray([
		{
			"func": addMetal,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 10,
				g_TileClasses.metal, 20,
				g_TileClasses.plateau, 2,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": allAmounts
		},
		{
			"func": addStone,
			"avoid": [g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 3,
				g_TileClasses.mountain, 2,
				g_TileClasses.player, 30,
				g_TileClasses.rock, 20,
				g_TileClasses.metal, 10,
				g_TileClasses.plateau, 2,
				g_TileClasses.water, 3
			],
			"sizes": ["normal"],
			"mixes": ["same"],
			"amounts": allAmounts
		},
		{
			"func": addForests,
			"avoid": [
				g_TileClasses.berries, 5,
				g_TileClasses.bluff, 5,
				g_TileClasses.forest, 18,
				g_TileClasses.metal, 3,
				g_TileClasses.mountain, 5,
				g_TileClasses.plateau, 2,
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
			g_TileClasses.plateau, 4,
			g_TileClasses.forest, 1,
			g_TileClasses.metal, 4,
			g_TileClasses.rock, 4,
			g_TileClasses.mountain, 4,
			g_TileClasses.animals, 2));

	return g_Map;
}
