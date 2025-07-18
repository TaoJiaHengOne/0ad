function Auras() {}

Auras.prototype.Schema =
	"<attribute name='datatype'>" +
		"<value>tokens</value>" +
	"</attribute>" +
	"<text a:help='A whitespace-separated list of aura files, placed under simulation/data/auras/'/>";

Auras.prototype.Init = function()
{
	this.affectedPlayers = {};

	for (const name of this.GetAuraNames())
		this.affectedPlayers[name] = [];

	// In case of autogarrisoning, this component can be called before ownership is set.
	// So it needs to be completely initialised from the start.
	this.Clean();
};

// We can modify identifier if we want stackable auras in some case.
Auras.prototype.GetModifierIdentifier = function(name)
{
	if (AuraTemplates.Get(name).stackable)
		return "aura/" + name + this.entity;
	return "aura/" + name;
};

Auras.prototype.GetDescriptions = function()
{
	var ret = {};
	for (const auraID of this.GetAuraNames())
	{
		const aura = AuraTemplates.Get(auraID);
		ret[auraID] = {
			"name": {
				"generic": aura.auraName
			},
			"description": aura.auraDescription || null,
			"radius": this.GetRange(auraID) || null
		};
	}
	return ret;
};

Auras.prototype.GetAuraNames = function()
{
	return this.template._string.split(/\s+/);
};

Auras.prototype.GetOverlayIcon = function(name)
{
	return AuraTemplates.Get(name).overlayIcon || "";
};

Auras.prototype.GetAffectedEntities = function(name)
{
	return this[name].targetUnits;
};

Auras.prototype.GetRange = function(name)
{
	if (this.IsRangeAura(name))
		return +AuraTemplates.Get(name).radius;
	return undefined;
};

Auras.prototype.GetClasses = function(name)
{
	return AuraTemplates.Get(name).affects;
};

Auras.prototype.GetModifications = function(name)
{
	return AuraTemplates.Get(name).modifications;
};

Auras.prototype.GetAffectedPlayers = function(name)
{
	return this.affectedPlayers[name];
};

Auras.prototype.GetRangeOverlays = function()
{
	const rangeOverlays = [];

	for (const name of this.GetAuraNames())
	{
		if (!this.IsRangeAura(name) || !this[name].isApplied)
			continue;

		const rangeOverlay = AuraTemplates.Get(name).rangeOverlay;

		rangeOverlays.push(
			rangeOverlay ?
				{
					"radius": this.GetRange(name),
					"texture": rangeOverlay.lineTexture,
					"textureMask": rangeOverlay.lineTextureMask,
					"thickness": rangeOverlay.lineThickness
				} :
				// Specify default in order not to specify it in about 40 auras
				{
					"radius": this.GetRange(name),
					"texture": "outline_border.png",
					"textureMask": "outline_border_mask.png",
					"thickness": 0.2
				});
	}

	return rangeOverlays;
};

Auras.prototype.CalculateAffectedPlayers = function(name)
{
	var affectedPlayers = AuraTemplates.Get(name).affectedPlayers || ["Player"];
	this.affectedPlayers[name] = [];

	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	if (!cmpPlayer)
		cmpPlayer = QueryOwnerInterface(this.entity);

	if (!cmpPlayer || cmpPlayer.IsDefeated())
		return;

	const playerID = cmpPlayer.GetPlayerID();
	const cmpDiplomacy = Engine.QueryInterface(this.entity, IID_Diplomacy) ??
		QueryPlayerIDInterface(playerID, IID_Diplomacy);

	const cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);
	for (const i of cmpPlayerManager.GetAllPlayers())
	{
		const cmpAffectedPlayer = QueryPlayerIDInterface(i);
		if (!cmpAffectedPlayer || cmpAffectedPlayer.IsDefeated())
			continue;

		if (affectedPlayers.some(p => p == "Player" ? playerID == i : cmpDiplomacy["Is" + p](i)))
			this.affectedPlayers[name].push(i);
	}
};

