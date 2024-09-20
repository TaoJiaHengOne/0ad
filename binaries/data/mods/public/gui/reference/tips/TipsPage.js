class TipsPage
{
	constructor()
	{
		this.tipDisplay = new TipDisplay();
		this.closeButton = new CloseButton(this);
	}

	closePage()
	{
		Engine.PopGuiPage("page_tips.xml");
	}
}

TipsPage.prototype.CloseButtonTooltip = translate("%(hotkey)s: Close Tips and Tricks.");
