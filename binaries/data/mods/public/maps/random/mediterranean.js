/**
 * Heightmap image source:
 * Imagery by Jesse Allen, NASA's Earth Observatory,
 * using data from the General Bathymetric Chart of the Oceans (GEBCO)
 * produced by the British Oceanographic Data Centre.
 * https://visibleearth.nasa.gov/view.php?id=73934
 *
 * Licensing: Public Domain, https://visibleearth.nasa.gov/useterms.php
 *
 * The heightmap image is reproduced using:
 * wget https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73934/gebco_08_rev_elev_{A,B,C,D}{1..2}_grey_geo.tif
 * gdal_merge.py -o world.tif gebco_08_rev_elev_{A,B,C,D}{1..2}_grey_geo.tif
 * lat=46; lon=14; width=58;
 * gdal_translate -projwin $((lon-width/2)) $((lat+width/2)) $((lon+width/2)) $((lat-width/2)) world.tif mediterranean.tif
 * convert mediterranean.tif -resize 512 -contrast-stretch 0 mediterranean.png
 * No further changes should be applied to the image to keep it easily interchangeable.
 */

import { addAnimals, addBerries, addDecoration, addFish, addForests, addLayeredPatches, addMetal,
	addSmallMetal, addStone, addStragglerTrees } from "maps/random/rmgen2/gaia.js";
import { addElements, createBase, initTileClasses } from "maps/random/rmgen2/setup.js";

Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");
Engine.LoadLibrary("rmbiome");

