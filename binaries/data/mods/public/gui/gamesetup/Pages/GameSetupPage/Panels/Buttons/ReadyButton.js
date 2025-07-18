class ReadyButton
{
	constructor(setupWindow)
	{
		this.readyController = setupWindow.controls.readyController;

		this.hidden = undefined;

		this.readyButtonPressHandlers = new Set();

		this.readyButton = Engine.GetGUIObjectByName("readyButton");
		this.readyButton.onPress = this.onPress.bind(this);
		this.readyButton.onPressRight = this.onPressRight.bind(this);

		setupWindow.controls.playerAssignmentsController.registerPlayerAssignmentsChangeHandler(this.onPlayerAssignmentsChange.bind(this));
		setupWindow.controls.netMessages.registerNetMessageHandler("netstatus", this.onNetStatusMessage.bind(this));

		if (g_IsController && g_IsNetworked)
			this.readyController.setReady(this.readyController.StayReady, true);
	}

	onNetStatusMessage(message)
	{
		if (message.status == "disconnected")
			this.readyButton.enabled = false;
	}

	onPlayerAssignmentsChange()
	{
		const playerAssignment = g_PlayerAssignments[Engine.GetPlayerGUID()];
		const hidden = g_IsController || !playerAssignment || playerAssignment.player == -1;

		if (!hidden)
		{
			this.readyButton.caption = this.Caption[playerAssignment.status];
			this.readyButton.tooltip = this.Tooltip[playerAssignment.status];
		}

		if (hidden == this.hidden)
			return;

		this.hidden = hidden;
		this.readyButton.hidden = hidden;
	}

	registerReadyButtonPressHandler(handler)
	{
		this.readyButtonPressHandlers.add(handler);
	}

	onPress()
	{
		const newState =
			(g_PlayerAssignments[Engine.GetPlayerGUID()].status + 1) % (this.readyController.StayReady + 1);

		for (const handler of this.readyButtonPressHandlers)
			handler(newState);

		this.readyController.setReady(newState, true);
	}

	onPressRight()
	{
		if (g_PlayerAssignments[Engine.GetPlayerGUID()].status != this.readyController.NotReady)
			this.readyController.setReady(this.readyController.NotReady, true);
	}
}

ReadyButton.prototype.Caption = [
	translate("I'm ready"),
	translate("Stay ready"),
	translate("I'm not ready!")
];

ReadyButton.prototype.Tooltip = [
	translate("State that you are ready to play."),
	translate("Stay ready even when the game settings change."),
	translate("State that you are not ready to play.")
];