Auras.prototype.CanApply = function(name)
{
	if (!AuraTemplates.Get(name).requiredTechnology)
		return true;

	const cmpTechnologyManager = QueryOwnerInterface(this.entity, IID_TechnologyManager);
	if (!cmpTechnologyManager)
		return false;

	return cmpTechnologyManager.IsTechnologyResearched(AuraTemplates.Get(name).requiredTechnology);
};

Auras.prototype.HasFormationAura = function()
{
	return this.GetAuraNames().some(n => this.IsFormationAura(n));
};

Auras.prototype.HasGarrisonAura = function()
{
	return this.GetAuraNames().some(n => this.IsGarrisonAura(n));
};

Auras.prototype.HasGarrisonedUnitsAura = function()
{
	return this.GetAuraNames().some(n => this.IsGarrisonedUnitsAura(n));
};

Auras.prototype.GetType = function(name)
{
	return AuraTemplates.Get(name).type;
};

Auras.prototype.IsFormationAura = function(name)
{
	return this.GetType(name) == "formation";
};

Auras.prototype.IsGarrisonAura = function(name)
{
	return this.GetType(name) == "garrison";
};

Auras.prototype.IsGarrisonedUnitsAura = function(name)
{
	return this.GetType(name) == "garrisonedUnits";
};

Auras.prototype.IsTurretedUnitsAura = function(name)
{
	return this.GetType(name) == "turretedUnits";
};

Auras.prototype.IsRangeAura = function(name)
{
	return this.GetType(name) == "range";
};

Auras.prototype.IsGlobalAura = function(name)
{
	return this.GetType(name) == "global";
};

Auras.prototype.IsPlayerAura = function(name)
{
	return this.GetType(name) == "player";
};

/**
 * clean all bonuses. Remove the old ones and re-apply the new ones
 */
Auras.prototype.Clean = function()
{
	var cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	var auraNames = this.GetAuraNames();
	const targetUnitsClone = {};
	let needVisualizationUpdate = false;
	// remove all bonuses
	for (const name of auraNames)
	{
		targetUnitsClone[name] = [];
		if (!this[name])
			continue;

		if (this.IsRangeAura(name))
			needVisualizationUpdate = true;

		if (this[name].targetUnits)
			targetUnitsClone[name] = this[name].targetUnits.slice();

		if (this.IsGlobalAura(name))
			this.RemoveTemplateAura(name);

		this.RemoveAura(name, this[name].targetUnits);

		if (this[name].rangeQuery)
			cmpRangeManager.DestroyActiveQuery(this[name].rangeQuery);
	}

	for (const name of auraNames)
	{
		// only calculate the affected players on re-applying the bonuses
		// this makes sure the template bonuses are removed from the correct players
		this.CalculateAffectedPlayers(name);
		// initialise range query
		this[name] = {};
		this[name].targetUnits = [];
		this[name].isApplied = this.CanApply(name);
		var affectedPlayers = this.GetAffectedPlayers(name);

		if (!affectedPlayers.length)
			continue;

		if (this.IsGlobalAura(name))
		{
			this.ApplyTemplateAura(name, affectedPlayers);
			// Only need to call ApplyAura for the aura icons, so skip it if there are none.
			if (this.GetOverlayIcon(name))
				for (const player of affectedPlayers)
					this.ApplyAura(name, cmpRangeManager.GetEntitiesByPlayer(player));
			continue;
		}

		if (this.IsPlayerAura(name))
		{
			const cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);
			this.ApplyAura(name, affectedPlayers.map(p => cmpPlayerManager.GetPlayerByID(p)));
			continue;
		}

		if (!this.IsRangeAura(name))
		{
			this.ApplyAura(name, targetUnitsClone[name]);
			continue;
		}

		needVisualizationUpdate = true;

		if (this[name].isApplied && (this.IsRangeAura(name) || this.IsGlobalAura(name) && !!this.GetOverlayIcon(name)))
		{
			// Do not account for entity sizes: structures can have various sizes
			// and we currently prefer auras to not depend on the source size
			// (this is generally irrelevant for units).
			this[name].rangeQuery = cmpRangeManager.CreateActiveQuery(
				this.entity,
				0,
				this.GetRange(name),
				affectedPlayers,
				IID_Identity,
				cmpRangeManager.GetEntityFlagMask("normal"),
				false
			);
			cmpRangeManager.EnableActiveQuery(this[name].rangeQuery);
		}
	}

	if (needVisualizationUpdate)
	{
		const cmpRangeOverlayManager = Engine.QueryInterface(this.entity, IID_RangeOverlayManager);
		if (cmpRangeOverlayManager)
		{
			cmpRangeOverlayManager.UpdateRangeOverlays("Auras");
			cmpRangeOverlayManager.RegenerateRangeOverlays(false);
		}
	}
};

