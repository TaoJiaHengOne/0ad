/**
 * Translated JSON file contents.
 */
var g_Options;

/**
 * Names of config keys that have changed, value returned when closing the page.
 */
var g_ChangedKeys;

/**
 * Vertical size of a tab button.
 */
var g_TabButtonHeight = 34;

/**
 * Vertical space between two tab buttons.
 */
var g_TabButtonDist = 5;

/**
 * Vertical distance between the top of the page and the first option.
 */
var g_OptionControlOffset = 5;

/**
 * Vertical size of each option control.
 */
var g_OptionControlHeight = 26;

/**
 * Vertical distance between two consecutive options.
 */
var g_OptionControlDist = 2;

/**
 * Horizontal indentation to distinguish options that depend on another option.
 */
var g_DependentLabelIndentation = 25;

/**
 * Color used to indicate that the string entered by the player isn't a sane color.
 */
var g_InsaneColor = "255 0 255";

/**
 * Defines the parsing of config strings and GUI control interaction for the different option types.
 *
 * @property configToValue - parses a string from the user config to a value of the declared type.
 * @property valueToGui - sets the GUI control to display the given value.
 * @property guiToValue - returns the value of the GUI control.
 * @property guiSetter - event name that should be considered a value change of the GUI control.
 * @property initGUI - sets properties of the GUI control that are independent of the current value.
 * @property sanitizeValue - Displays a visual clue if the entered value is invalid and returns a sane value.
 * @property tooltip - appends a custom tooltip to the given option description depending on the current value.
 */
var g_OptionType = {
	"boolean":
	{
		"configToValue": config => config == "true",
		"valueToGui": (value, control) => {
			control.checked = value;
		},
		"guiToValue": control => control.checked,
		"guiSetter": "onPress"
	},
	"string":
	{
		"configToValue": value => value,
		"valueToGui": (value, control) => {
			control.caption = value;
		},
		"guiToValue": control => control.caption,
		"guiSetter": "onTextEdit"
	},
	"color":
	{
		"configToValue": value => value,
		"valueToGui": (value, control) => {
			control.caption = value;
		},
		"initGUI": (option, control) => {
			control.children[2].onPress = async() => {
				const color = await Engine.OpenChildPage("page_colormixer.xml", control.caption);

				if (color != control.caption)
				{
					control.caption = color;
					control.onTextEdit();
				}
			};
		},
		"guiToValue": control => control.caption,
		"guiSetter": "onTextEdit",
		"sanitizeValue": (value, control, option) => {
			const color = guiToRgbColor(value);
			const sanitized = rgbToGuiColor(color);
			if (control)
			{
				control.sprite = sanitized == value ? "ModernDarkBoxWhite" : "ModernDarkBoxWhiteInvalid";
				control.children[1].sprite = sanitized == value ? "color:" + value : "color:" + g_InsaneColor;
			}
			return sanitized;
		},
		"tooltip": (value, option) =>
			sprintf(translate("Default: %(value)s"), {
				"value": Engine.ConfigDB_GetValue("default", option.config)
			})
	},
	"number":
	{
		"configToValue": value => value,
		"valueToGui": (value, control) => {
			control.caption = value;
		},
		"guiToValue": control => control.caption,
		"guiSetter": "onTextEdit",
		"sanitizeValue": (value, control, option) => {
			const sanitized =
				Math.min(option.max !== undefined ? option.max : +Infinity,
					Math.max(option.min !== undefined ? option.min : -Infinity,
						isNaN(+value) ? 0 : value));

			if (control)
				control.sprite = sanitized == value ? "ModernDarkBoxWhite" : "ModernDarkBoxWhiteInvalid";

			return sanitized;
		},
		"tooltip": (value, option) =>
			sprintf(
				option.min !== undefined && option.max !== undefined ?
					translateWithContext("option number", "Min: %(min)s, Max: %(max)s") :
					option.min !== undefined && option.max === undefined ?
						translateWithContext("option number", "Min: %(min)s") :
						option.min === undefined && option.max !== undefined ?
							translateWithContext("option number", "Max: %(max)s") :
							"",
				{
					"min": option.min,
					"max": option.max
				})
	},
	"dropdown":
	{
		"configToValue": value => value,
		"valueToGui": (value, control) => {
			control.selected = control.list_data.indexOf(value);
		},
		"guiToValue": control => control.list_data[control.selected],
		"guiSetter": "onSelectionChange",
		"initGUI": (option, control) => {
			control.list = option.list.map(e => e.label);
			control.list_data = option.list.map(e => e.value);
			control.onHoverChange = () => {
				const item = option.list[control.hovered];
				control.tooltip = item && item.tooltip || option.tooltip;
			};
		}
	},
	"dropdownNumber":
	{
		"configToValue": value => +value,
		"valueToGui": (value, control) => {
			control.selected = control.list_data.indexOf("" + value);
		},
		"guiToValue": control => +control.list_data[control.selected],
		"guiSetter": "onSelectionChange",
		"initGUI": (option, control) => {
			control.list = option.list.map(e => e.label);
			control.list_data = option.list.map(e => e.value);
			control.onHoverChange = () => {
				const item = option.list[control.hovered];
				control.tooltip = item && item.tooltip || option.tooltip;
			};
		},
		"timeout": async(option, oldValue, hasChanges, newValue) => {
			if (!option.timeout)
				return;
			const buttonIndex = await timedConfirmation(
				500, 200,
				translate("Changes will be reverted in %(time)s seconds. Do you want to keep changes?"),
				"time",
				option.timeout,
				translate("Warning"),
				[translate("No"), translate("Yes")]);

			if (buttonIndex === 0)
				revertChange(option, +oldValue, hasChanges);
		}
	},
	"slider":
	{
		"configToValue": value => +value,
		"valueToGui": (value, control) => {
			control.value = +value;
		},
		"guiToValue": control => control.value,
		"guiSetter": "onValueChange",
		"initGUI": (option, control) => {
			control.max_value = option.max;
			control.min_value = option.min;
		},
		"tooltip": (value, option) =>
			sprintf(translateWithContext("slider number", "Value: %(val)s (min: %(min)s, max: %(max)s)"), {
				"val": value.toFixed(2),
				"min": option.min.toFixed(2),
				"max": option.max.toFixed(2)
			})
	}
};

