/**
 * This class provides a cache for accessing damage types metadata stored in JSON files.
 * Note that damage types need not be defined in JSON files to be handled in-game.
 * (this is intended to simplify modding)
 * This class must be initialised before using, as initialising it directly in globalscripts would
 * introduce disk I/O every time e.g. a GUI page is loaded.
 */
class DamageTypesMetadata
{
	constructor()
	{
		this.damageTypeData = {};

		const files = Engine.ListDirectoryFiles("simulation/data/damage_types", "*.json", false);
		for (const filename of files)
		{
			const data = Engine.ReadJSONFile(filename);
			if (!data)
				continue;

			if (data.code in this.damageTypeData)
			{
				error("Encountered two damage types with the code " + data.name);
				continue;
			}

			this.damageTypeData[data.code] = data;
		}

		const hasMetadata = (a) => this.damageTypeData[a] ? -1 : 1;
		this._sort = (a, b) => {
			if (this.damageTypeData[a] && this.damageTypeData[b])
				return this.damageTypeData[a].order - this.damageTypeData[b].order;
			return hasMetadata(a) - hasMetadata(b);
		};
	}

	/**
	 * @param {string[]} damageTypes - The damageTypes to sort.
	 * @returns {string[]} - The damageTypes in sorted order; first the ones
	 *			where metadata is provided, then the rest.
	 */
	sort(damageTypes)
	{
		const sorted = damageTypes.slice();
		sorted.sort(this._sort);
		return sorted;
	}

	/**
	 * @returns the name of the @param code damage type, or @code if no metadata exists in JSON files.
	 */
	getName(code)
	{
		return this.damageTypeData[code] ? this.damageTypeData[code].name : code;
	}
}