Auras.prototype.GiveMembersWithValidClass = function(auraName, entityList)
{
	var match = this.GetClasses(auraName);
	return entityList.filter(ent => {
		const cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
		return cmpIdentity && MatchesClassList(cmpIdentity.GetClassesList(), match);
	});
};

Auras.prototype.OnRangeUpdate = function(msg)
{
	for (const name of this.GetAuraNames().filter(n => this[n] && msg.tag == this[n].rangeQuery))
	{
		this.ApplyAura(name, msg.added);
		this.RemoveAura(name, msg.removed);
	}
};

Auras.prototype.OnGarrisonedUnitsChanged = function(msg)
{
	for (const name of this.GetAuraNames().filter(n => this.IsGarrisonedUnitsAura(n)))
	{
		this.ApplyAura(name, msg.added);
		this.RemoveAura(name, msg.removed);
	}
};

Auras.prototype.OnTurretsChanged = function(msg)
{
	for (const name of this.GetAuraNames().filter(n => this.IsTurretedUnitsAura(n)))
	{
		this.ApplyAura(name, msg.added);
		this.RemoveAura(name, msg.removed);
	}
};

Auras.prototype.ApplyFormationAura = function(memberList)
{
	for (const name of this.GetAuraNames().filter(n => this.IsFormationAura(n)))
		this.ApplyAura(name, memberList);
};

Auras.prototype.ApplyGarrisonAura = function(structure)
{
	for (const name of this.GetAuraNames().filter(n => this.IsGarrisonAura(n)))
		this.ApplyAura(name, [structure]);
};

Auras.prototype.ApplyTemplateAura = function(name, players)
{
	if (!this[name].isApplied)
		return;

	if (!this.IsGlobalAura(name))
		return;

	const derivedModifiers = DeriveModificationsFromTech({
		"modifications": this.GetModifications(name),
		"affects": this.GetClasses(name)
	});
	const cmpModifiersManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_ModifiersManager);
	const cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);

	const modifName = this.GetModifierIdentifier(name);
	for (const player of players)
		cmpModifiersManager.AddModifiers(modifName, derivedModifiers, cmpPlayerManager.GetPlayerByID(player));
};

Auras.prototype.RemoveFormationAura = function(memberList)
{
	for (const name of this.GetAuraNames().filter(n => this.IsFormationAura(n)))
		this.RemoveAura(name, memberList);
};

Auras.prototype.RemoveGarrisonAura = function(structure)
{
	for (const name of this.GetAuraNames().filter(n => this.IsGarrisonAura(n)))
		this.RemoveAura(name, [structure]);
};

Auras.prototype.RemoveTemplateAura = function(name)
{
	if (!this[name].isApplied)
		return;

	if (!this.IsGlobalAura(name))
		return;

	const derivedModifiers = DeriveModificationsFromTech({
		"modifications": this.GetModifications(name),
		"affects": this.GetClasses(name)
	});
	const cmpModifiersManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_ModifiersManager);
	const cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);

	const modifName = this.GetModifierIdentifier(name);
	for (const player of this.GetAffectedPlayers(name))
	{
		const playerId = cmpPlayerManager.GetPlayerByID(player);
		for (const modifierPath in derivedModifiers)
			cmpModifiersManager.RemoveModifier(modifierPath, modifName, playerId);
	}
};

