// Number of rounds of firing per 2 seconds.
const roundCount = 20;
const attackType = "Ranged";

function BuildingAI() {}

BuildingAI.prototype.Schema =
	"<element name='DefaultArrowCount'>" +
		"<data type='nonNegativeInteger'/>" +
	"</element>" +
	"<optional>" +
		"<element name='MaxArrowCount' a:help='Limit the number of arrows to a certain amount'>" +
			"<data type='nonNegativeInteger'/>" +
		"</element>" +
	"</optional>" +
	"<element name='GarrisonArrowMultiplier'>" +
		"<ref name='nonNegativeDecimal'/>" +
	"</element>" +
	"<element name='GarrisonArrowClasses' a:help='Add extra arrows for this class list'>" +
		"<text/>" +
	"</element>";

BuildingAI.prototype.MAX_PREFERENCE_BONUS = 2;

BuildingAI.prototype.Init = function()
{
	this.currentRound = 0;
	this.archersGarrisoned = 0;
	this.arrowsLeft = 0;
	this.targetUnits = [];
	this.focusTargets = [];
};

BuildingAI.prototype.OnGarrisonedUnitsChanged = function(msg)
{
	let classes = this.template.GarrisonArrowClasses;
	for (let ent of msg.added)
	{
		let cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
		if (cmpIdentity && MatchesClassList(cmpIdentity.GetClassesList(), classes))
			++this.archersGarrisoned;
	}
	for (let ent of msg.removed)
	{
		let cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
		if (cmpIdentity && MatchesClassList(cmpIdentity.GetClassesList(), classes))
			--this.archersGarrisoned;
	}
};

BuildingAI.prototype.OnOwnershipChanged = function(msg)
{
	this.targetUnits = [];
	this.focusTargets = [];
	this.SetupRangeQuery();
	this.SetupGaiaRangeQuery();
};

BuildingAI.prototype.OnDiplomacyChanged = function(msg)
{
	if (!IsOwnedByPlayer(msg.player, this.entity))
		return;

	// Remove maybe now allied/neutral units.
	this.targetUnits = [];
	this.SetupRangeQuery();
	this.SetupGaiaRangeQuery();
};

BuildingAI.prototype.OnDestroy = function()
{
	if (this.timer)
	{
		let cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
		cmpTimer.CancelTimer(this.timer);
		this.timer = undefined;
	}

	// Clean up range queries.
	let cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	if (this.enemyUnitsQuery)
		cmpRangeManager.DestroyActiveQuery(this.enemyUnitsQuery);
	if (this.gaiaUnitsQuery)
		cmpRangeManager.DestroyActiveQuery(this.gaiaUnitsQuery);
};

/**
 * React on Attack value modifications, as it might influence the range.
 */
BuildingAI.prototype.OnValueModification = function(msg)
{
	if (msg.component != "Attack")
		return;

	this.targetUnits = [];
	this.SetupRangeQuery();
	this.SetupGaiaRangeQuery();
};

/**
 * Setup the Range Query to detect units coming in & out of range.
 */
BuildingAI.prototype.SetupRangeQuery = function()
{
	var cmpAttack = Engine.QueryInterface(this.entity, IID_Attack);
	if (!cmpAttack)
		return;

	var cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	if (this.enemyUnitsQuery)
	{
		cmpRangeManager.DestroyActiveQuery(this.enemyUnitsQuery);
		this.enemyUnitsQuery = undefined;
	}

	const cmpDiplomacy = QueryOwnerInterface(this.entity, IID_Diplomacy);
	if (!cmpDiplomacy)
		return;

	const enemies = cmpDiplomacy.GetEnemies();
	// Remove gaia.
	if (enemies.length && enemies[0] == 0)
		enemies.shift();

	if (!enemies.length)
		return;

	const range = cmpAttack.GetRange(attackType);
	const yOrigin = cmpAttack.GetAttackYOrigin(attackType);
	// This takes entity sizes into accounts, so no need to compensate for structure size.
	this.enemyUnitsQuery = cmpRangeManager.CreateActiveParabolicQuery(
		this.entity, range.min, range.max, yOrigin,
		enemies, IID_Resistance, cmpRangeManager.GetEntityFlagMask("normal"));

	cmpRangeManager.EnableActiveQuery(this.enemyUnitsQuery);
};

