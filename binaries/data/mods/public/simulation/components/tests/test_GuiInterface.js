Engine.LoadHelperScript("ObstructionSnap.js");
Engine.LoadHelperScript("Player.js");
Engine.LoadComponentScript("interfaces/AlertRaiser.js");
Engine.LoadComponentScript("interfaces/Auras.js");
Engine.LoadComponentScript("interfaces/Barter.js");
Engine.LoadComponentScript("interfaces/Builder.js");
Engine.LoadComponentScript("interfaces/Capturable.js");
Engine.LoadComponentScript("interfaces/CeasefireManager.js");
Engine.LoadComponentScript("interfaces/DeathDamage.js");
Engine.LoadComponentScript("interfaces/Diplomacy.js");
Engine.LoadComponentScript("interfaces/EndGameManager.js");
Engine.LoadComponentScript("interfaces/EntityLimits.js");
Engine.LoadComponentScript("interfaces/Formation.js");
Engine.LoadComponentScript("interfaces/Foundation.js");
Engine.LoadComponentScript("interfaces/Garrisonable.js");
Engine.LoadComponentScript("interfaces/GarrisonHolder.js");
Engine.LoadComponentScript("interfaces/Gate.js");
Engine.LoadComponentScript("interfaces/Guard.js");
Engine.LoadComponentScript("interfaces/Heal.js");
Engine.LoadComponentScript("interfaces/Health.js");
Engine.LoadComponentScript("interfaces/Loot.js");
Engine.LoadComponentScript("interfaces/Market.js");
Engine.LoadComponentScript("interfaces/Pack.js");
Engine.LoadComponentScript("interfaces/Population.js");
Engine.LoadComponentScript("interfaces/PopulationCapManager.js");
Engine.LoadComponentScript("interfaces/ProductionQueue.js");
Engine.LoadComponentScript("interfaces/Promotion.js");
Engine.LoadComponentScript("interfaces/Repairable.js");
Engine.LoadComponentScript("interfaces/Researcher.js");
Engine.LoadComponentScript("interfaces/Resistance.js");
Engine.LoadComponentScript("interfaces/ResourceDropsite.js");
Engine.LoadComponentScript("interfaces/ResourceGatherer.js");
Engine.LoadComponentScript("interfaces/ResourceTrickle.js");
Engine.LoadComponentScript("interfaces/ResourceSupply.js");
Engine.LoadComponentScript("interfaces/TechnologyManager.js");
Engine.LoadComponentScript("interfaces/Trader.js");
Engine.LoadComponentScript("interfaces/Trainer.js");
Engine.LoadComponentScript("interfaces/TurretHolder.js");
Engine.LoadComponentScript("interfaces/Timer.js");
Engine.LoadComponentScript("interfaces/Treasure.js");
Engine.LoadComponentScript("interfaces/TreasureCollector.js");
Engine.LoadComponentScript("interfaces/Turretable.js");
Engine.LoadComponentScript("interfaces/StatisticsTracker.js");
Engine.LoadComponentScript("interfaces/StatusEffectsReceiver.js");
Engine.LoadComponentScript("interfaces/UnitAI.js");
Engine.LoadComponentScript("interfaces/Upgrade.js");
Engine.LoadComponentScript("interfaces/Upkeep.js");
Engine.LoadComponentScript("interfaces/BuildingAI.js");
Engine.LoadComponentScript("GuiInterface.js");

Resources = {
	"GetCodes": () => ["food", "metal", "stone", "wood"],
	"GetNames": () => ({
		"food": "Food",
		"metal": "Metal",
		"stone": "Stone",
		"wood": "Wood"
	}),
	"GetResource": resource => ({
		"aiAnalysisInfluenceGroup":
			resource == "food" ? "ignore" :
				resource == "wood" ? "abundant" : "sparse"
	})
};

var cmp = ConstructComponent(SYSTEM_ENTITY, "GuiInterface");


