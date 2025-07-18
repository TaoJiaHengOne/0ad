function EntityLimits() {}

EntityLimits.prototype.Schema =
	"<a:help>Specifies per category limits on number of entities (buildings or units) that can be created for each player</a:help>" +
	"<a:example>" +
		"<Limits>" +
			"<Palace>1</Palace>" +
			"<Fortress>10</Fortress>" +
			"<Hero>1</Hero>" +
			"<Monument>5</Monument>" +
			"<Tower>25</Tower>" +
			"<Wonder>1</Wonder>" +
		"</Limits>" +
		"<LimitChangers>" +
			"<Monument>" +
				"<CivilCentre>2</CivilCentre>" +
			"</Monument>" +
		"</LimitChangers>" +
		"<LimitRemovers>" +
			"<CivilCentre>" +
				"<RequiredTechs datatype=\"tokens\">town_phase</RequiredTechs>" +
			"</CivilCentre>" +
		"</LimitRemovers>" +
	"</a:example>" +
	"<element name='Limits' a:help='List of building/unit(s) requirements on which to apply a specific limit.'>" +
		"<zeroOrMore>" +
			"<element a:help='Specifies a category of building/unit on which to apply this limit. See BuildRestrictions/TrainingRestrictions for possible categories'>" +
				"<anyName />" +
				"<data type='integer'/>" +
			"</element>" +
		"</zeroOrMore>" +
	"</element>" +
	"<element name='LimitChangers' a:help='List of building/unit(s) requirements that can change a specific limit.'>" +
		"<zeroOrMore>" +
			"<element a:help='Specifies a category of building/unit on which to apply this limit. See BuildRestrictions/TrainingRestrictions for possible categories'>" +
				"<anyName />" +
				"<zeroOrMore>" +
					"<element a:help='Specifies the class that changes the entity limit'>" +
						"<anyName />" +
						"<data type='integer'/>" +
					"</element>" +
				"</zeroOrMore>" +
			"</element>" +
		"</zeroOrMore>" +
	"</element>" +
	"<element name='LimitRemovers' a:help='List of building/unit(s) requirements to lift a specific limit.'>" +
		"<zeroOrMore>" +
			"<element a:help='Specifies a category of building/unit on which to remove this limit. The limit will be removed if all the following requirements are satisfied'>" +
				"<anyName />" +
				"<oneOrMore>" +
					"<element a:help='Possible requirements are: RequiredTechs and RequiredClasses'>" +
						"<anyName />" +
						"<attribute name='datatype'>" +
							"<value>tokens</value>" +
						"</attribute>" +
						"<text/>" +
					"</element>" +
				"</oneOrMore>" +
			"</element>" +
		"</zeroOrMore>" +
	"</element>";


const TRAINING = "training";
const BUILD = "build";

EntityLimits.prototype.Init = function()
{
	this.limit = {};
	// Counts entities which change the limit of the given category.
	this.count = {};
	this.changers = {};
	this.removers = {};
	// Counts entities with the given class, used in the limit removal.
	this.classCount = {};
	this.removedLimit = {};
	this.matchTemplateCount = {};
	for (var category in this.template.Limits)
	{
		this.limit[category] = +this.template.Limits[category];
		this.count[category] = 0;
		if (category in this.template.LimitChangers)
		{
			this.changers[category] = {};
			for (const c in this.template.LimitChangers[category])
				this.changers[category][c] = +this.template.LimitChangers[category][c];
		}
		if (category in this.template.LimitRemovers)
		{
			// Keep a copy of removable limits for possible restoration.
			this.removedLimit[category] = this.limit[category];
			this.removers[category] = {};
			for (const c in this.template.LimitRemovers[category])
			{
				this.removers[category][c] = this.template.LimitRemovers[category][c]._string.split(/\s+/);
				if (c === "RequiredClasses")
					for (var cls of this.removers[category][c])
						this.classCount[cls] = 0;
			}
		}
	}
};

EntityLimits.prototype.ChangeCount = function(category, value)
{
	if (this.count[category] !== undefined)
		this.count[category] += value;
};

EntityLimits.prototype.ChangeMatchCount = function(template, value)
{
	if (!this.matchTemplateCount[template])
		this.matchTemplateCount[template] = 0;

	this.matchTemplateCount[template] += value;
};

EntityLimits.prototype.GetLimits = function()
{
	return this.limit;
};

EntityLimits.prototype.GetCounts = function()
{
	return this.count;
};

EntityLimits.prototype.GetMatchCounts = function()
{
	return this.matchTemplateCount;
};

EntityLimits.prototype.GetLimitChangers = function()
{
	return this.changers;
};

EntityLimits.prototype.UpdateLimitsFromTech = function(tech)
{
	for (var category in this.removers)
		if ("RequiredTechs" in this.removers[category] && this.removers[category].RequiredTechs.indexOf(tech) !== -1)
			this.removers[category].RequiredTechs.splice(this.removers[category].RequiredTechs.indexOf(tech), 1);

	this.UpdateLimitRemoval();
};

