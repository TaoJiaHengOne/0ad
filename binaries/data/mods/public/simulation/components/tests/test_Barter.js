Engine.LoadHelperScript("Player.js");
Engine.LoadComponentScript("interfaces/Barter.js");
Engine.LoadComponentScript("interfaces/Foundation.js");
Engine.LoadComponentScript("interfaces/Player.js");
Engine.LoadComponentScript("interfaces/StatisticsTracker.js");
Engine.LoadComponentScript("interfaces/Timer.js");
Engine.LoadComponentScript("Barter.js");

// truePrice. Use the same for each resource for those tests.
const truePrice = 110;

Resources = {
	"GetBarterableCodes": () => ["wood", "stone", "metal"],
	"GetResource": (resource) => ({ "truePrice": truePrice })
};

const playerID = 1;
const playerEnt = 11;

AddMock(SYSTEM_ENTITY, IID_PlayerManager, {
	"GetPlayerByID": id => playerEnt
});

let timerActivated = false;
let bought = 0;
let sold = 0;
const multiplier = {
	"buy": {
		"wood": 1.0,
		"stone": 1.0,
		"metal": 1.0
	},
	"sell": {
		"wood": 1.0,
		"stone": 1.0,
		"metal": 1.0
	}
};
const cmpBarter = ConstructComponent(SYSTEM_ENTITY, "Barter");

AddMock(SYSTEM_ENTITY, IID_Timer, {
	"CancelTimer": id => { timerActivated = false; },
	"SetInterval": (ent, iid, funcname, time, repeattime, data) => {
		TS_ASSERT_EQUALS(time, cmpBarter.RESTORE_TIMER_INTERVAL);
		TS_ASSERT_EQUALS(repeattime, cmpBarter.RESTORE_TIMER_INTERVAL);
		timerActivated = true;
		return 7;
	}
});

// Init
TS_ASSERT_EQUALS(cmpBarter.restoreTimer, undefined);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": 0, "stone": 0, "metal": 0 });

const cmpPlayer = AddMock(playerEnt, IID_Player, {
	"TrySubtractResources": amounts => {
		sold = amounts[Object.keys(amounts)[0]];
		return true;
	},
	"AddResource": (type, amount) => {
		bought = amount;
		return true;
	},
	"CanBarter": () => true,
	"GetBarterMultiplier": () => multiplier
});

// GetPrices
cmpBarter.priceDifferences = { "wood": 8, "stone": 0, "metal": 0 };
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.GetPrices(cmpPlayer).buy, {
	"wood": truePrice * (100 + 8 + cmpBarter.CONSTANT_DIFFERENCE) / 100,
	"stone": truePrice * (100 + cmpBarter.CONSTANT_DIFFERENCE) / 100,
	"metal": truePrice * (100 + cmpBarter.CONSTANT_DIFFERENCE) / 100
});
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.GetPrices(cmpPlayer).sell, {
	"wood": truePrice * (100 + 8 - cmpBarter.CONSTANT_DIFFERENCE) / 100,
	"stone": truePrice * (100 - cmpBarter.CONSTANT_DIFFERENCE) / 100,
	"metal": truePrice * (100 - cmpBarter.CONSTANT_DIFFERENCE) / 100
});

multiplier.buy.stone = 2.0;
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.GetPrices(cmpPlayer).buy, {
	"wood": truePrice * (100 + 8 + cmpBarter.CONSTANT_DIFFERENCE) / 100,
	"stone": truePrice * (100 + cmpBarter.CONSTANT_DIFFERENCE) * 2.0 / 100,
	"metal": truePrice * (100 + cmpBarter.CONSTANT_DIFFERENCE) / 100
});
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.GetPrices(cmpPlayer).sell, {
	"wood": truePrice * (100 + 8 - cmpBarter.CONSTANT_DIFFERENCE) / 100,
	"stone": truePrice * (100 - cmpBarter.CONSTANT_DIFFERENCE) / 100,
	"metal": truePrice * (100 - cmpBarter.CONSTANT_DIFFERENCE) / 100
});
multiplier.buy.stone = 1.0;

// ExchangeResources
// Price differences magnitude are caped by 99 - CONSTANT_DIFFERENCE.
// If we have a bigger DIFFERENCE_PER_DEAL than 99 - CONSTANT_DIFFERENCE at each deal we reach the cap.
TS_ASSERT(cmpBarter.DIFFERENCE_PER_DEAL < 99 - cmpBarter.CONSTANT_DIFFERENCE);

