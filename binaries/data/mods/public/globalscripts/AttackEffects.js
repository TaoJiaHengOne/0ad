/**
 * This class provides a cache for accessing attack effects stored in JSON files.
 */
class AttackEffects
{
	constructor()
	{
		const effectsDataObj = {};
		this.effectReceivers = [];

		for (const filename of Engine.ListDirectoryFiles("simulation/data/attack_effects", "*.json", false))
		{
			const data = Engine.ReadJSONFile(filename);
			if (!data)
				continue;

			if (effectsDataObj[data.code])
			{
				error("Encountered two effect types with the code " + data.name + ".");
				continue;
			}

			effectsDataObj[data.code] = data;

			this.effectReceivers.push({
				"type": data.code,
				"IID": data.IID,
				"method": data.method
			});
		}

		const effDataSort = (a, b) => a.order < b.order ? -1 : a.order > b.order ? 1 : 0;
		const effSort = (a, b) => effDataSort(
			effectsDataObj[a.type],
			effectsDataObj[b.type]
		);
		this.effectReceivers.sort(effSort);

		deepfreeze(this.effectReceivers);
	}

	/**
	 * @return {Object[]} - The effects possible with their data.
	 */
	Receivers()
	{
		return this.effectReceivers;
	}
}