// Set up a range query for Gaia units within LOS range which can be attacked.
// This should be called whenever our ownership changes.
BuildingAI.prototype.SetupGaiaRangeQuery = function()
{
	var cmpAttack = Engine.QueryInterface(this.entity, IID_Attack);
	if (!cmpAttack)
		return;

	var cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	if (this.gaiaUnitsQuery)
	{
		cmpRangeManager.DestroyActiveQuery(this.gaiaUnitsQuery);
		this.gaiaUnitsQuery = undefined;
	}

	if (!QueryOwnerInterface(this.entity, IID_Diplomacy)?.IsEnemy(0))
		return;

	const range = cmpAttack.GetRange(attackType);
	const yOrigin = cmpAttack.GetAttackYOrigin(attackType);

	// This query is only interested in Gaia entities that can attack.
	// This takes entity sizes into accounts, so no need to compensate for structure size.
	this.gaiaUnitsQuery = cmpRangeManager.CreateActiveParabolicQuery(
		this.entity, range.min, range.max, yOrigin,
		[0], IID_Attack, cmpRangeManager.GetEntityFlagMask("normal"));

	cmpRangeManager.EnableActiveQuery(this.gaiaUnitsQuery);
};

/**
 * Called when units enter or leave range.
 */
BuildingAI.prototype.OnRangeUpdate = function(msg)
{

	var cmpAttack = Engine.QueryInterface(this.entity, IID_Attack);
	if (!cmpAttack)
		return;

	// Target enemy units except non-dangerous animals.
	if (msg.tag == this.gaiaUnitsQuery)
	{
		msg.added = msg.added.filter(e => {
			let cmpUnitAI = Engine.QueryInterface(e, IID_UnitAI);
			return cmpUnitAI && (!cmpUnitAI.IsAnimal() || cmpUnitAI.IsDangerousAnimal());
		});
	}
	else if (msg.tag != this.enemyUnitsQuery)
		return;

	// Add new targets.
	for (let entity of msg.added)
		if (cmpAttack.CanAttack(entity))
			this.targetUnits.push(entity);

	// Remove targets outside of vision-range.
	for (let entity of msg.removed)
	{
		let index = this.targetUnits.indexOf(entity);
		if (index > -1)
			this.targetUnits.splice(index, 1);
	}

	if (this.targetUnits.length)
		this.StartTimer();
};

BuildingAI.prototype.StartTimer = function()
{
	if (this.timer)
		return;

	var cmpAttack = Engine.QueryInterface(this.entity, IID_Attack);
	if (!cmpAttack)
		return;

	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	var attackTimers = cmpAttack.GetTimers(attackType);

	this.timer = cmpTimer.SetInterval(this.entity, IID_BuildingAI, "FireArrows",
		attackTimers.prepare, attackTimers.repeat / roundCount, null);
};

BuildingAI.prototype.GetDefaultArrowCount = function()
{
	var arrowCount = +this.template.DefaultArrowCount;
	return Math.round(ApplyValueModificationsToEntity("BuildingAI/DefaultArrowCount", arrowCount, this.entity));
};

BuildingAI.prototype.GetMaxArrowCount = function()
{
	if (!this.template.MaxArrowCount)
		return Infinity;

	let maxArrowCount = +this.template.MaxArrowCount;
	return Math.round(ApplyValueModificationsToEntity("BuildingAI/MaxArrowCount", maxArrowCount, this.entity));
};

BuildingAI.prototype.GetGarrisonArrowMultiplier = function()
{
	var arrowMult = +this.template.GarrisonArrowMultiplier;
	return ApplyValueModificationsToEntity("BuildingAI/GarrisonArrowMultiplier", arrowMult, this.entity);
};

BuildingAI.prototype.GetGarrisonArrowClasses = function()
{
	var string = this.template.GarrisonArrowClasses;
	if (string)
		return string.split(/\s+/);
	return [];
};

/**
 * Returns the number of arrows which needs to be fired.
 * DefaultArrowCount + Garrisoned Archers (i.e., any unit capable
 * of shooting arrows from inside buildings).
 */
BuildingAI.prototype.GetArrowCount = function()
{
	let count = this.GetDefaultArrowCount() +
		Math.round(this.archersGarrisoned * this.GetGarrisonArrowMultiplier());

	return Math.min(count, this.GetMaxArrowCount());
};

BuildingAI.prototype.SetUnitAITarget = function(ent)
{
	this.unitAITarget = ent;
	if (ent)
		this.StartTimer();
};

/**
 * Adds index to keep track of the user-targeted units supporting a queue
 * @param {ent} - Target of focus-fire from unit-actions if the selection is an enemy.
 */
