Engine.LoadHelperScript("Player.js");
Engine.LoadHelperScript("ValueModification.js");
Engine.LoadComponentScript("interfaces/Auras.js");
Engine.LoadComponentScript("interfaces/Capturable.js");
Engine.LoadComponentScript("interfaces/Diplomacy.js");
Engine.LoadComponentScript("interfaces/GarrisonHolder.js");
Engine.LoadComponentScript("interfaces/StatisticsTracker.js");
Engine.LoadComponentScript("interfaces/ModifiersManager.js");
Engine.LoadComponentScript("interfaces/TerritoryDecay.js");
Engine.LoadComponentScript("interfaces/Timer.js");
Engine.LoadComponentScript("Capturable.js");

var testData = {
	"structure": 20,
	"playerID": 1,
	"regenRate": 2,
	"garrisonedEntities": [30, 31, 32, 33],
	"garrisonRegenRate": 2,
	"decay": false,
	"decayRate": 30,
	"maxCapturePoints": 3000,
	"neighbours": [20, 0, 20, 10]
};

const testCaptureAttacks = {
	"30": 1.5,
	"32": 2.0
};

function testCapturable(data, test_function)
{
	ResetState();

	AddMock(SYSTEM_ENTITY, IID_Timer, {
		"SetInterval": (ent, iid, funcname, time, repeattime, payload) => {},
		"CancelTimer": timer => {}
	});

	AddMock(data.structure, IID_Ownership, {
		"GetOwner": () => data.playerID,
		"SetOwner": id => {}
	});

	AddMock(data.structure, IID_GarrisonHolder, {
		"GetEntities": () => data.garrisonedEntities
	});

	AddMock(data.structure, IID_Fogging, {
		"Activate": () => {}
	});

	AddMock(10, IID_Diplomacy, {
		"IsEnemy": id => id != 0
	});

	AddMock(11, IID_Diplomacy, {
		"IsEnemy": id => id != 1 && id != 2
	});

	AddMock(12, IID_Diplomacy, {
		"IsEnemy": id => id != 1 && id != 2
	});

	AddMock(13, IID_Diplomacy, {
		"IsEnemy": id => id != 3
	});

	AddMock(SYSTEM_ENTITY, IID_PlayerManager, {
		"GetNumPlayers": () => 4,
		"GetPlayerByID": id => 10 + id
	});

	AddMock(data.structure, IID_StatisticsTracker, {
		"LostEntity": () => {},
		"CapturedBuilding": () => {}
	});

	const cmpCapturable = ConstructComponent(data.structure, "Capturable", {
		"CapturePoints": data.maxCapturePoints,
		"RegenRate": data.regenRate,
		"GarrisonRegenRate": data.garrisonRegenRate
	});

	AddMock(data.structure, IID_TerritoryDecay, {
		"IsDecaying": () => data.decay,
		"GetDecayRate": () => data.decayRate,
		"GetConnectedNeighbours": () => data.neighbours
	});

	let regenRate = data.regenRate;
	for (const entity of data.garrisonedEntities)
	{
		if (testCaptureAttacks[entity] === undefined)
			continue;
		AddMock(entity, IID_Attack, {
			"GetAttackEffectsData": (type) => {
				return type === "Capture" ? { "Capture": testCaptureAttacks[entity] } : undefined;
			},
		});
		regenRate += testCaptureAttacks[entity] * data.garrisonRegenRate;
	}

	TS_ASSERT_EQUALS(cmpCapturable.GetRegenRate(), regenRate);
	test_function(cmpCapturable);
	Engine.PostMessage = (ent, iid, message) => {};
}

// Tests initialisation of the capture points when the entity is created
testCapturable(testData, cmpCapturable => {
	Engine.PostMessage = function(ent, iid, message) {
		TS_ASSERT_UNEVAL_EQUALS(message, { "regenerating": true, "regenRate": cmpCapturable.GetRegenRate(), "territoryDecay": 0 });
	};
	cmpCapturable.OnOwnershipChanged({ "from": INVALID_PLAYER, "to": testData.playerID });
	TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.GetCapturePoints(), [0, 3000, 0, 0]);
});

// Tests if the message is sent when capture points change
testCapturable(testData, cmpCapturable => {
	cmpCapturable.SetCapturePoints([0, 2000, 0, 1000]);
	TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.GetCapturePoints(), [0, 2000, 0, 1000]);
	Engine.PostMessage = function(ent, iid, message)
	{
		TS_ASSERT_UNEVAL_EQUALS(message, { "capturePoints": [0, 2000, 0, 1000] });
	};
	cmpCapturable.RegisterCapturePointsChanged();
});

