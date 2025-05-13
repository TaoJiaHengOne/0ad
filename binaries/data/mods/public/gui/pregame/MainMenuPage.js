class MusicHandler
{
	constructor()
	{
		initMusic();
		global.music.setState(global.music.states.MENU);
	}
}

class ProjectInformationHandler
{
	constructor(projectInformation)
	{
		for (const objectName in projectInformation)
			for (const propertyName in projectInformation[objectName])
				Engine.GetGUIObjectByName(objectName)[propertyName] = projectInformation[objectName][propertyName];
	}
}

class CommunityButtonHandler
{
	constructor(communityButtons)
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
}

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

		new MusicHandler();
		new ProjectInformationHandler(projectInformation);
		new CommunityButtonHandler(communityButtons);
	}

	getHotloadData()
	{
		return {
			"splashScreenHandler": this.splashScreenHandler.getHotloadData()
		};
	}
}
