class CivInfoButton
{
	constructor(parentPage)
	{
		this.parentPage = parentPage;

		this.civInfoButton = Engine.GetGUIObjectByName("civInfoButton");
		this.civInfoButton.onPress = this.onPress.bind(this);
		this.civInfoButton.caption = this.Caption;
		this.civInfoButton.tooltip = colorizeHotkey(this.Tooltip, this.Hotkey);
	}

	onPress()
	{
		this.parentPage.closePageCallback({
			"nextPage": "page_civinfo.xml",
			"args": {
				"civ": this.parentPage.activeCiv
			}
		});
	}

}

CivInfoButton.prototype.Caption =
	translate("Civilization Overview");

CivInfoButton.prototype.Hotkey =
	"civinfo";

CivInfoButton.prototype.Tooltip =
	translate("%(hotkey)s: Switch to Civilization Overview.");