async function init(data, hotloadData)
{
	g_ChangedKeys = hotloadData ? hotloadData.changedKeys : new Set();
	g_TabCategorySelected = hotloadData ? hotloadData.tabCategorySelected : 0;

	g_Options = Engine.ReadJSONFile("gui/options/options.json");
	translateObjectKeys(g_Options, ["label", "tooltip"]);
	deepfreeze(g_Options);

	placeTabButtons(
		g_Options,
		false,
		g_TabButtonHeight,
		g_TabButtonDist,
		selectPanel,
		displayOptions);

	while (true)
	{
		await new Promise(resolve => { Engine.GetGUIObjectByName("closeButton").onPress = resolve; });
		if (await shouldClosePage())
			return g_ChangedKeys;
	}
}

function getHotloadData()
{
	return {
		"tabCategorySelected": g_TabCategorySelected,
		"changedKeys": g_ChangedKeys
	};
}

/**
 * Sets up labels and controls of all options of the currently selected category.
 */
function displayOptions()
{
	// Hide all controls
	for (const body of Engine.GetGUIObjectByName("option_controls").children)
	{
		body.hidden = true;
		for (const control of body.children)
			control.hidden = true;
	}

	// Initialize label and control of each option for this category
	for (let i = 0; i < g_Options[g_TabCategorySelected].options.length; ++i)
	{
		// Position vertically
		const body = Engine.GetGUIObjectByName("option_control[" + i + "]");
		body.size.top = g_OptionControlOffset + i * (g_OptionControlHeight + g_OptionControlDist);
		body.size.bottom = body.size.top + g_OptionControlHeight;
		body.hidden = false;

		// Load option data
		const option = g_Options[g_TabCategorySelected].options[i];
		const optionType = g_OptionType[option.type];
		const value = optionType.configToValue(Engine.ConfigDB_GetValue("user", option.config));

		// Setup control
		const control = Engine.GetGUIObjectByName("option_control_" + option.type + "[" + i + "]");
		control.tooltip = option.tooltip + (optionType.tooltip ? "\n" + optionType.tooltip(value, option) : "");
		control.hidden = false;

		if (optionType.initGUI)
			optionType.initGUI(option, control);

		control[optionType.guiSetter] = function() {};
		optionType.valueToGui(value, control);
		if (optionType.sanitizeValue)
			optionType.sanitizeValue(value, control, option);

		control[optionType.guiSetter] = function() {

			const newValue = optionType.guiToValue(control);

			if (optionType.sanitizeValue)
				optionType.sanitizeValue(newValue, control, option);

			const oldValue = optionType.configToValue(Engine.ConfigDB_GetValue("user", option.config));

			control.tooltip = option.tooltip + (optionType.tooltip ? "\n" + optionType.tooltip(newValue, option) : "");

			const hasChanges = Engine.ConfigDB_HasChanges("user");
			Engine.ConfigDB_CreateValue("user", option.config, String(newValue));

			g_ChangedKeys.add(option.config);
			fireConfigChangeHandlers(new Set([option.config]));

			if (option.timeout)
				optionType.timeout(option, oldValue, hasChanges, newValue);

			if (option.function)
				Engine[option.function](newValue);

			enableButtons();
		};

		// Setup label
		const label = Engine.GetGUIObjectByName("option_label[" + i + "]");
		label.caption = option.label;
		label.tooltip = option.tooltip;
		label.hidden = false;

		label.size.left = option.dependencies ? g_DependentLabelIndentation : 0;
		label.size.rright = control.size.rleft;
	}

	enableButtons();
}

