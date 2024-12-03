class TipsPage
{
	constructor(initData, hotloadData)
	{
		this.tipDisplay = new TipDisplay(initData, hotloadData);
		this.closeButton = new CloseButton(this);
	}

	closePage()
	{
		Engine.PopGuiPage("page_tips.xml");
	}
}

TipsPage.prototype.CloseButtonTooltip = translate("%(hotkey)s: Close Tips and Tricks.");
