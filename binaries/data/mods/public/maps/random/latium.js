Engine.LoadLibrary("rmgen");
Engine.LoadLibrary("rmgen-common");

export function* generateMap()
{
	const tOceanDepths = "medit_sea_depths";
	const tOceanRockDeep = "medit_sea_coral_deep";
	const tOceanRockShallow = "medit_rocks_wet";
	const tOceanCoral = "medit_sea_coral_plants";
	const tBeachWet = "medit_sand_wet";
	const tBeachDry = "medit_sand";
	const tBeachGrass = "medit_rocks_grass";
	const tBeachCliff = "medit_dirt";
	const tCity = "medit_city_tile";
	const tGrassDry = ["medit_grass_field_brown", "medit_grass_field_dry", "medit_grass_field_b"];
	const tGrass = ["medit_grass_field_dry", "medit_grass_field_brown", "medit_grass_field_b"];
	const tGrassShrubs = ["medit_grass_shrubs", "medit_grass_flowers"];
	const tGrassRock = ["medit_rocks_grass"];
	const tDirt = "medit_dirt";
	const tDirtCliff = "medit_cliff_italia";
	const tGrassCliff = "medit_cliff_italia_grass";
	const tCliff = ["medit_cliff_italia", "medit_cliff_italia", "medit_cliff_italia_grass"];
	const tForestFloor = "medit_grass_wild";

	const oBeech = "gaia/tree/euro_beech";
	const oBerryBush = "gaia/fruit/berry_01";
	const oCarob = "gaia/tree/carob";
	const oCypress1 = "gaia/tree/cypress";
	const oCypress2 = "gaia/tree/cypress";
	const oLombardyPoplar = "gaia/tree/poplar_lombardy";
	const oPalm = "gaia/tree/medit_fan_palm";
	const oPine = "gaia/tree/aleppo_pine";
	const oDeer = "gaia/fauna_deer";
	const oFish = "gaia/fish/generic";
	const oSheep = "gaia/fauna_sheep";
	const oStoneLarge = "gaia/rock/mediterranean_large";
	const oStoneSmall = "gaia/rock/mediterranean_small";
	const oMetalLarge = "gaia/ore/mediterranean_large";

	const aBushMedDry = "actor|props/flora/bush_medit_me_dry.xml";
	const aBushMed = "actor|props/flora/bush_medit_me.xml";
	const aBushSmall = "actor|props/flora/bush_medit_sm.xml";
	const aBushSmallDry = "actor|props/flora/bush_medit_sm_dry.xml";
	const aGrass = "actor|props/flora/grass_soft_large_tall.xml";
	const aGrassDry = "actor|props/flora/grass_soft_dry_large_tall.xml";
	const aRockLarge = "actor|geology/stone_granite_large.xml";
	const aRockMed = "actor|geology/stone_granite_med.xml";
	const aRockSmall = "actor|geology/stone_granite_small.xml";

	const pPalmForest = [tForestFloor + TERRAIN_SEPARATOR + oPalm, tGrass];
	const pPineForest = [tForestFloor + TERRAIN_SEPARATOR + oPine, tGrass];
	const pPoplarForest = [tForestFloor + TERRAIN_SEPARATOR + oLombardyPoplar, tGrass];
	const pMainForest = [
		tForestFloor + TERRAIN_SEPARATOR + oCarob,
		tForestFloor + TERRAIN_SEPARATOR + oBeech,
		tGrass,
		tGrass
	];

	const heightSeaGround = -16;
	const heightLand = 0;
	const heightPlayer = 5;
	const heightHill = 12;

	globalThis.g_Map = new RandomMap(heightLand, tGrass);

	const numPlayers = getNumPlayers();
	const mapSize = g_Map.getSize();
	const mapCenter = g_Map.getCenter();
	const mapBounds = g_Map.getBounds();

	const clWater = g_Map.createTileClass();
	const clCliff = g_Map.createTileClass();
	const clForest = g_Map.createTileClass();
	const clMetal = g_Map.createTileClass();
	const clRock = g_Map.createTileClass();
	const clFood = g_Map.createTileClass();
	const clPlayer = g_Map.createTileClass();
	const clBaseResource = g_Map.createTileClass();

	const WATER_WIDTH = 0.1;

	g_Map.log("Creating players");
	const startAngle = randBool() ? 0 : Math.PI / 2;
	const playerPosition = playerPlacementLine(startAngle + Math.PI / 2, mapCenter,
		fractionToTiles(randFloat(0.42, 0.46)));

	function distanceToPlayers(x, z)
	{
		let r = Infinity;
		for (let i = 0; i < numPlayers; ++i)
		{
			const dx = x - tilesToFraction(playerPosition[i].x);
			const dz = z - tilesToFraction(playerPosition[i].y);
			r = Math.min(r, Math.square(dx) + Math.square(dz));
		}
		return Math.sqrt(r);
	}

	function playerNearness(x, z)
	{
		const d = fractionToTiles(distanceToPlayers(x, z));

		if (d < 13)
			return 0;

		if (d < 19)
			return (d-13)/(19-13);

		return 1;
	}

	for (const x of [mapBounds.left, mapBounds.right])
		paintRiver({
			"parallel": true,
			"start": new Vector2D(x, mapBounds.top).rotateAround(startAngle, mapCenter),
			"end": new Vector2D(x, mapBounds.bottom).rotateAround(startAngle, mapCenter),
			"width": 2 * fractionToTiles(WATER_WIDTH),
			"fadeDist": 16,
			"deviation": 0,
			"heightRiverbed": heightSeaGround,
			"heightLand": heightLand,
			"meanderShort": 0,
			"meanderLong": 0,
			"waterFunc": (position, height, z) => {
				clWater.add(position);
			}
		});
	yield 10;

	g_Map.log("Painting elevation");
	const noise0 = new Noise2D(scaleByMapSize(4, 16));
	const noise1 = new Noise2D(scaleByMapSize(8, 32));
	const noise2 = new Noise2D(scaleByMapSize(15, 60));

	const noise2a = new Noise2D(scaleByMapSize(20, 80));
	const noise2b = new Noise2D(scaleByMapSize(35, 140));

	const noise3 = new Noise2D(scaleByMapSize(4, 16));
	const noise4 = new Noise2D(scaleByMapSize(6, 24));
	const noise5 = new Noise2D(scaleByMapSize(11, 44));

	for (let ix = 0; ix <= mapSize; ix++)
		for (let iz = 0; iz <= mapSize; iz++)
		{
			const position = new Vector2D(ix, iz);

			const x = ix / (mapSize + 1.0);
			const z = iz / (mapSize + 1.0);
			const pn = playerNearness(x, z);

			const c = startAngle ? z : x;
			const distToWater = clWater.has(position) ? 0 : (0.5 - WATER_WIDTH - Math.abs(c - 0.5));
			let h = distToWater ? heightHill * (1 - Math.abs(c - 0.5) / (0.5 - WATER_WIDTH)) :
				g_Map.getHeight(position);

			// add some base noise
			let baseNoise =
				16 * noise0.get(x, z) +
				8 * noise1.get(x, z) +
				4 * noise2.get(x, z) -
				(16 + 8 + 4) / 2;
			if (baseNoise < 0)
			{
				baseNoise *= pn;
				baseNoise *= Math.max(0.1, distToWater / (0.5 - WATER_WIDTH));
			}
			const oldH = h;
			h += baseNoise;

			// add some higher-frequency noise on land
			if (oldH > 0)
				h += (0.4 * noise2a.get(x, z) + 0.2 * noise2b.get(x, z)) * Math.min(oldH / 10, 1);

			// create cliff noise
			if (h > -10)
			{
				let cliffNoise = (noise3.get(x, z) + 0.5*noise4.get(x, z)) / 1.5;
				if (h < 1)
				{
					const u = 1 - 0.3*((h-1)/-10);
					cliffNoise *= u;
				}
				cliffNoise += 0.05 * distToWater / (0.5 - WATER_WIDTH);
				if (cliffNoise > 0.6)
				{
					const u = 0.8 * (cliffNoise - 0.6);
					cliffNoise += u * noise5.get(x, z);
					cliffNoise /= (1 + u);
				}
				cliffNoise -= 0.59;
				cliffNoise *= pn;
				if (cliffNoise > 0)
					h += 19 * Math.min(cliffNoise, 0.045) / 0.045;
			}
			g_Map.setHeight(position, h);
		}
	yield 20;

	g_Map.log("Painting terrain");
	const noise6 = new Noise2D(scaleByMapSize(10, 40));
	const noise7 = new Noise2D(scaleByMapSize(20, 80));
	const noise8 = new Noise2D(scaleByMapSize(13, 52));
	const noise9 = new Noise2D(scaleByMapSize(26, 104));
	const noise10 = new Noise2D(scaleByMapSize(50, 200));

	for (let ix = 0; ix < mapSize; ix++)
		for (let iz = 0; iz < mapSize; iz++)
		{
			const position = new Vector2D(ix, iz);
			const x = ix / (mapSize + 1.0);
			const z = iz / (mapSize + 1.0);
			const pn = playerNearness(x, z);

			// Compute height difference
			let minH = +Infinity;
			let maxH = -Infinity;
			for (const vertex of g_TileVertices)
			{
				const height = g_Map.getHeight(Vector2D.add(position, vertex));
				minH = Math.min(minH, height);
				maxH = Math.max(maxH, height);
			}
			const diffH = maxH - minH;

			// figure out if we're at the top of a cliff using min
			// adjacent height
			let minAdjHeight = minH;
			if (maxH > 15)
			{
				const maxNx = Math.min(ix + 2, mapSize);
				const maxNz = Math.min(iz + 2, mapSize);
				for (let nx = Math.max(ix - 1, 0); nx <= maxNx; ++nx)
					for (let nz = Math.max(iz - 1, 0); nz <= maxNz; ++nz)
						minAdjHeight =
							Math.min(minAdjHeight, g_Map.getHeight(new Vector2D(nx, nz)));
			}

			// choose a terrain based on elevation
			let t = tGrass;

			// water
			if (maxH < -12)
				t = tOceanDepths;
			else if (maxH < -8.8)
				t = tOceanRockDeep;
			else if (maxH < -4.7)
				t = tOceanCoral;
			else if (maxH < -2.8)
				t = tOceanRockShallow;
			else if (maxH < 0.9 && minH < 0.35)
				t = tBeachWet;
			else if (maxH < 1.5 && minH < 0.9)
				t = tBeachDry;
			else if (maxH < 2.3 && minH < 1.3)
				t = tBeachGrass;

			if (minH < 0)
				clWater.add(position);

			// cliffs
			if (diffH > 2.9 && minH > -7)
			{
				t = tCliff;
				clCliff.add(position);
			}
			else if (diffH > 2.5 && minH > -5 || maxH - minAdjHeight > 2.9 && minH > 0)
			{
				if (minH < -1)
					t = tCliff;
				else if (minH < 0.5)
					t = tBeachCliff;
				else
					t = [tDirtCliff, tGrassCliff, tGrassCliff, tGrassRock, tCliff];

				clCliff.add(position);
			}

			// Don't place resources onto potentially impassable mountains
			if (minH >= 20)
				clCliff.add(position);

			// forests
			if (g_Map.getHeight(position) < 11 && diffH < 2 && minH > 1)
			{
				const forestNoise = (noise6.get(x, z) + 0.5 * noise7.get(x, z)) / 1.5 * pn - 0.59;

				// Thin out trees a bit
				if (forestNoise > 0 && randBool())
				{
					if (minH < 11 && minH >= 4)
					{
						const typeNoise = noise10.get(x, z);

						if (typeNoise < 0.43 && forestNoise < 0.05)
							t = pPoplarForest;
						else if (typeNoise < 0.63)
							t = pMainForest;
						else
							t = pPineForest;

						clForest.add(position);
					}
					else if (minH < 4)
					{
						t = pPalmForest;
						clForest.add(position);
					}
				}
			}

			// grass variations
			if (t == tGrass)
			{
				const grassNoise = (noise8.get(x, z) + 0.6 * noise9.get(x, z)) / 1.6;
				if (grassNoise < 0.3)
					t = (diffH > 1.2) ? tDirtCliff : tDirt;
				else if (grassNoise < 0.34)
				{
					t = (diffH > 1.2) ? tGrassCliff : tGrassDry;
					if (diffH < 0.5 && randBool(0.02))
						g_Map.placeEntityAnywhere(aGrassDry, 0, randomPositionOnTile(position),
							randomAngle());
				}
				else if (grassNoise > 0.61)
				{
					t = (diffH > 1.2 ? tGrassRock : tGrassShrubs);
				}
				else if (diffH < 0.5 && randBool(0.02))
					g_Map.placeEntityAnywhere(aGrass, 0, randomPositionOnTile(position),
						randomAngle());
			}

			createTerrain(t).place(position);
		}

	yield 30;

	placePlayerBases({
		"PlayerPlacement": [primeSortAllPlayers(), playerPosition],
		"PlayerTileClass": clPlayer,
		"BaseResourceClass": clBaseResource,
		"baseResourceConstraint": avoidClasses(clCliff, 4),
		"CityPatch": {
			"radius": 11,
			"outerTerrain": tGrass,
			"innerTerrain": tCity,
			"width": 4,
			"painters": [
				new SmoothElevationPainter(ELEVATION_SET, heightPlayer, 2)
			]
		},
		"StartingAnimal": {
		},
		"Berries": {
			"template": oBerryBush,
			"distance": 9
		},
		"Mines": {
			"types": [
				{ "template": oMetalLarge },
				{ "template": oStoneLarge }
			]
		},
		"Trees": {
			"template": oPalm,
			"count": 5,
			"minDist": 10,
			"maxDist": 11
		}
		// No decoratives
	});
	yield 40;

	g_Map.log("Creating bushes");
	let group = new SimpleGroup(
		[
			new SimpleObject(aBushSmall, 0, 2, 0, 2),
			new SimpleObject(aBushSmallDry, 0, 2, 0, 2),
			new SimpleObject(aBushMed, 0, 1, 0, 2),
			new SimpleObject(aBushMedDry, 0, 1, 0, 2)
		]
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 4, clCliff, 2),
		scaleByMapSize(9, 146), 50
	);
	yield 45;

	g_Map.log("Creating rocks");
	group = new SimpleGroup(
		[
			new SimpleObject(aRockSmall, 0, 3, 0, 2),
			new SimpleObject(aRockMed, 0, 2, 0, 2),
			new SimpleObject(aRockLarge, 0, 1, 0, 2)
		]
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 2, clCliff, 1),
		scaleByMapSize(9, 146), 50
	);
	yield 50;

	g_Map.log("Creating large stone mines");
	group = new SimpleGroup(
		[
			new SimpleObject(oStoneSmall, 0, 2, 0, 4, 0, 2 * Math.PI, 1),
			new SimpleObject(oStoneLarge, 1, 1, 0, 4, 0, 2 * Math.PI, 4)
		], true, clRock);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 1, clForest, 4, clPlayer, 40, clRock, 60, clMetal, 10, clCliff, 3),
		scaleByMapSize(4, 16), 100
	);

	g_Map.log("Creating small stone mines");
	group = new SimpleGroup([new SimpleObject(oStoneSmall, 2, 5, 1, 3)], true, clRock);
	createObjectGroups(group, 0,
		avoidClasses(clForest, 4, clWater, 1, clPlayer, 40, clRock, 30, clMetal, 10, clCliff, 3),
		scaleByMapSize(4, 16), 100
	);
	g_Map.log("Creating metal mines");
	group = new SimpleGroup([new SimpleObject(oMetalLarge, 1, 1, 0, 2)], true, clMetal);
	createObjectGroups(group, 0,
		avoidClasses(clForest, 4, clWater, 1, clPlayer, 40, clMetal, 50, clCliff, 3),
		scaleByMapSize(4, 16), 100
	);
	yield 60;

	createStragglerTrees(
		[oCarob, oBeech, oLombardyPoplar, oLombardyPoplar, oPine],
		avoidClasses(clWater, 5, clCliff, 4, clForest, 2, clPlayer, 15, clMetal, 6, clRock, 6),
		clForest,
		scaleByMapSize(10, 190));

	yield 70;

	g_Map.log("Creating straggler cypresses");
	group = new SimpleGroup(
		[new SimpleObject(oCypress2, 1, 3, 0, 3), new SimpleObject(oCypress1, 0, 2, 0, 2)],
		true
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(clWater, 5, clCliff, 4, clForest, 2, clPlayer, 15, clMetal, 6, clRock, 6),
		scaleByMapSize(5, 75), 50
	);
	yield 80;

	g_Map.log("Creating sheep");
	group = new SimpleGroup([new SimpleObject(oSheep, 2, 4, 0, 2)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(
			clWater, 5,
			clForest, 2,
			clCliff, 1,
			clPlayer, 20,
			clMetal, 6,
			clRock, 6,
			clFood, 8),
		3 * numPlayers, 50
	);
	yield 85;

	g_Map.log("Creating fish");
	createObjectGroups(
		new SimpleGroup([new SimpleObject(oFish, 1, 1, 0, 1)], true, clFood),
		0,
		[
			avoidClasses(clFood, 10),
			stayClasses(clWater, 4),
			new HeightConstraint(-Infinity, heightLand)
		],
		scaleByMapSize(45, 65));

	yield 90;

	g_Map.log("Creating deer");
	group = new SimpleGroup(
		[new SimpleObject(oDeer, 5, 7, 0, 4)],
		true, clFood
	);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(
			clWater, 5,
			clForest, 2,
			clCliff, 1,
			clPlayer, 20,
			clMetal, 6,
			clRock, 6,
			clFood, 8),
		3 * numPlayers, 50
	);
	yield 95;

	g_Map.log("Creating berry bushes");
	group = new SimpleGroup([new SimpleObject(oBerryBush, 5, 7, 0, 3)], true, clFood);
	createObjectGroupsDeprecated(group, 0,
		avoidClasses(
			clWater, 5,
			clForest, 2,
			clCliff, 1,
			clPlayer, 20,
			clMetal, 6,
			clRock, 6,
			clFood, 8),
		1.5 * numPlayers, 100
	);

	placePlayersNomad(clPlayer,
		avoidClasses(clWater, 4, clCliff, 2, clForest, 1, clMetal, 4, clRock, 4, clFood, 2));

	setSkySet("sunny");
	setWaterColor(0.024, 0.262, 0.224);
	setWaterTint(0.133, 0.325, 0.255);
	setWaterWaviness(2.5);
	setWaterType("ocean");
	setWaterMurkiness(0.8);

	return g_Map;
}