cmpBarter.priceDifferences = { "wood": 0, "stone": 0, "metal": 0 };
cmpBarter.ExchangeResources(playerID, "wood", "stone", 100);
TS_ASSERT_EQUALS(cmpBarter.restoreTimer, 7);
TS_ASSERT(timerActivated);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": -cmpBarter.DIFFERENCE_PER_DEAL, "stone": cmpBarter.DIFFERENCE_PER_DEAL, "metal": 0 });
TS_ASSERT_EQUALS(sold, 100);
TS_ASSERT_EQUALS(bought, Math.round(100 * (100 - cmpBarter.CONSTANT_DIFFERENCE + 0) / (100 + cmpBarter.CONSTANT_DIFFERENCE + 0)));

// Amount which is not 100 or 500 is invalid.
cmpBarter.priceDifferences = { "wood": 0, "stone": 0, "metal": 0 };
cmpBarter.ExchangeResources(playerID, "wood", "stone", 40);
bought = 0;
sold = 0;
timerActivated = false;
TS_ASSERT(!timerActivated);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": 0, "stone": 0, "metal": 0 });
TS_ASSERT_EQUALS(sold, 0);
TS_ASSERT_EQUALS(bought, 0);

// Amount which is NaN is invalid.
cmpBarter.priceDifferences = { "wood": 0, "stone": 0, "metal": 0 };
cmpBarter.ExchangeResources(playerID, "wood", "stone", NaN);
TS_ASSERT(!timerActivated);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": 0, "stone": 0, "metal": 0 });
TS_ASSERT_EQUALS(sold, 0);
TS_ASSERT_EQUALS(bought, 0);

cmpBarter.priceDifferences = { "wood": 0, "stone": 99 - cmpBarter.CONSTANT_DIFFERENCE, "metal": 0 };
cmpBarter.ExchangeResources(playerID, "wood", "stone", 100);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": -cmpBarter.DIFFERENCE_PER_DEAL, "stone": 99 - cmpBarter.CONSTANT_DIFFERENCE, "metal": 0 });
TS_ASSERT_EQUALS(bought, Math.round(100 * (100 - cmpBarter.CONSTANT_DIFFERENCE + 0) / (100 + cmpBarter.CONSTANT_DIFFERENCE + 99 - cmpBarter.CONSTANT_DIFFERENCE)));

cmpBarter.priceDifferences = { "wood": -99 + cmpBarter.CONSTANT_DIFFERENCE, "stone": 0, "metal": 0 };
cmpBarter.ExchangeResources(playerID, "wood", "stone", 100);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": -99 + cmpBarter.CONSTANT_DIFFERENCE, "stone": cmpBarter.DIFFERENCE_PER_DEAL, "metal": 0 });
TS_ASSERT_EQUALS(bought, Math.round(100 * (100 - cmpBarter.CONSTANT_DIFFERENCE - 99 + cmpBarter.CONSTANT_DIFFERENCE) / (100 + cmpBarter.CONSTANT_DIFFERENCE + 0)));

cmpBarter.priceDifferences = { "wood": 0, "stone": 0, "metal": 0 };
cmpBarter.restoreTimer = undefined;
timerActivated = false;
AddMock(playerEnt, IID_Player, {
	"TrySubtractResources": () => false,
	"AddResource": () => {},
	"CanBarter": () => true,
	"GetBarterMultiplier": () => multiplier
});
cmpBarter.ExchangeResources(playerID, "wood", "stone", 100);

TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": 0, "stone": 0, "metal": 0 });

DeleteMock(playerEnt, IID_Player);

// ProgressTimeout
// Price difference restoration magnitude is capped by DIFFERENCE_RESTORE.

AddMock(playerEnt, IID_Player, {
	"TrySubtractResources": () => true,
	"AddResource": () => {}
});

timerActivated = true;
cmpBarter.restoreTimer = 7;
cmpBarter.priceDifferences = { "wood": -cmpBarter.DIFFERENCE_RESTORE, "stone": -1 - cmpBarter.DIFFERENCE_RESTORE, "metal": cmpBarter.DIFFERENCE_RESTORE };
cmpBarter.ProgressTimeout();
TS_ASSERT_EQUALS(cmpBarter.restoreTimer, 7);
TS_ASSERT(timerActivated);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": 0, "stone": -1, "metal": 0 });

cmpBarter.restoreTimer = 7;
cmpBarter.priceDifferences = { "wood": -cmpBarter.DIFFERENCE_RESTORE, "stone": cmpBarter.DIFFERENCE_RESTORE / 2, "metal": cmpBarter.DIFFERENCE_RESTORE };
cmpBarter.ProgressTimeout();
TS_ASSERT_EQUALS(cmpBarter.restoreTimer, undefined);
TS_ASSERT(!timerActivated);
TS_ASSERT_UNEVAL_EQUALS(cmpBarter.priceDifferences, { "wood": 0, "stone": 0, "metal": 0 });