AddMock(SYSTEM_ENTITY, IID_Barter, {
	"GetPrices": function() {
		return {
			"buy": { "food": 150 },
			"sell": { "food": 25 }
		};
	}
});

AddMock(SYSTEM_ENTITY, IID_EndGameManager, {
	"GetVictoryConditions": () => ["conquest", "wonder"],
	"GetAlliedVictory": function() { return false; }
});

AddMock(SYSTEM_ENTITY, IID_PlayerManager, {
	"GetNumPlayers": function() { return 2; },
	"GetPlayerByID": function(id) { TS_ASSERT(id === 0 || id === 1); return 100 + id; },
	"GetMaxWorldPopulation": function() {}
});

AddMock(SYSTEM_ENTITY, IID_RangeManager, {
	"GetLosVisibility": function(ent, player) { return "visible"; },
	"GetLosCircular": function() { return false; }
});

AddMock(SYSTEM_ENTITY, IID_TemplateManager, {
	"GetCurrentTemplateName": function(ent) { return "example"; },
	"GetTemplate": function(name) { return ""; }
});

AddMock(SYSTEM_ENTITY, IID_PopulationCapManager, {
	"GetPopulationCapType": function() { return "player"; },
	"GetPopulationCap": function() { return 200; }
});

AddMock(SYSTEM_ENTITY, IID_Timer, {
	"GetTime": function() { return 0; },
	"SetTimeout": function(ent, iid, funcname, time, data) { return 0; }
});

AddMock(100, IID_Player, {
	"entity": 100,
	"GetColor": function() { return { "r": 1, "g": 1, "b": 1, "a": 1 }; },
	"CanControlAllUnits": function() { return false; },
	"GetPopulationCount": function() { return 10; },
	"GetPopulationLimit": function() { return 20; },
	"GetMaxPopulation": function() { return 200; },
	"GetResourceCounts": function() { return { "food": 100 }; },
	"GetResourceGatherers": function() { return { "food": 1 }; },
	"GetPanelEntities": function() { return []; },
	"IsTrainingBlocked": function() { return false; },
	"GetState": function() { return "active"; },
	"GetCheatsEnabled": function() { return false; },
	"GetDisabledTemplates": function() { return {}; },
	"GetDisabledTechnologies": function() { return {}; },
	"CanBarter": function() { return false; },
	"GetSpyCostMultiplier": function() { return 1; }
});

AddMock(100, IID_Diplomacy, {
	"GetTeam": () => -1,
	"IsTeamLocked": () => false,
	"GetDiplomacy": () => [-1, 1],
	"IsAlly": () => false,
	"IsMutualAlly": () => false,
	"IsNeutral": () => false,
	"IsEnemy": () => true,
	"HasSharedDropsites": () => false,
	"HasSharedLos": () => false,
});

AddMock(100, IID_Identity, {
	"GetName": function() { return "Player 1"; },
	"GetCiv": function() { return "gaia"; },
	"GetRankTechName": function() { return undefined; }
});

AddMock(100, IID_EntityLimits, {
	"GetLimits": function() { return { "Foo": 10 }; },
	"GetCounts": function() { return { "Foo": 5 }; },
	"GetLimitChangers": function() { return { "Foo": {} }; },
	"GetMatchCounts": function() { return { "Bar": 0 }; }
});

AddMock(100, IID_TechnologyManager, {
	"IsTechnologyResearched": tech => tech == "phase_village",
	"GetQueuedResearch": () => new Map(),
	"GetStartedTechs": () => new Set(),
	"GetResearchedTechs": () => new Set(),
	"GetClassCounts": () => ({}),
	"GetTypeCountsByClass": () => ({})
});