export function* generateMap(mapSettings)
{
	TILE_CENTERED_HEIGHT_MAP = true;

	const tWater = "medit_sand_wet";
	const tSnowedRocks = ["alpine_cliff_b", "alpine_cliff_snow"];
	setBiome("generic/aegean");

	const heightScale = num => num * mapSettings.Size / 320;

	const heightSeaGround = heightScale(-6);
	const heightWaterLevel = heightScale(0);
	const heightShoreline = heightScale(0.5);
	const heightSnow = heightScale(10);

	globalThis.g_Map = new RandomMap(heightWaterLevel, g_Terrains.mainTerrain);
	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();
	const mapBounds = g_Map.getBounds();

	g_Map.LoadHeightmapImage("mediterranean.png", 0, 40);
	yield 15;

	initTileClasses([
		"autumn",
		"desert",
		"medit",
		"polar",
		"steppe",
		"temp",
		"shoreline",
		"africa",
		"northern_europe",
		"southern_europe",
		"western_europe",
		"eastern_europe"
	]);

	const northernTopLeft = new Vector2D(fractionToTiles(0.3), fractionToTiles(0.7));
	const westernTopLeft = new Vector2D(fractionToTiles(0.7), fractionToTiles(0.47));
	const africaTop = fractionToTiles(0.33);

	const climateZones = [
		{
			"tileClass": g_TileClasses.northern_europe,
			"position1": new Vector2D(northernTopLeft.x, mapBounds.top),
			"position2": new Vector2D(mapBounds.right, northernTopLeft.y),
			"biome": "generic/arctic",
			"constraint": new NullConstraint()
		},
		{
			"tileClass": g_TileClasses.western_europe,
			"position1": new Vector2D(mapBounds.left, mapBounds.top),
			"position2": westernTopLeft,
			"biome": "generic/temperate",
			"constraint": avoidClasses(g_TileClasses.northern_europe, 0)
		},
		{
			"tileClass": g_TileClasses.eastern_europe,
			"position1": new Vector2D(westernTopLeft.x, mapBounds.top),
			"position2": new Vector2D(mapBounds.right, westernTopLeft.y),
			"biome": "generic/autumn",
			"constraint": avoidClasses(g_TileClasses.northern_europe, 0)
		},
		{
			"tileClass": g_TileClasses.southern_europe,
			"position1": new Vector2D(mapBounds.left, africaTop),
			"position2": new Vector2D(mapBounds.right, westernTopLeft.y),
			"biome": "generic/aegean",
			"constraint": new NullConstraint()
		},
		{
			"tileClass": g_TileClasses.africa,
			"position1": new Vector2D(mapBounds.left, africaTop),
			"position2": new Vector2D(mapBounds.right, mapBounds.bottom),
			"biome": "generic/sahara",
			"constraint": new NullConstraint()
		}
	];

	g_Map.log("Lowering sea ground");
	createArea(
		new MapBoundsPlacer(),
		new SmoothElevationPainter(ELEVATION_SET, heightSeaGround, 2),
		new HeightConstraint(-Infinity, heightWaterLevel));
	yield 20;

	g_Map.log("Smoothing heightmap");
	createArea(
		new MapBoundsPlacer(),
		new SmoothingPainter(1, scaleByMapSize(0.3, 0.8), 1));
	yield 25;

	g_Map.log("Marking water");
	createArea(
		new MapBoundsPlacer(),
		new TileClassPainter(g_TileClasses.water),
		new HeightConstraint(-Infinity, heightWaterLevel));
	yield 30;

	g_Map.log("Marking land");
	createArea(
		new DiskPlacer(fractionToTiles(0.5), mapCenter),
		new TileClassPainter(g_TileClasses.land),
		avoidClasses(g_TileClasses.water, 0));
	yield 35;

	g_Map.log("Marking climate zones");
	for (const zone of climateZones)
	{
		setBiome(zone.biome);
		createArea(
			new RectPlacer(zone.position1, zone.position2, Infinity),
			new TileClassPainter(zone.tileClass),
			zone.constraint);

		createArea(
			new RectPlacer(zone.position1, zone.position2, Infinity),
			new TerrainPainter(g_Terrains.mainTerrain),
			[
				new HeightConstraint(heightWaterLevel, Infinity),
				zone.constraint
			]);
	}
	yield 40;

	g_Map.log("Fuzzing biome borders");
	for (const zone of climateZones)
	{
		setBiome(zone.biome);

		createLayeredPatches(
			[scaleByMapSize(3, 6), scaleByMapSize(5, 10), scaleByMapSize(8, 21)],
			[
				[g_Terrains.mainTerrain, g_Terrains.tier1Terrain],
				[g_Terrains.tier1Terrain, g_Terrains.tier2Terrain],
				[g_Terrains.tier2Terrain, g_Terrains.tier3Terrain]
			],
			[1, 1],
			[
				avoidClasses(
					g_TileClasses.forest, 2,
					g_TileClasses.water, 2,
					g_TileClasses.mountain, 2,
					g_TileClasses.dirt, 5,
					g_TileClasses.player, 8),
				borderClasses(zone.tileClass, 3, 3),
			],
			scaleByMapSize(20, 60),
			g_TileClasses.dirt);
	}
	yield 45;

	if (!isNomad())
	{
		g_Map.log("Finding player positions");

		const [playerIDs, playerPosition] = playerPlacementRandom(
			sortAllPlayers(),
			[
				avoidClasses(g_TileClasses.mountain, 5),
				stayClasses(g_TileClasses.land, scaleByMapSize(8, 25))
			]);

		g_Map.log("Flatten the initial CC area and placing playerbases");
		for (let i = 0; i < getNumPlayers(); ++i)
		{
			g_Map.logger.printDuration();
			setBiome(climateZones.find(zone => zone.tileClass.has(playerPosition[i])).biome);

			createArea(
				new ClumpPlacer(diskArea(defaultPlayerBaseRadius() * 0.8), 0.95, 0.6, Infinity,
					playerPosition[i]),
				new SmoothElevationPainter(ELEVATION_SET, g_Map.getHeight(playerPosition[i]), 6));

			createBase(playerIDs[i], playerPosition[i], mapSize >= 384);
		}
	}
	yield 50;

	for (const zone of climateZones)
	{
		setBiome(zone.biome);

		g_Map.log("Painting shoreline");
		createArea(
			new MapBoundsPlacer(),
			[
				new TerrainPainter(g_Terrains.shore),
				new TileClassPainter(g_TileClasses.shoreline)
			],
			[
				stayClasses(zone.tileClass, 0),
				new HeightConstraint(-Infinity, heightShoreline)
			]);

		g_Map.log("Painting cliffs");
		createArea(
			new MapBoundsPlacer(),
			[
				new TerrainPainter(g_Terrains.cliff),
				new TileClassPainter(g_TileClasses.mountain),
			],
			[
				stayClasses(zone.tileClass, 0),
				avoidClasses(g_TileClasses.water, 2),
				new SlopeConstraint(2, Infinity)
			]);

		g_Map.log("Placing resources");
		addElements([
			{
				"func": addMetal,
				"avoid": [
					g_TileClasses.berries, 5,
					g_TileClasses.forest, 3,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 30,
					g_TileClasses.rock, 10,
					g_TileClasses.metal, 25,
					g_TileClasses.water, 4
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["same"],
				"amounts": ["many"]
			},
			{
				"func": addStone,
				"avoid": [
					g_TileClasses.berries, 5,
					g_TileClasses.forest, 3,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 30,
					g_TileClasses.rock, 10,
					g_TileClasses.metal, 25,
					g_TileClasses.water, 4
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["same"],
				"amounts": ["many"]
			},
			{
				"func": addForests,
				"avoid": [
					g_TileClasses.berries, 3,
					g_TileClasses.forest, 15,
					g_TileClasses.metal, 3,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 12,
					g_TileClasses.rock, 2,
					g_TileClasses.water, 2
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["normal"],
				"amounts": ["normal"]
			},
			{
				"func": addSmallMetal,
				"avoid": [
					g_TileClasses.berries, 5,
					g_TileClasses.forest, 3,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 30,
					g_TileClasses.rock, 10,
					g_TileClasses.metal, 15,
					g_TileClasses.water, 4
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["same"],
				"amounts": ["few", "normal", "many"]
			},
			{
				"func": addBerries,
				"avoid": [
					g_TileClasses.berries, 30,
					g_TileClasses.forest, 2,
					g_TileClasses.metal, 4,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 20,
					g_TileClasses.rock, 4,
					g_TileClasses.water, 2
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["normal"],
				"amounts": ["many"]
			},
			{
				"func": addAnimals,
				"avoid": [
					g_TileClasses.animals, 10,
					g_TileClasses.forest, 1,
					g_TileClasses.metal, 2,
					g_TileClasses.mountain, 1,
					g_TileClasses.player, 15,
					g_TileClasses.rock, 2,
					g_TileClasses.water, 3
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["normal"],
				"amounts": ["many"]
			},
			{
				"func": addAnimals,
				"avoid": [
					g_TileClasses.animals, 10,
					g_TileClasses.forest, 1,
					g_TileClasses.metal, 2,
					g_TileClasses.mountain, 1,
					g_TileClasses.player, 15,
					g_TileClasses.rock, 2,
					g_TileClasses.water, 1
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["small"],
				"mixes": ["normal"],
				"amounts": ["tons"]
			},
			{
				"func": addStragglerTrees,
				"avoid": [
					g_TileClasses.berries, 5,
					g_TileClasses.forest, 5,
					g_TileClasses.metal, 2,
					g_TileClasses.mountain, 1,
					g_TileClasses.player, 12,
					g_TileClasses.rock, 2,
					g_TileClasses.water, 3
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["normal"],
				"amounts": ["some"]
			},
			{
				"func": addLayeredPatches,
				"avoid": [
					g_TileClasses.dirt, 5,
					g_TileClasses.forest, 2,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 12,
					g_TileClasses.water, 3
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["normal"],
				"mixes": ["normal"],
				"amounts": ["tons"]
			},
			{
				"func": addDecoration,
				"avoid": [
					g_TileClasses.forest, 2,
					g_TileClasses.mountain, 2,
					g_TileClasses.player, 12,
					g_TileClasses.water, 4
				],
				"stay": [zone.tileClass, 0],
				"sizes": ["small"],
				"mixes": ["same"],
				"amounts": ["normal"]
			}
		]);
	}
	yield 60;

	g_Map.log("Painting water");
	createArea(
		new MapBoundsPlacer(),
		new TerrainPainter(tWater),
		new HeightConstraint(-Infinity, heightWaterLevel));

	g_Map.log("Painting snow on mountains");
	createArea(
		new MapBoundsPlacer(),
		new TerrainPainter(tSnowedRocks),
		[
			new HeightConstraint(heightSnow, Infinity),
			avoidClasses(
				g_TileClasses.africa, 0,
				g_TileClasses.southern_europe, 0,
				g_TileClasses.player, 6)
		]);

	yield 70;

	g_Map.log("Placing fish");
	g_Gaia.fish = "gaia/fish/generic";
	addElements([
		{
			"func": addFish,
			"avoid": [
				g_TileClasses.fish, 10,
			],
			"stay": [g_TileClasses.water, 4],
			"sizes": ["normal"],
			"mixes": ["similar"],
			"amounts": ["many"]
		}
	]);
	yield 85;

	g_Map.log("Placing whale");
	g_Gaia.fish = "gaia/fauna_whale_fin";
	addElements([
		{
			"func": addFish,
			"avoid": [
				g_TileClasses.fish, 2,
				g_TileClasses.desert, 50,
				g_TileClasses.steppe, 50
			],
			"stay": [g_TileClasses.water, 7],
			"sizes": ["small"],
			"mixes": ["same"],
			"amounts": ["scarce"]
		}
	]);
	yield 95;

	placePlayersNomad(
		g_Map.createTileClass(),
		[
			stayClasses(g_TileClasses.land, 5),
			avoidClasses(
				g_TileClasses.forest, 2,
				g_TileClasses.rock, 4,
				g_TileClasses.metal, 4,
				g_TileClasses.berries, 2,
				g_TileClasses.animals, 2,
				g_TileClasses.mountain, 2)
		]);

	setWindAngle(-0.589049);
	setWaterTint(0.556863, 0.615686, 0.643137);
	setWaterColor(0.494118, 0.639216, 0.713726);
	setWaterWaviness(8);
	setWaterMurkiness(0.87);
	setWaterType("ocean");

	setAmbientColor(0.72, 0.72, 0.82);

	setSunColor(0.733, 0.746, 0.574);
	setSunRotation(Math.PI * 0.95);
	setSunElevation(Math.PI / 6);

	setSkySet("cumulus");
	setFogFactor(0);
	setFogThickness(0);
	setFogColor(0.69, 0.616, 0.541);

	setPPEffect("hdr");
	setPPContrast(0.67);
	setPPSaturation(0.42);
	setPPBloom(0.23);

	return g_Map;
}
