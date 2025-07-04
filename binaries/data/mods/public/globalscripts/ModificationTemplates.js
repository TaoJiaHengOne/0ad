/**
 * @file This provides a cache for Aura and Technology templates.
 * They may not be serialized, otherwise rejoined clients would refer
 * to different objects, triggering an Out-of-sync error.
 */
function ModificationTemplates(path)
{
	const suffix = ".json";

	this.names = deepfreeze(listFiles(path, suffix, true));

	this.templates = {};

	for (const name of this.names)
		this.templates[name] = Engine.ReadJSONFile(path + name + suffix);

	deepfreeze(this.templates);
}

ModificationTemplates.prototype.GetNames = function()
{
	return this.names;
};

ModificationTemplates.prototype.Has = function(name)
{
	return this.names.indexOf(name) != -1;
};

ModificationTemplates.prototype.Get = function(name)
{
	return this.templates[name];
};

ModificationTemplates.prototype.GetAll = function()
{
	return this.templates;
};


function LoadModificationTemplates()
{
	global.AuraTemplates = new ModificationTemplates("simulation/data/auras/");
	global.TechnologyTemplates = new ModificationTemplates("simulation/data/technologies/");
}

/**
 * Derives modifications (to be applied to entities) from a given aura/technology.
 *
 * @param {Object} techTemplate - The aura/technology template to derive the modifications from.
 * @return {Object} - An object containing the relevant modifications.
 */
function DeriveModificationsFromTech(techTemplate)
{
	if (!techTemplate.modifications)
		return {};

	const techMods = {};
	let techAffects = [];
	if (techTemplate.affects && techTemplate.affects.length)
		techAffects = techTemplate.affects.map(affected => affected.split(/\s+/));
	else
		techAffects.push([]);

	for (const mod of techTemplate.modifications)
	{
		const affects = techAffects.slice();
		if (mod.affects)
		{
			const specAffects = mod.affects.split(/\s+/);
			for (const a in affects)
				affects[a] = affects[a].concat(specAffects);
		}

		const newModifier = { "affects": affects };
		for (const idx in mod)
			if (idx !== "value" && idx !== "affects")
				newModifier[idx] = mod[idx];

		if (!techMods[mod.value])
			techMods[mod.value] = [];
		techMods[mod.value].push(newModifier);
	}
	return techMods;
}

/**
 * Derives modifications (to be applied to entities) from a provided array
 * of aura/technology template data.
 *
 * @param {Object[]} techsDataArray
 * @return {Object} - The combined relevant modifications of all the technologies.
 */
function DeriveModificationsFromTechnologies(techsDataArray)
{
	if (!techsDataArray.length)
		return {};

	const derivedModifiers = {};
	for (const technology of techsDataArray)
	{
		// Auras don't have a "reqs" property
		if ('reqs' in technology && !technology.reqs)
			continue;

		const modifiers = DeriveModificationsFromTech(technology);
		for (const modPath in modifiers)
		{
			if (!derivedModifiers[modPath])
				derivedModifiers[modPath] = [];
			derivedModifiers[modPath] = derivedModifiers[modPath].concat(modifiers[modPath]);
		}
	}
	return derivedModifiers;
}

/**
 * Common definition of the XML schema for in-template modifications.
 */
const ModificationSchema =
"<interleave>" +
	"<element name='Paths' a:help='Space separated value paths to modify.'>" +
		"<attribute name='datatype'>" +
			"<value>tokens</value>" +
		"</attribute>" +
		"<text/>" +
	"</element>" +
	"<element name='Affects' a:help='An array of classes to affect.'>" +
		"<attribute name='datatype'>" +
			"<value>tokens</value>" +
		"</attribute>" +
		"<text/>" +
	"</element>" +
	"<choice>" +
		"<element name='Add'>" +
			"<data type='decimal' />" +
		"</element>" +
		"<element name='Multiply'>" +
			"<data type='decimal' />" +
		"</element>" +
		"<element name='Replace'>" +
			"<text/>" +
		"</element>" +
	"</choice>" +
"</interleave>";

const ModificationsSchema =
"<element name='Modifiers' a:help='List of modifiers.'>" +
	"<oneOrMore>" +
		"<element>" +
			"<anyName />" +
			ModificationSchema +
		"</element>" +
	"</oneOrMore>" +
"</element>";

/**
 * Derives a single modification (to be applied to entities) from a given XML template.
 *
 * @param {Object} techTemplate - The XML template node to derive the modification from.
 * @return {Object} containing the relevant modification.
 */
function DeriveModificationFromXMLTemplate(template)
{
	const effect = {};
	if (template.Add)
		effect.add = +template.Add;
	if (template.Multiply)
		effect.multiply = +template.Multiply;
	if (template.Replace)
		effect.replace = template.Replace;
	effect.affects = template.Affects ? template.Affects._string.split(/\s/) : [];

	const ret = {};
	for (const path of template.Paths._string.split(/\s/))
	{
		ret[path] = [effect];
	}

	return ret;
}

/**
 * Derives all modifications (to be applied to entities) from a given XML template.
 *
 * @param {Object} techTemplate - The XML template node to derive the modifications from.
 * @return {Object} containing the combined modifications.
 */
function DeriveModificationsFromXMLTemplate(template)
{
	const ret = {};
	for (const name in template)
	{
		const modification = DeriveModificationFromXMLTemplate(template[name]);
		for (const path in modification)
		{
			if (!ret[path])
				ret[path] = [];
			ret[path].push(modification[path][0]);
		}
	}
	return ret;
}
