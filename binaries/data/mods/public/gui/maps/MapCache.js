/**
 * This class obtains, caches, and provides the game settings from map XML and JSON files.
 */
class MapCache
{
	constructor()
	{
		this.cache = {};
	}

	checkIfExists(mapType, mapPath)
	{
		if (!mapPath || mapPath == "random")
			return true;

		return g_Settings.MapTypes.find(type => type.Name == mapType).CheckIfExists(mapPath);
	}

	getMapData(mapType, mapPath)
	{
		if (!mapPath || mapPath == "random")
			return undefined;
		if (!this.cache[mapPath])
		{
			const mapData = g_Settings.MapTypes.find(type => type.Name == mapType).GetData(mapPath);

			// Remove gaia, TODO: Maps should be consistent
			if (mapData &&
				mapData.settings &&
				mapData.settings.PlayerData &&
				mapData.settings.PlayerData.length &&
				!mapData.settings.PlayerData[0])
			{
				mapData.settings.PlayerData.shift();
			}

			this.cache[mapPath] = mapData;
		}

		return this.cache[mapPath];
	}

	/**
	 * Doesn't translate, so that lobby page viewers can do that locally.
	 * The result is to be used with translateMapName.
	 */
	getTranslatableMapName(mapType, mapPath)
	{
		if (mapPath == "random")
			return "random";

		const mapData = this.getMapData(mapType, mapPath);
		return mapData && mapData.settings && mapData.settings.Name || undefined;
	}

	translateMapName(mapName)
	{
		return mapName == "random" ?
			translateWithContext("map selection", "Random") :
			mapName ? translate(mapName) : "";
	}

	getTranslatedMapDescription(mapType, mapPath)
	{
		if (mapPath == "random")
			return translate("A randomly selected map.");

		const mapData = this.getMapData(mapType, mapPath);
		return mapData && mapData.settings && translate(mapData.settings.Description) || "";
	}

	previewExists(filename)
	{
		return Engine.TextureExists(this.TexturesPath + this.PreviewsPath + filename);
	}

	getMapPreview(mapType, mapPath, filename = undefined)
	{
		if (!filename)
		{
			const mapData = this.getMapData(mapType, mapPath);
			filename = mapData && mapData.settings && mapData.settings.Preview || this.DefaultPreview;
		}

		return "cropped:" + this.PreviewWidth + "," + this.PreviewHeight + ":" + this.PreviewsPath + filename;
	}
}

MapCache.prototype.TexturesPath =
	"art/textures/ui/";

MapCache.prototype.PreviewsPath =
	"session/icons/mappreview/";

MapCache.prototype.DefaultPreview =
	"nopreview.png";

MapCache.prototype.PreviewWidth =
	400 / 512;

MapCache.prototype.PreviewHeight =
	300 / 512;