AddMock(100, IID_StatisticsTracker, {
	"GetBasicStatistics": function() {
		return {
			"resourcesGathered": {
				"food": 100,
				"wood": 0,
				"metal": 0,
				"stone": 0,
				"vegetarianFood": 0
			},
			"percentMapExplored": 10
		};
	},
	"GetSequences": function() {
		return {
			"unitsTrained": [0, 10],
			"unitsLost": [0, 42],
			"buildingsConstructed": [1, 3],
			"buildingsCaptured": [3, 7],
			"buildingsLost": [3, 10],
			"civCentresBuilt": [4, 10],
			"resourcesGathered": {
				"food": [5, 100],
				"wood": [0, 0],
				"metal": [0, 0],
				"stone": [0, 0],
				"vegetarianFood": [0, 0]
			},
			"treasuresCollected": [1, 20],
			"lootCollected": [0, 2],
			"percentMapExplored": [0, 10],
			"teamPercentMapExplored": [0, 10],
			"percentMapControlled": [0, 10],
			"teamPercentMapControlled": [0, 10],
			"peakPercentOfMapControlled": [0, 10],
			"teamPeakPercentOfMapControlled": [0, 10]
		};
	},
	"IncreaseTrainedUnitsCounter": function() { return 1; },
	"IncreaseConstructedBuildingsCounter": function() { return 1; },
	"IncreaseBuiltCivCentresCounter": function() { return 1; }
});

AddMock(101, IID_Player, {
	"entity": 101,
	"GetColor": function() { return { "r": 1, "g": 0, "b": 0, "a": 1 }; },
	"CanControlAllUnits": function() { return true; },
	"GetPopulationCount": function() { return 40; },
	"GetPopulationLimit": function() { return 30; },
	"GetMaxPopulation": function() { return 300; },
	"GetResourceCounts": function() { return { "food": 200 }; },
	"GetResourceGatherers": function() { return { "food": 3 }; },
	"GetPanelEntities": function() { return []; },
	"IsTrainingBlocked": function() { return false; },
	"GetState": function() { return "active"; },
	"GetCheatsEnabled": function() { return false; },
	"GetDisabledTemplates": function() { return {}; },
	"GetDisabledTechnologies": function() { return {}; },
	"CanBarter": function() { return false; },
	"GetSpyCostMultiplier": function() { return 1; },
});

AddMock(101, IID_Diplomacy, {
	"GetTeam": () => -1,
	"IsTeamLocked": () => false,
	"GetDiplomacy": () => [-1, 1],
	"IsAlly": () => true,
	"IsMutualAlly": () => false,
	"IsNeutral": () => false,
	"IsEnemy": () => false,
	"HasSharedDropsites": () => false,
	"HasSharedLos": () => false,
});

AddMock(101, IID_Identity, {
	"GetName": function() { return "Player 2"; },
	"GetCiv": function() { return "mace"; },
	"GetRankTechName": function() { return undefined; }
});

AddMock(101, IID_EntityLimits, {
	"GetLimits": function() { return { "Bar": 20 }; },
	"GetCounts": function() { return { "Bar": 0 }; },
	"GetLimitChangers": function() { return { "Bar": {} }; },
	"GetMatchCounts": function() { return { "Foo": 0 }; }
});

AddMock(101, IID_TechnologyManager, {
	"IsTechnologyResearched": tech => tech == "phase_village",
	"GetQueuedResearch": () => new Map(),
	"GetStartedTechs": () => new Set(),
	"GetResearchedTechs": () => new Set(),
	"GetClassCounts": () => ({}),
	"GetTypeCountsByClass": () => ({})
});