/**
 * Enable exactly the buttons whose dependencies are met.
 */
function enableButtons()
{
	g_Options[g_TabCategorySelected].options.forEach((option, i) => {
		const isDependencyMet = dependency => {
			if (typeof dependency === "string")
				return Engine.ConfigDB_GetValue("user", dependency) == "true";
			else if (typeof dependency === "object")
			{
				const availableOps = {
					"==": (config, value) => config == value,
					"!=": (config, value) => config != value,
					"<": (config, value) => +config < +value,
					"<=": (config, value) => +config <= +value,
					">": (config, value) => +config > +value,
					">=": (config, value) => +config >= +value
				};
				const op = availableOps[dependency.op] || availableOps["=="];
				return op(Engine.ConfigDB_GetValue("user", dependency.config), dependency.value);
			}
			error("Unsupported dependency: " + uneval(dependency));
			return false;
		};

		const enabled = !option.dependencies || option.dependencies.every(isDependencyMet);

		Engine.GetGUIObjectByName("option_label[" + i + "]").enabled = enabled;
		Engine.GetGUIObjectByName("option_control_" + option.type + "[" + i + "]").enabled = enabled;
	});

	const hasChanges = Engine.ConfigDB_HasChanges("user");
	Engine.GetGUIObjectByName("revertChanges").enabled = hasChanges;
	Engine.GetGUIObjectByName("saveChanges").enabled = hasChanges;
}

async function setDefaults()
{
	const buttonIndex = await messageBox(
		500, 200,
		translate("Resetting the options will erase your saved settings. Do you want to continue?"),
		translate("Warning"),
		[translate("No"), translate("Yes")]);

	if (buttonIndex === 0)
		return;

	for (const category in g_Options)
		for (const option of g_Options[category].options)
		{
			Engine.ConfigDB_RemoveValue("user", option.config);
			g_ChangedKeys.add(option.config);
		}

	Engine.ConfigDB_SaveChanges("user");
	revertChanges();
}

function revertChange(option, oldValue, hadChanges)
{
	Engine.ConfigDB_CreateValue("user", option.config, String(oldValue));

	if (!hadChanges)
		Engine.ConfigDB_SetChanges("user", false);

	if (option.function)
		Engine[option.function](oldValue);

	displayOptions();
}

function revertChanges()
{
	Engine.ConfigDB_Reload("user");

	for (const category in g_Options)
		for (const option of g_Options[category].options)
			if (option.function)
				Engine[option.function](
					g_OptionType[option.type].configToValue(
						Engine.ConfigDB_GetValue("user", option.config)));

	displayOptions();
}

async function saveChanges()
{
	const category = Object.keys(g_Options).find(key => {
		return g_Options[key].options.some(option => {
			const optionType = g_OptionType[option.type];
			if (!optionType.sanitizeValue)
				return false;

			const value = optionType.configToValue(Engine.ConfigDB_GetValue("user", option.config));
			return value != optionType.sanitizeValue(value, undefined, option);
		});
	});

	if (category)
	{
		selectPanel(category);

		const buttonIndex = await messageBox(
			500, 200,
			translate("Some setting values are invalid! Are you sure you want to save them?"),
			translate("Warning"),
			[translate("No"), translate("Yes")]);

		if (buttonIndex === 0)
			return;
	}

	Engine.ConfigDB_SaveChanges("user");
	enableButtons();
}

/**
 * Asks the user if this page really should be closed.
 */
async function shouldClosePage()
{
	if (!Engine.ConfigDB_HasChanges("user"))
		return true;

	const buttonIndex = await messageBox(
		500, 200,
		translate("You have unsaved changes, do you want to close this window?"),
		translate("Warning"),
		[translate("No"), translate("Yes")]);

	return buttonIndex === 1;
}
