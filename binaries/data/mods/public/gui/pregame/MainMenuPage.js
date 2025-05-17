/**
 * This is the handler that coordinates all other handlers on this GUI page.
 */
class MainMenuPage
{
	constructor(closePageCallback, data, hotloadData, mainMenuItems, backgroundLayerData,
		projectInformation, communityButtons)
	{
		this.backgroundHandler = new BackgroundHandler(pickRandom(backgroundLayerData));
		this.menuHandler = new MainMenuItemHandler(closePageCallback, mainMenuItems);
		this.splashScreenHandler = new SplashScreenHandler(data, hotloadData && hotloadData.splashScreenHandler);

		this.initMusic();
		this.initProjectInformation(projectInformation);
		this.initCommunityButton(communityButtons);
	}

	initMusic()
	{
		globalThis.initMusic();
		globalThis.music.setState(global.music.states.MENU);
	}

	initProjectInformation(projectInformation)
	{
		for (const objectName in projectInformation)
			for (const propertyName in projectInformation[objectName])
				Engine.GetGUIObjectByName(objectName)[propertyName] = projectInformation[objectName][propertyName];
	}

	initCommunityButton(communityButtons)
	{
		const buttons = Engine.GetGUIObjectByName("communityButtons").children;

		communityButtons.forEach((buttonInfo, i) => {
			const button = buttons[i];
			button.hidden = false;
			for (const propertyName in buttonInfo)
				button[propertyName] = buttonInfo[propertyName];
		});

		if (buttons.length < communityButtons.length)
			error("GUI page has space for " + buttons.length + " community buttons, but " + menuItems.length + " items are provided!");
	}

	getHotloadData()
	{
		return {
			"splashScreenHandler": this.splashScreenHandler.getHotloadData()
		};
	}
}