BuildingAI.prototype.AddFocusTarget = function(ent, queued, push)
{
	if (!ent || this.targetUnits.indexOf(ent) === -1)
		return;
	if (queued)
		this.focusTargets.push({"entityId": ent});
	else if (push)
		this.focusTargets.unshift({"entityId": ent});
	else
		this.focusTargets = [{"entityId": ent}];
};

/**
 * Fire arrows with random temporal distribution on prefered targets.
 * Called 'roundCount' times every 'RepeatTime' seconds when there are units in the range.
 */
BuildingAI.prototype.FireArrows = function()
{
	if (!this.targetUnits.length && !this.unitAITarget)
	{
		if (!this.timer)
			return;

		let cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
		cmpTimer.CancelTimer(this.timer);
		this.timer = undefined;
		return;
	}

	let cmpAttack = Engine.QueryInterface(this.entity, IID_Attack);
	if (!cmpAttack)
		return;

	if (this.currentRound > roundCount - 1)
		this.currentRound = 0;

	if (this.currentRound == 0)
		this.arrowsLeft = this.GetArrowCount();

	let arrowsToFire = 0;
	if (this.currentRound == roundCount - 1)
		arrowsToFire = this.arrowsLeft;
	else
		arrowsToFire = Math.min(
			// shooting arrows in the first quarter of rounds results in a burst.
			this.GetArrowCount() / (roundCount / 4),
			this.arrowsLeft
		);

	if (arrowsToFire <= 0)
	{
		++this.currentRound;
		return;
	}

	// Add targets to a list.
	let targets = [];
	let addTarget = function(target)
	{
	    const pref = (cmpAttack.GetPreference(target) ?? 49);
	    targets.push({"entityId": target, "preference": pref});
	};

	// Add the UnitAI target separately, as the UnitMotion and RangeManager implementations differ.
	if (this.unitAITarget && this.targetUnits.indexOf(this.unitAITarget) == -1)
		addTarget(this.unitAITarget);

	else if (this.unitAITarget && this.targetUnits.indexOf(this.unitAITarget) != -1)
		this.focusTargets = [{"entityId": this.unitAITarget}];

	if (!this.focusTargets.length)
	{
		for (let target of this.targetUnits)
	        addTarget(target);
		// Sort targets by preference and then by proximity.
		targets.sort( (a,b) => {
			if (a.preference > b.preference)
				return 1;
			else if (a.preference < b.preference)
				return -1;
			else if (PositionHelper.DistanceBetweenEntities(this.entity, a.entityId) > PositionHelper.DistanceBetweenEntities(this.entity, b.entityId))
				return 1;
			return -1;
		});
	}
	else
		targets = this.focusTargets;

	// The obstruction manager performs approximate range checks.
	// so we need to verify them here.
	// TODO: perhaps an optional 'precise' mode to range queries would be more performant.
	const cmpObstructionManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_ObstructionManager);
	const range = cmpAttack.GetRange(attackType);
	const yOrigin = cmpAttack.GetAttackYOrigin(attackType);

	let firedArrows = 0;
	let targetIndex = 0;	
	while (firedArrows < arrowsToFire && targetIndex < targets.length)
	{

		let selectedTarget = targets[targetIndex].entityId;
		if (this.CheckTargetVisible(selectedTarget) && cmpObstructionManager.IsInTargetParabolicRange(
			this.entity,
			selectedTarget,
			range.min,
			range.max,
			yOrigin,
			false))
		{
			cmpAttack.PerformAttack(attackType, selectedTarget);
			PlaySound("attack_" + attackType.toLowerCase(), this.entity);
			++firedArrows;
		}
		else
			++targetIndex;// Could not attack target, try a different target.
	}
	targets.splice(0, targetIndex);
	this.arrowsLeft -= firedArrows;
	++this.currentRound;
};

/**
 * Returns true if the target entity is visible through the FoW/SoD.
 */
BuildingAI.prototype.CheckTargetVisible = function(target)
{
	var cmpOwnership = Engine.QueryInterface(this.entity, IID_Ownership);
	if (!cmpOwnership)
		return false;

	// Entities that are hidden and miraged are considered visible.
	var cmpFogging = Engine.QueryInterface(target, IID_Fogging);
	if (cmpFogging && cmpFogging.IsMiraged(cmpOwnership.GetOwner()))
		return true;

	// Either visible directly, or visible in fog.
	let cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	return cmpRangeManager.GetLosVisibility(target, cmpOwnership.GetOwner()) != "hidden";
};

Engine.RegisterComponentType(IID_BuildingAI, "BuildingAI", BuildingAI);
