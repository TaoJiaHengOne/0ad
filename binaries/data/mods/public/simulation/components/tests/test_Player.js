Resources = {
	"GetCodes": () => ["food", "metal", "stone", "wood"],
	"GetTradableCodes": () => ["food", "metal", "stone", "wood"],
	"GetBarterableCodes": () => ["food", "metal", "stone", "wood"],
	"GetResource": () => ({}),
	"BuildSchema": (type) => {
		let schema = "";
		for (const res of Resources.GetCodes())
			schema +=
				"<optional>" +
					"<element name='" + res + "'>" +
						"<ref name='" + type + "'/>" +
					"</element>" +
				"</optional>";
		return "<interleave>" + schema + "</interleave>";
	}
};

Engine.LoadHelperScript("ValueModification.js");
Engine.LoadComponentScript("interfaces/Player.js");
Engine.LoadComponentScript("interfaces/Cost.js");
Engine.LoadComponentScript("interfaces/Foundation.js");
Engine.LoadComponentScript("interfaces/ModifiersManager.js");
Engine.LoadComponentScript("Player.js");

var cmpPlayer = ConstructComponent(10, "Player", {
	"SpyCostMultiplier": 1,
	"BarterMultiplier": {
		"Buy": {
			"wood": 1.0,
			"stone": 1.0,
			"metal": 1.0
		},
		"Sell": {
			"wood": 1.0,
			"stone": 1.0,
			"metal": 1.0
		}
	},
	"Formations": { "_string": "" },
});

var playerID = 1;
cmpPlayer.SetPlayerID(playerID);
TS_ASSERT_EQUALS(cmpPlayer.GetPlayerID(), playerID);

TS_ASSERT_EQUALS(cmpPlayer.GetPopulationCount(), 0);
TS_ASSERT_EQUALS(cmpPlayer.GetPopulationLimit(), 0);

TS_ASSERT_EQUALS(cmpPlayer.GetSpyCostMultiplier(), 1);
TS_ASSERT_UNEVAL_EQUALS(cmpPlayer.GetBarterMultiplier(), {
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
});

AddMock(60, IID_Identity, {
	"GetClassesList": () => {},
	"HasClass": (cl) => true
});
AddMock(60, IID_Ownership);
AddMock(60, IID_Foundation, {});
cmpPlayer.OnGlobalOwnershipChanged({ "entity": 60, "from": INVALID_PLAYER, "to": playerID });
TS_ASSERT(!cmpPlayer.CanBarter());

AddMock(61, IID_Identity, {
	"GetClassesList": () => {},
	"HasClass": (cl) => false
});
cmpPlayer.OnGlobalOwnershipChanged({ "entity": 61, "from": INVALID_PLAYER, "to": playerID });
TS_ASSERT(!cmpPlayer.CanBarter());

AddMock(62, IID_Identity, {
	"GetClassesList": () => {},
	"HasClass": (cl) => true
});
cmpPlayer.OnGlobalOwnershipChanged({ "entity": 62, "from": INVALID_PLAYER, "to": playerID });
TS_ASSERT(cmpPlayer.CanBarter());

cmpPlayer.OnGlobalOwnershipChanged({ "entity": 62, "from": playerID, "to": INVALID_PLAYER });
TS_ASSERT(!cmpPlayer.CanBarter());