Auras.prototype.ApplyAura = function(name, ents)
{
	var validEnts = this.GiveMembersWithValidClass(name, ents);
	if (!validEnts.length)
		return;

	this[name].targetUnits = this[name].targetUnits.concat(validEnts);

	if (!this[name].isApplied)
		return;

	// update status bars if this has an icon
	if (this.GetOverlayIcon(name))
		for (const ent of validEnts)
		{
			const cmpStatusBars = Engine.QueryInterface(ent, IID_StatusBars);
			if (cmpStatusBars)
				cmpStatusBars.AddAuraSource(this.entity, name);
		}

	// Global aura modifications are handled at the player level by the modification manager,
	// so stop after icons have been applied.
	if (this.IsGlobalAura(name))
		return;

	const cmpModifiersManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_ModifiersManager);

	const derivedModifiers = DeriveModificationsFromTech({
		"modifications": this.GetModifications(name),
		"affects": this.GetClasses(name)
	});

	const modifName = this.GetModifierIdentifier(name);
	for (const ent of validEnts)
		cmpModifiersManager.AddModifiers(modifName, derivedModifiers, ent);
};

Auras.prototype.RemoveAura = function(name, ents, skipModifications = false)
{
	var validEnts = this.GiveMembersWithValidClass(name, ents);
	if (!validEnts.length)
		return;

	this[name].targetUnits = this[name].targetUnits.filter(v => validEnts.indexOf(v) == -1);

	if (!this[name].isApplied)
		return;

	// update status bars if this has an icon
	if (this.GetOverlayIcon(name))
		for (const ent of validEnts)
		{
			const cmpStatusBars = Engine.QueryInterface(ent, IID_StatusBars);
			if (cmpStatusBars)
				cmpStatusBars.RemoveAuraSource(this.entity, name);
		}

	// Global aura modifications are handled at the player level by the modification manager,
	// so stop after icons have been removed.
	if (this.IsGlobalAura(name))
		return;

	const cmpModifiersManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_ModifiersManager);

	const derivedModifiers = DeriveModificationsFromTech({
		"modifications": this.GetModifications(name),
		"affects": this.GetClasses(name)
	});

	const modifName = this.GetModifierIdentifier(name);
	for (const ent of ents)
		for (const modifierPath in derivedModifiers)
			cmpModifiersManager.RemoveModifier(modifierPath, modifName, ent);
};

Auras.prototype.OnOwnershipChanged = function(msg)
{
	this.Clean();
};

Auras.prototype.OnDiplomacyChanged = function(msg)
{
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	if (cmpPlayer && (cmpPlayer.GetPlayerID() == msg.player || cmpPlayer.GetPlayerID() == msg.otherPlayer) ||
	   IsOwnedByPlayer(msg.player, this.entity) ||
	   IsOwnedByPlayer(msg.otherPlayer, this.entity))
		this.Clean();
};

Auras.prototype.OnGlobalResearchFinished = function(msg)
{
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	if ((!cmpPlayer || cmpPlayer.GetPlayerID() != msg.player) && !IsOwnedByPlayer(msg.player, this.entity))
		return;
	for (const name of this.GetAuraNames())
	{
		const requiredTech = AuraTemplates.Get(name).requiredTechnology;
		if (requiredTech && requiredTech == msg.tech)
		{
			this.Clean();
			return;
		}
	}
};

/**
 * Update auras of the player entity and entities affecting player entities that didn't change ownership.
 */
Auras.prototype.OnGlobalPlayerDefeated = function(msg)
{
	const cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	if (cmpPlayer && cmpPlayer.GetPlayerID() == msg.playerId ||
		this.GetAuraNames().some(name => this.GetAffectedPlayers(name).indexOf(msg.playerId) != -1))
		this.Clean();
};

Auras.prototype.OnGarrisonedStateChanged = function(msg)
{
	if (!this.HasGarrisonAura())
		return;

	if (msg.holderID != INVALID_ENTITY)
		this.ApplyGarrisonAura(msg.holderID);
	if (msg.oldHolder != INVALID_ENTITY)
		this.RemoveGarrisonAura(msg.oldHolder);
};

Engine.RegisterComponentType(IID_Auras, "Auras", Auras);
