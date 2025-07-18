/**
 * Whether to log the water levels and which units became killed or transformed to visual actors.
 */
var debugLog = false;

/**
 * Whether to rise the water to the maximum level in a minute or two.
 */
var debugWaterRise = false;

/**
 * Duration in minutes for which the notification will be shown that states that the water will rise soon.
 */
var waterRiseNotificationDuration = 1;

/**
 * Time in minutes between increases of the water level.
 * If the water rises too fast, the hills are of no strategic importance,
 * building structures would be pointless.
 *
 * At height 27, most trees are not gatherable anymore and enemies not reachable.
 * At height 37 most hills are barely usable.
 *
 * At min 30 stuff at the ground level should not be gatherable anymore.
 * At min 45 CC should be destroyed.
 *
 * Notice regular and military docks will raise with the water!
 */
var waterIncreaseTime = [0.5, 1];

/**
 * Number of meters the waterheight increases each step.
 * Each time the water level is changed, the pathfinder grids have to be recomputed.
 * Therefore raising the level should occur as rarely as possible, i.e. have the value
 * as big as possible, but as small as needed to keep it visually authentic.
 */
var waterLevelIncreaseHeight = 1;

/**
 * At which height to stop increasing the water level.
 * Since players can survive on ships, don't endlessly raise the water.
 */
var maxWaterLevel = 70;

/**
 * Let buildings, relics and siege engines become actors, but kill organic units.
 */
var drownClass = "Organic";

/**
 * Maximum height that units and structures can be submerged before drowning or becoming destructed.
 */
var drownHeight = 1;

/**
 * One of these warnings is printed some minutes before the water level starts to rise.
 */
var waterWarningTexts = [
	markForTranslation("It keeps on raining, we will have to evacuate soon!"),
	markForTranslation("The rivers are standing high, we need to find a safe place!"),
	markForTranslation("We have to find dry ground, our lands will drown soon!"),
	markForTranslation("The lakes start swallowing the land, we have to find shelter!")
];

/**
 * Units to be garrisoned in the wooden towers.
 */
var garrisonedUnits = "units/rome/champion_infantry_swordsman_02";

Trigger.prototype.RaisingWaterNotification = function()
{
	Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface).AddTimeNotification({
		"message": pickRandom(waterWarningTexts),
		"translateMessage": true
	}, waterRiseNotificationDuration * 60 * 1000);
};

Trigger.prototype.DebugLog = function(txt)
{
	if (!debugLog)
		return;

	print("DEBUG [" + Math.round(TriggerHelper.GetMinutes()) + "] " + txt + "\n");
};

Trigger.prototype.GarrisonWoodenTowers = function()
{
	for (const gaiaEnt of Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager).GetEntitiesByPlayer(0))
	{
		const cmpIdentity = Engine.QueryInterface(gaiaEnt, IID_Identity);
		if (!cmpIdentity || !cmpIdentity.HasClass("Tower"))
			continue;

		const cmpGarrisonHolder = Engine.QueryInterface(gaiaEnt, IID_GarrisonHolder);
		if (!cmpGarrisonHolder)
			continue;

		TriggerHelper.SpawnGarrisonedUnits(gaiaEnt, garrisonedUnits, cmpGarrisonHolder.GetCapacity(), 0);
	}
};

Trigger.prototype.RaiseWaterLevelStep = function()
{
	const cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	const time = cmpTimer.GetTime();
	const cmpWaterManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_WaterManager);
	const newLevel = cmpWaterManager.GetWaterLevel() + waterLevelIncreaseHeight;
	cmpWaterManager.SetWaterLevel(newLevel);
	this.DebugLog("Raising water level to " + Math.round(newLevel) + " took " + (cmpTimer.GetTime() - time));

	if (newLevel < maxWaterLevel)
		this.DoAfterDelay((debugWaterRise ? 10 : randFloat(...waterIncreaseTime) * 60) * 1000, "RaiseWaterLevelStep", {});
	else
		this.DebugLog("Water reached final level");

	const actorTemplates = {};
	const killedTemplates = {};

	const cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	const cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);

	for (const ent of cmpRangeManager.GetGaiaAndNonGaiaEntities())
	{
		const cmpPosition = Engine.QueryInterface(ent, IID_Position);
		if (!cmpPosition || !cmpPosition.IsInWorld())
			continue;

		const pos = cmpPosition.GetPosition();
		if (pos.y + drownHeight >= newLevel)
			continue;

		const cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
		if (!cmpIdentity)
			continue;

		const templateName = cmpTemplateManager.GetCurrentTemplateName(ent);

		// Animals and units drown
		const cmpHealth = Engine.QueryInterface(ent, IID_Health);
		if (cmpHealth && cmpIdentity.HasClass(drownClass))
		{
			cmpHealth.Kill();

			if (debugLog)
				killedTemplates[templateName] = (killedTemplates[templateName] || 0) + 1;

			continue;
		}

		// Resources and buildings become actors
		// Do not use ChangeEntityTemplate for performance and
		// because we don't need nor want the effects of MT_EntityRenamed

		const cmpVisualActor = Engine.QueryInterface(ent, IID_Visual);
		if (!cmpVisualActor)
			continue;

		const height = cmpPosition.GetHeightOffset();
		const rot = cmpPosition.GetRotation();

		const actorTemplate = cmpTemplateManager.GetTemplate(templateName).VisualActor.Actor;
		const seed = cmpVisualActor.GetActorSeed();
		Engine.DestroyEntity(ent);

		const newEnt = Engine.AddEntity("actor|" + actorTemplate);
		Engine.QueryInterface(newEnt, IID_Visual).SetActorSeed(seed);

		const cmpNewPos = Engine.QueryInterface(newEnt, IID_Position);
		cmpNewPos.JumpTo(pos.x, pos.z);
		cmpNewPos.SetHeightOffset(height);
		cmpNewPos.SetXZRotation(rot.x, rot.z);
		cmpNewPos.SetYRotation(rot.y);

		if (debugLog)
			actorTemplates[templateName] = (actorTemplates[templateName] || 0) + 1;
	}

	this.DebugLog("Checking entities took " + (cmpTimer.GetTime() - time));
	this.DebugLog("Killed: " + uneval(killedTemplates));
	this.DebugLog("Converted to actors: " + uneval(actorTemplates));
};

{
	const waterRiseTime = debugWaterRise ? 0 : (InitAttributes.settings.SeaLevelRiseTime || 0);
	const cmpTrigger = Engine.QueryInterface(SYSTEM_ENTITY, IID_Trigger);
	cmpTrigger.GarrisonWoodenTowers();
	cmpTrigger.DoAfterDelay((waterRiseTime - waterRiseNotificationDuration) * 60 * 1000, "RaisingWaterNotification", {});
	cmpTrigger.DoAfterDelay(waterRiseTime * 60 * 1000, "RaiseWaterLevelStep", {});
}
