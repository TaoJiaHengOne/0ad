Engine.LoadComponentScript("interfaces/EndGameManager.js");
Engine.LoadComponentScript("EndGameManager.js");

const cmpEndGameManager = ConstructComponent(SYSTEM_ENTITY, "EndGameManager");

const playerEnt1 = 1;
const wonderDuration = 2 * 60 * 1000;

AddMock(SYSTEM_ENTITY, IID_PlayerManager, {
	"GetNumPlayers": () => 4
});

AddMock(SYSTEM_ENTITY, IID_GuiInterface, {
	"DeleteTimeNotification": () => null,
	"AddTimeNotification": () => 1
});

AddMock(playerEnt1, IID_Player, {
	"GetName": () => "Player 1",
	"IsActive": () => true,
});

TS_ASSERT_EQUALS(cmpEndGameManager.skipAlliedVictoryCheck, true);
cmpEndGameManager.SetAlliedVictory(true);
TS_ASSERT_EQUALS(cmpEndGameManager.GetAlliedVictory(), true);
cmpEndGameManager.SetGameSettings({
	"victoryConditions": ["wonder"],
	"wonderDuration": wonderDuration
});
TS_ASSERT_EQUALS(cmpEndGameManager.skipAlliedVictoryCheck, false);
TS_ASSERT_UNEVAL_EQUALS(cmpEndGameManager.GetVictoryConditions(), ["wonder"]);
TS_ASSERT_EQUALS(cmpEndGameManager.GetGameSettings().wonderDuration, wonderDuration);
