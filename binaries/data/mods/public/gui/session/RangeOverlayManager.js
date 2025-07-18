/**
 * This class informs:
 *   - the GuiInterface of which components of entities are to display range overlays and
 *   - the RangeOverlayManager entity component which entities are to display range overlays.
 */
class RangeOverlayManager
{
	constructor(selection)
	{
		this.selection = selection;

		for (const type of this.Types)
		{
			this.setEnabled(type, this.isEnabled(type));
			Engine.SetGlobalHotkey(type.hotkey, "Press", this.toggle.bind(this, type));
		}

		registerConfigChangeHandler(this.onConfigChange.bind(this));
	}

	onConfigChange(changes)
	{
		for (const type of this.Types)
			if (changes.has(type.config))
				this.setEnabled(type, this.isEnabled(type));
	}

	isEnabled(type)
	{
		return Engine.ConfigDB_GetValue("user", type.config) == "true";
	}

	setEnabled(type, enabled)
	{
		Engine.GuiInterfaceCall("EnableVisualRangeOverlayType", {
			"type": type.component,
			"enabled": enabled
		});

		const selected = this.selection.toList();
		for (const ent in this.selection.highlighted)
			selected.push(this.selection.highlighted[ent]);

		Engine.GuiInterfaceCall("SetRangeOverlays", {
			"entities": selected,
			"enabled": enabled
		});
	}

	toggle(type)
	{
		const enabled = !this.isEnabled(type);

		Engine.ConfigDB_CreateAndSaveValue(
			"user",
			type.config,
			String(enabled));

		this.setEnabled(type, enabled);
	}
}

RangeOverlayManager.prototype.Types = [
	{
		"component": "Attack",
		"config": "gui.session.attackrange",
		"hotkey": "session.toggleattackrange"
	},
	{
		"component": "Auras",
		"config": "gui.session.aurasrange",
		"hotkey": "session.toggleaurasrange"
	},
	{
		"component": "Heal",
		"config": "gui.session.healrange",
		"hotkey": "session.togglehealrange"
	}
];
