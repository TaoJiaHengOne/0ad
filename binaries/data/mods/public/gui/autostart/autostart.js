function init(initData)
{
	// This doesn't use the autostart/ folder as those are intended for CLI commands and the duplication isn't big enough.

	this.settings = new GameSettings().init();

	this.settings.fromInitAttributes(initData.attribs);

	this.settings.launchGame(initData.playerAssignments, initData.storeReplay);
	
	Engine.SwitchGuiPage("page_loading.xml", {
		"attribs": this.settings.finalizedAttributes,
		"playerAssignments": initData.playerAssignments
	});
}
