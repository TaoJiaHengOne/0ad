/**
 * This class provides barter buttons.
 * This is instantiated once for the selection panels and once for the trade dialog.
 */
class BarterButtonManager
{
	constructor(panel)
	{
		if (!BarterButtonManager.IsAvailable(panel))
			throw "BarterButtonManager instantiated with no barterable resources or too few buttons!";

		// The player may be the owner of the selected market
		this.viewedPlayer = -1;

		const resourceCodes = g_ResourceData.GetBarterableCodes();
		this.selectedResource = resourceCodes[0];
		this.buttons = resourceCodes.map((resourceCode, i) =>
			new BarterButton(this, resourceCode, i, panel));

		panel.onPress = this.update.bind(this);
		panel.onRelease = this.update.bind(this);
	}

	setViewedPlayer(viewedPlayer)
	{
		this.viewedPlayer = viewedPlayer;
	}

	setSelectedResource(resourceCode)
	{
		this.selectedResource = resourceCode;
		this.update();
	}

	getSelectedButton()
	{
		return this.buttons.find(button => !this.selectedResource || this.selectedResource === button.resourceCode);
	}

	update()
	{
		for (const button of this.buttons)
			button.update(this.viewedPlayer);
	}
}

BarterButtonManager.IsAvailable = function(panel)
{
	const resourceCount = g_ResourceData.GetBarterableCodes().length;
	return resourceCount && resourceCount <= panel.children.length;
};