AddMock(101, IID_StatisticsTracker, {
	"GetBasicStatistics": function() {
		return {
			"resourcesGathered": {
				"food": 100,
				"wood": 0,
				"metal": 0,
				"stone": 0,
				"vegetarianFood": 0
			},
			"percentMapExplored": 10
		};
	},
	"GetSequences": function() {
		return {
			"unitsTrained": [0, 10],
			"unitsLost": [0, 9],
			"buildingsConstructed": [0, 5],
			"buildingsCaptured": [0, 7],
			"buildingsLost": [0, 4],
			"civCentresBuilt": [0, 1],
			"resourcesGathered": {
				"food": [0, 100],
				"wood": [0, 0],
				"metal": [0, 0],
				"stone": [0, 0],
				"vegetarianFood": [0, 0]
			},
			"treasuresCollected": [0, 0],
			"lootCollected": [0, 0],
			"percentMapExplored": [0, 10],
			"teamPercentMapExplored": [0, 10],
			"percentMapControlled": [0, 10],
			"teamPercentMapControlled": [0, 10],
			"peakPercentOfMapControlled": [0, 10],
			"teamPeakPercentOfMapControlled": [0, 10]
		};
	},
	"IncreaseTrainedUnitsCounter": function() { return 1; },
	"IncreaseConstructedBuildingsCounter": function() { return 1; },
	"IncreaseBuiltCivCentresCounter": function() { return 1; }
});

// Note: property order matters when using TS_ASSERT_UNEVAL_EQUALS,
//	because uneval preserves property order. So make sure this object
//	matches the ordering in GuiInterface.
TS_ASSERT_UNEVAL_EQUALS(cmp.GetSimulationState(), {
	"players": [
		{
			"name": "Player 1",
			"civ": "gaia",
			"color": { "r": 1, "g": 1, "b": 1, "a": 1 },
			"entity": 100,
			"controlsAll": false,
			"popCount": 10,
			"popLimit": 20,
			"popMax": 200,
			"panelEntities": [],
			"resourceCounts": { "food": 100 },
			"resourceGatherers": { "food": 1 },
			"trainingBlocked": false,
			"state": "active",
			"team": -1,
			"teamLocked": false,
			"cheatsEnabled": false,
			"disabledTemplates": {},
			"disabledTechnologies": {},
			"hasSharedDropsites": false,
			"hasSharedLos": false,
			"spyCostMultiplier": 1,
			"phase": "village",
			"isAlly": [false, false],
			"isMutualAlly": [false, false],
			"isNeutral": [false, false],
			"isEnemy": [true, true],
			"entityLimits": { "Foo": 10 },
			"entityCounts": { "Foo": 5 },
			"matchEntityCounts": { "Bar": 0 },
			"entityLimitChangers": { "Foo": {} },
			"researchQueued": new Map(),
			"researchedTechs": new Set(),
			"classCounts": {},
			"typeCountsByClass": {},
			"canBarter": false,
			"barterPrices": {
				"buy": { "food": 150 },
				"sell": { "food": 25 }
			},
			"statistics": {
				"resourcesGathered": {
					"food": 100,
					"wood": 0,
					"metal": 0,
					"stone": 0,
					"vegetarianFood": 0
				},
				"percentMapExplored": 10
			}
		},
		{
			"name": "Player 2",
			"civ": "mace",
			"color": { "r": 1, "g": 0, "b": 0, "a": 1 },
			"entity": 101,
			"controlsAll": true,
			"popCount": 40,
			"popLimit": 30,
			"popMax": 300,
			"panelEntities": [],
			"resourceCounts": { "food": 200 },
			"resourceGatherers": { "food": 3 },
			"trainingBlocked": false,
			"state": "active",
			"team": -1,
			"teamLocked": false,
			"cheatsEnabled": false,
			"disabledTemplates": {},
			"disabledTechnologies": {},
			"hasSharedDropsites": false,
			"hasSharedLos": false,
			"spyCostMultiplier": 1,
			"phase": "village",
			"isAlly": [true, true],
			"isMutualAlly": [false, false],
			"isNeutral": [false, false],
			"isEnemy": [false, false],
			"entityLimits": { "Bar": 20 },
			"entityCounts": { "Bar": 0 },
			"matchEntityCounts": { "Foo": 0 },
			"entityLimitChangers": { "Bar": {} },
			"researchQueued": new Map(),
			"researchedTechs": new Set(),
			"classCounts": {},
			"typeCountsByClass": {},
			"canBarter": false,
			"barterPrices": {
				"buy": { "food": 150 },
				"sell": { "food": 25 }
			},
			"statistics": {
				"resourcesGathered": {
					"food": 100,
					"wood": 0,
					"metal": 0,
					"stone": 0,
					"vegetarianFood": 0
				},
				"percentMapExplored": 10
			}
		}
	],
	"circularMap": false,
	"timeElapsed": 0,
	"victoryConditions": ["conquest", "wonder"],
	"alliedVictory": false,
	"populationCapType": "player",
	"populationCap": 200
});