// Tests reducing capture points (after a capture attack or a decay)
testCapturable(testData, cmpCapturable => {
	cmpCapturable.SetCapturePoints([0, 2000, 0, 1000]);
	cmpCapturable.CheckTimer();
	Engine.PostMessage = function(ent, iid, message) {
		if (iid == MT_CapturePointsChanged)
			TS_ASSERT_UNEVAL_EQUALS(message, { "capturePoints": [0, 2000 - 100, 0, 1000 + 100] });
		if (iid == MT_CaptureRegenStateChanged)
			TS_ASSERT_UNEVAL_EQUALS(message, { "regenerating": true, "regenRate": cmpCapturable.GetRegenRate(), "territoryDecay": 0 });
	};
	TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.Reduce(100, 3), 100);
	TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.GetCapturePoints(), [0, 2000 - 100, 0, 1000 + 100]);
});

// Tests reducing capture points (after a capture attack or a decay)
testCapturable(testData, cmpCapturable => {
	cmpCapturable.SetCapturePoints([0, 2000, 0, 1000]);
	cmpCapturable.CheckTimer();
	TS_ASSERT_EQUALS(cmpCapturable.Reduce(2500, 3), 2000);
	TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.GetCapturePoints(), [0, 0, 0, 3000]);
});

function testRegen(data, capturePointsIn, capturePointsOut, regenerating)
{
	testCapturable(data, cmpCapturable => {
		cmpCapturable.SetCapturePoints(capturePointsIn);
		cmpCapturable.CheckTimer();
		Engine.PostMessage = function(ent, iid, message) {
			if (iid == MT_CaptureRegenStateChanged)
				TS_ASSERT_UNEVAL_EQUALS(message.regenerating, regenerating);
		};
		cmpCapturable.TimerTick(capturePointsIn);
		TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.GetCapturePoints(), capturePointsOut);
	});
}

// With our testData, the total regen rate is 9. That should be taken from the enemies
testRegen(testData, [12, 2950, 2, 36], [7.5, 2959, 2, 31.5], true);
testRegen(testData, [0, 2994, 2, 4], [0, 2998, 2, 0], true);
testRegen(testData, [0, 2998, 2, 0], [0, 2998, 2, 0], false);

// If the regeneration rate becomes negative, capture points are given in favour of gaia
testData.regenRate = -19;
// With our testData, the total regen rate is -12. That should be taken from all players to gaia
testRegen(testData, [100, 2800, 50, 50], [112, 2796, 46, 46], true);
testData.regenRate = 2;

function testDecay(data, capturePointsIn, capturePointsOut)
{
	testCapturable(data, cmpCapturable => {
		cmpCapturable.SetCapturePoints(capturePointsIn);
		cmpCapturable.CheckTimer();
		Engine.PostMessage = function(ent, iid, message) {
			if (iid == MT_CaptureRegenStateChanged)
				TS_ASSERT_UNEVAL_EQUALS(message.territoryDecay, data.decayRate);
		};
		cmpCapturable.TimerTick();
		TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.GetCapturePoints(), capturePointsOut);
	});
}
testData.decay = true;
testData.regenRate = 8;
testData.garrisonRegenRate = 4;
// With our testData, the decay rate is 30, that should be given to all neighbours with weights [20/50, 0, 20/50, 10/50], then it regens.
testDecay(testData, [2900, 35, 10, 55], [2901, 27, 22, 50]);
testData.decay = false;

// Tests Reduce
function testReduce(data, amount, player, taken)
{
	testCapturable(data, cmpCapturable => {
		cmpCapturable.SetCapturePoints([0, 2000, 0, 1000]);
		cmpCapturable.CheckTimer();
		TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.Reduce(amount, player), taken);
	});
}

testReduce(testData, 50, 3, 50);
testReduce(testData, 50, 2, 50);
testReduce(testData, 50, 1, 50);
testReduce(testData, -50, 3, 0);
testReduce(testData, 50, 0, 50);
testReduce(testData, 0, 3, 0);
testReduce(testData, 1500, 3, 1500);
testReduce(testData, 2000, 3, 2000);
testReduce(testData, 3000, 3, 2000);

// Test defeated player
testCapturable(testData, cmpCapturable => {
	cmpCapturable.SetCapturePoints([500, 1000, 0, 250]);
	cmpCapturable.OnGlobalPlayerDefeated({ "playerId": 3 });
	TS_ASSERT_UNEVAL_EQUALS(cmpCapturable.GetCapturePoints(), [750, 1000, 0, 0]);
});
