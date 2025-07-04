var g_BiomeID;

var g_Terrains = {};
var g_Gaia = {};
var g_Decoratives = {};
var g_ResourceCounts = {};
var g_Heights = {};

function currentBiome()
{
	return g_BiomeID;
}

function setBiome(biomeID)
{
	RandomMapLogger.prototype.printDirectly("Setting biome " + biomeID + ".\n");

	loadBiomeFile("defaultbiome");

	setSkySet(pickRandom(["default", "cirrus", "cumulus", "sunny"]));
	setSunRotation(randomAngle());
	setSunElevation(Math.PI * randFloat(1/6, 1/3));

	g_BiomeID = biomeID;

	loadBiomeFile(biomeID);

	Engine.LoadLibrary("rmbiome/" + dirname(biomeID));
	const setupBiomeFunc = global["setupBiome_" + basename(biomeID)];
	if (setupBiomeFunc)
		setupBiomeFunc();
}

/**
 * Copies JSON contents to defined global variables.
 */
function loadBiomeFile(file)
{
	const path = "maps/random/rmbiome/" + file + ".json";

	if (!Engine.FileExists(path))
	{
		error("Could not load biome file '" + file + "'");
		return;
	}

	const biome = Engine.ReadJSONFile(path);

	const copyProperties = (from, to) => {
		for (const prop in from)
		{
			if (from[prop] !== null && typeof from[prop] == "object" && !Array.isArray(from[prop]))
			{
				if (!to[prop])
					to[prop] = {};

				copyProperties(from[prop], to[prop]);
			}
			else
				to[prop] = from[prop];
		}
	};

	for (const rmsGlobal in biome)
	{
		if (rmsGlobal == "Description")
			continue;

		if (!global["g_" + rmsGlobal])
			throw new Error(rmsGlobal + " not defined!");

		copyProperties(biome[rmsGlobal], global["g_" + rmsGlobal]);
	}
}

function rBiomeTreeCount(multiplier = 1)
{
	return [
		g_ResourceCounts.trees.min * multiplier,
		g_ResourceCounts.trees.max * multiplier,
		g_ResourceCounts.trees.forestProbability
	];
}