TS_ASSERT_UNEVAL_EQUALS(cmp.GetExtendedSimulationState(), {
	"players": [
		{
			"name": "Player 1",
			"civ": "gaia",
			"color": { "r": 1, "g": 1, "b": 1, "a": 1 },
			"entity": 100,
			"controlsAll": false,
			"popCount": 10,
			"popLimit": 20,
			"popMax": 200,
			"panelEntities": [],
			"resourceCounts": { "food": 100 },
			"resourceGatherers": { "food": 1 },
			"trainingBlocked": false,
			"state": "active",
			"team": -1,
			"teamLocked": false,
			"cheatsEnabled": false,
			"disabledTemplates": {},
			"disabledTechnologies": {},
			"hasSharedDropsites": false,
			"hasSharedLos": false,
			"spyCostMultiplier": 1,
			"phase": "village",
			"isAlly": [false, false],
			"isMutualAlly": [false, false],
			"isNeutral": [false, false],
			"isEnemy": [true, true],
			"entityLimits": { "Foo": 10 },
			"entityCounts": { "Foo": 5 },
			"matchEntityCounts": { "Bar": 0 },
			"entityLimitChangers": { "Foo": {} },
			"researchQueued": new Map(),
			"researchedTechs": new Set(),
			"classCounts": {},
			"typeCountsByClass": {},
			"canBarter": false,
			"barterPrices": {
				"buy": { "food": 150 },
				"sell": { "food": 25 }
			},
			"statistics": {
				"resourcesGathered": {
					"food": 100,
					"wood": 0,
					"metal": 0,
					"stone": 0,
					"vegetarianFood": 0
				},
				"percentMapExplored": 10
			},
			"sequences": {
				"unitsTrained": [0, 10],
				"unitsLost": [0, 42],
				"buildingsConstructed": [1, 3],
				"buildingsCaptured": [3, 7],
				"buildingsLost": [3, 10],
				"civCentresBuilt": [4, 10],
				"resourcesGathered": {
					"food": [5, 100],
					"wood": [0, 0],
					"metal": [0, 0],
					"stone": [0, 0],
					"vegetarianFood": [0, 0]
				},
				"treasuresCollected": [1, 20],
				"lootCollected": [0, 2],
				"percentMapExplored": [0, 10],
				"teamPercentMapExplored": [0, 10],
				"percentMapControlled": [0, 10],
				"teamPercentMapControlled": [0, 10],
				"peakPercentOfMapControlled": [0, 10],
				"teamPeakPercentOfMapControlled": [0, 10]
			}
		},
		{
			"name": "Player 2",
			"civ": "mace",
			"color": { "r": 1, "g": 0, "b": 0, "a": 1 },
			"entity": 101,
			"controlsAll": true,
			"popCount": 40,
			"popLimit": 30,
			"popMax": 300,
			"panelEntities": [],
			"resourceCounts": { "food": 200 },
			"resourceGatherers": { "food": 3 },
			"trainingBlocked": false,
			"state": "active",
			"team": -1,
			"teamLocked": false,
			"cheatsEnabled": false,
			"disabledTemplates": {},
			"disabledTechnologies": {},
			"hasSharedDropsites": false,
			"hasSharedLos": false,
			"spyCostMultiplier": 1,
			"phase": "village",
			"isAlly": [true, true],
			"isMutualAlly": [false, false],
			"isNeutral": [false, false],
			"isEnemy": [false, false],
			"entityLimits": { "Bar": 20 },
			"entityCounts": { "Bar": 0 },
			"matchEntityCounts": { "Foo": 0 },
			"entityLimitChangers": { "Bar": {} },
			"researchQueued": new Map(),
			"researchedTechs": new Set(),
			"classCounts": {},
			"typeCountsByClass": {},
			"canBarter": false,
			"barterPrices": {
				"buy": { "food": 150 },
				"sell": { "food": 25 }
			},
			"statistics": {
				"resourcesGathered": {
					"food": 100,
					"wood": 0,
					"metal": 0,
					"stone": 0,
					"vegetarianFood": 0
				},
				"percentMapExplored": 10
			},
			"sequences": {
				"unitsTrained": [0, 10],
				"unitsLost": [0, 9],
				"buildingsConstructed": [0, 5],
				"buildingsCaptured": [0, 7],
				"buildingsLost": [0, 4],
				"civCentresBuilt": [0, 1],
				"resourcesGathered": {
					"food": [0, 100],
					"wood": [0, 0],
					"metal": [0, 0],
					"stone": [0, 0],
					"vegetarianFood": [0, 0]
				},
				"treasuresCollected": [0, 0],
				"lootCollected": [0, 0],
				"percentMapExplored": [0, 10],
				"teamPercentMapExplored": [0, 10],
				"percentMapControlled": [0, 10],
				"teamPercentMapControlled": [0, 10],
				"peakPercentOfMapControlled": [0, 10],
				"teamPeakPercentOfMapControlled": [0, 10]
			}
		}
	],
	"circularMap": false,
	"timeElapsed": 0,
	"victoryConditions": ["conquest", "wonder"],
	"alliedVictory": false,
	"populationCapType": "player",
	"populationCap": 200
});