EntityLimits.prototype.UpdateLimitRemoval = function()
{
	for (var category in this.removers)
	{
		var nolimit = true;
		if ("RequiredTechs" in this.removers[category])
			nolimit = !this.removers[category].RequiredTechs.length;
		if (nolimit && "RequiredClasses" in this.removers[category])
			for (var cls of this.removers[category].RequiredClasses)
				nolimit = nolimit && this.classCount[cls] > 0;

		if (nolimit && this.limit[category] !== undefined)
			this.limit[category] = undefined;
		else if (!nolimit && this.limit[category] === undefined)
			this.limit[category] = this.removedLimit[category];
	}
};

EntityLimits.prototype.AllowedToCreate = function(limitType, category, count, templateName, matchLimit)
{
	if (this.count[category] !== undefined && this.limit[category] !== undefined &&
		this.count[category] + count > this.limit[category])
	{
		this.NotifyLimit(limitType, category, this.limit[category]);
		return false;
	}

	if (this.matchTemplateCount[templateName] !== undefined && matchLimit !== undefined &&
		this.matchTemplateCount[templateName] + count > matchLimit)
	{
		this.NotifyLimit(limitType, category, matchLimit);
		return false;
	}

	return true;
};

EntityLimits.prototype.NotifyLimit = function(limitType, category, limit)
{
	const cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	const notification = {
		"players": [cmpPlayer.GetPlayerID()],
		"translateMessage": true,
		"translateParameters": ["category"],
		"parameters": { "category": category, "limit": limit },
	};

	if (limitType == BUILD)
		notification.message = markForTranslation("%(category)s build limit of %(limit)s reached");
	else if (limitType == TRAINING)
		notification.message = markForTranslation("%(category)s training limit of %(limit)s reached");
	else
	{
		warn("EntityLimits.js: Unknown LimitType " + limitType);
		notification.message = markForTranslation("%(category)s limit of %(limit)s reached");
	}
	const cmpGUIInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
	cmpGUIInterface.PushNotification(notification);
};

EntityLimits.prototype.AllowedToBuild = function(category)
{
	// We pass count 0 as the creation of the building has already taken place and
	// the ownership has been set (triggering OnGlobalOwnershipChanged)
	return this.AllowedToCreate(BUILD, category, 0);
};

EntityLimits.prototype.AllowedToTrain = function(category, count, templateName, matchLimit)
{
	return this.AllowedToCreate(TRAINING, category, count, templateName, matchLimit);
};

/**
 * @param {number} ent - id of the entity which would be replaced.
 * @param {string} template - name of the new template.
 * @return {boolean} - whether we can replace ent.
 */
EntityLimits.prototype.AllowedToReplace = function(ent, template)
{
	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	const templateFrom = cmpTemplateManager.GetTemplate(cmpTemplateManager.GetCurrentTemplateName(ent));
	const templateTo = cmpTemplateManager.GetTemplate(template);

	if (templateTo.TrainingRestrictions)
	{
		const category = templateTo.TrainingRestrictions.Category;
		return this.AllowedToCreate(TRAINING, category, templateFrom.TrainingRestrictions && templateFrom.TrainingRestrictions.Category == category ? 0 : 1);
	}

	if (templateTo.BuildRestrictions)
	{
		const category = templateTo.BuildRestrictions.Category;
		return this.AllowedToCreate(BUILD, category, templateFrom.BuildRestrictions && templateFrom.BuildRestrictions.Category == category ? 0 : 1);
	}

	return true;
};

EntityLimits.prototype.OnGlobalOwnershipChanged = function(msg)
{
	// check if we are adding or removing an entity from this player
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	if (!cmpPlayer)
	{
		error("EntityLimits component is defined on a non-player entity");
		return;
	}
	let modifier;
	if (msg.from == cmpPlayer.GetPlayerID())
		modifier = -1;
	else if (msg.to == cmpPlayer.GetPlayerID())
		modifier = 1;
	else
		return;

	// Update entity counts
	{
		let category;
		const cmpBuildRestrictions = Engine.QueryInterface(msg.entity, IID_BuildRestrictions);
		if (cmpBuildRestrictions)
			category = cmpBuildRestrictions.GetCategory();
		const cmpTrainingRestrictions = Engine.QueryInterface(msg.entity, IID_TrainingRestrictions);
		if (cmpTrainingRestrictions)
			category = cmpTrainingRestrictions.GetCategory();
		if (category)
			this.ChangeCount(category, modifier);
	}

	// Update entity limits
	var cmpIdentity = Engine.QueryInterface(msg.entity, IID_Identity);
	if (!cmpIdentity)
		return;

	// foundations shouldn't change the entity limits until they're completed
	var cmpFoundation = Engine.QueryInterface(msg.entity, IID_Foundation);
	if (cmpFoundation)
		return;
	var classes = cmpIdentity.GetClassesList();
	for (const category in this.changers)
		for (var c in this.changers[category])
			if (classes.indexOf(c) >= 0)
			{
				if (this.limit[category] != undefined)
					this.limit[category] += modifier * this.changers[category][c];
				if (this.removedLimit[category] != undefined)	// update removed limits in case we want to restore it
					this.removedLimit[category] += modifier * this.changers[category][c];
			}

	for (const category in this.removers)
		if ("RequiredClasses" in this.removers[category])
			for (var cls of this.removers[category].RequiredClasses)
				if (classes.indexOf(cls) !== -1)
					this.classCount[cls] += modifier;

	this.UpdateLimitRemoval();
};

Engine.RegisterComponentType(IID_EntityLimits, "EntityLimits", EntityLimits);
