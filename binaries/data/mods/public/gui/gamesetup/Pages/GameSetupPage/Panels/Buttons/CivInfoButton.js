class CivInfoButton
{
	constructor()
	{
		this.civInfo = {
			"page": "page_civinfo.xml"
		};

		const civInfoButton = Engine.GetGUIObjectByName("civInfoButton");
		civInfoButton.onPress = this.onPress.bind(this);
		civInfoButton.tooltip =
			sprintf(translate(this.Tooltip), {
				"hotkey_civinfo": colorizeHotkey("%(hotkey)s", "civinfo"),
				"hotkey_structree": colorizeHotkey("%(hotkey)s", "structree")
			});

		Engine.SetGlobalHotkey("structree", "Press", this.openPage.bind(this, "page_structree.xml"));
		Engine.SetGlobalHotkey("civinfo", "Press", this.openPage.bind(this, "page_civinfo.xml"));
	}

	onPress()
	{
		this.openPage(this.civInfo.page);
	}

	async openPage(page)
	{
		this.civInfo = await pageLoop(page, this.civInfo.args);
	}
}

CivInfoButton.prototype.Tooltip =
	translate("%(hotkey_civinfo)s / %(hotkey_structree)s: View Civilization Overview / Structure Tree\nLast opened will be reopened on click.");