AddMock(10, IID_Builder, {
	"GetEntitiesList": function() {
		return ["test1", "test2"];
	},
});

AddMock(10, IID_Health, {
	"GetHitpoints": function() { return 50; },
	"GetMaxHitpoints": function() { return 60; },
	"IsRepairable": function() { return false; },
	"IsUnhealable": function() { return false; }
});

AddMock(10, IID_Identity, {
	"GetClassesList": function() { return ["class1", "class2"]; },
	"GetRank": function() { return "foo"; },
	"GetSelectionGroupName": function() { return "Selection Group Name"; },
	"HasClass": function() { return true; },
	"IsUndeletable": function() { return false; },
	"IsControllable": function() { return true; },
	"GetRankTechName": function() { return undefined; }
});

AddMock(10, IID_Position, {
	"GetTurretParent": function() { return INVALID_ENTITY; },
	"GetPosition": function() {
		return { "x": 1, "y": 2, "z": 3 };
	},
	"IsInWorld": function() {
		return true;
	}
});

AddMock(10, IID_ResourceTrickle, {
	"GetInterval": () => 1250,
	"GetRates": () => ({ "food": 2, "wood": 3, "stone": 5, "metal": 9 })
});

// Note: property order matters when using TS_ASSERT_UNEVAL_EQUALS,
//	because uneval preserves property order. So make sure this object
//	matches the ordering in GuiInterface.
TS_ASSERT_UNEVAL_EQUALS(cmp.GetEntityState(-1, 10), {
	"id": 10,
	"player": INVALID_PLAYER,
	"template": "example",
	"identity": {
		"rank": "foo",
		"rankTechName": undefined,
		"classes": ["class1", "class2"],
		"selectionGroupName": "Selection Group Name",
		"canDelete": true,
		"controllable": true,
	},
	"position": { "x": 1, "y": 2, "z": 3 },
	"hitpoints": 50,
	"maxHitpoints": 60,
	"needsRepair": false,
	"needsHeal": true,
	"builder": true,
	"visibility": "visible",
	"isBarterMarket": true,
	"resourceTrickle": {
		"interval": 1250,
		"rates": { "food": 2, "wood": 3, "stone": 5, "metal": 9 }
	}
});
