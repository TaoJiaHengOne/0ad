Resources = {
	"BuildSchema": () => {
		let schema = "";
		for (const res of ["food", "metal"])
		{
			for (const subtype in ["meat", "grain"])
				schema += "<value>" + res + "." + subtype + "</value>";
			schema += "<value> treasure." + res + "</value>";
		}
		return "<choice>" + schema + "</choice>";
	}
};

Engine.LoadHelperScript("Player.js");
Engine.LoadComponentScript("interfaces/StatisticsTracker.js");
Engine.LoadComponentScript("interfaces/Treasure.js");
Engine.LoadComponentScript("interfaces/Trigger.js");
Engine.LoadComponentScript("Treasure.js");
Engine.LoadComponentScript("Trigger.js");

Engine.RegisterGlobal("ApplyValueModificationsToEntity", (prop, oVal, ent) => oVal);
ConstructComponent(SYSTEM_ENTITY, "Trigger", {});

const entity = 11;
const treasurer = 12;
const treasurerOwner = 1;

const cmpTreasure = ConstructComponent(entity, "Treasure", {
	"CollectTime": "1000",
	"Resources": {
		"Food": "10"
	}
});
cmpTreasure.OnOwnershipChanged({ "to": 0 });

TS_ASSERT(!cmpTreasure.Reward(treasurer));

AddMock(treasurer, IID_Ownership, {
	"GetOwner": () => treasurerOwner
});

AddMock(SYSTEM_ENTITY, IID_PlayerManager, {
	"GetPlayerByID": (id) => treasurerOwner
});

const cmpPlayer = AddMock(treasurerOwner, IID_Player, {
	"AddResources": (type, amount) => {},
	"GetPlayerID": () => treasurerOwner
});
const spy = new Spy(cmpPlayer, "AddResources");
TS_ASSERT(cmpTreasure.Reward(treasurer));
TS_ASSERT_EQUALS(spy._called, 1);

// Don't allow collecting twice.
TS_ASSERT(!cmpTreasure.Reward(treasurer));
TS_ASSERT_EQUALS(spy._called, 1);
