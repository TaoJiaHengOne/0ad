/**
 * This class manages the counters in the top panel.
 * For allies who researched team vision and observers,
 * it displays the resources in a tooltip in a player chosen order.
 */
class CounterManager
{
	constructor(playerViewControl)
	{
		this.allyPlayerStates = {};

		this.counters = [];

		this.resourceCounts = Engine.GetGUIObjectByName("resourceCounts");

		// TODO: filter resources depending on JSON file
		for (const resCode of g_ResourceData.GetCodes())
			this.addCounter(resCode, CounterResource);

		this.addCounter("population", CounterPopulation);

		this.init();

		registerSimulationUpdateHandler(this.rebuild.bind(this));
		playerViewControl.registerViewedPlayerChangeHandler(this.rebuild.bind(this));
	}

	addCounter(resCode, type)
	{
		const panelCount = this.resourceCounts.children.length;
		if (this.counters.length + 1 > panelCount)
			throw "There are " + (this.counters.length + 1) + " resource counters to display, but only " + panelCount + " panel items!";

		const id = "[" + this.counters.length + "]";
		this.counters.push(
			new type(
				resCode,
				Engine.GetGUIObjectByName("resource" + id),
				Engine.GetGUIObjectByName("resource" + id + "_icon"),
				Engine.GetGUIObjectByName("resource" + id + "_count"),
				Engine.GetGUIObjectByName("resource" + id + "_stats")));
	}

	init()
	{
		horizontallySpaceObjects("resourceCounts", this.counters.length);
		hideRemaining("resourceCounts", this.counters.length);

		for (const counter of this.counters)
		{
			counter.icon.sprite = "stretched:session/icons/resources/" + counter.resCode + ".png";
			counter.panel.onPress = this.onPress.bind(this);
		}
	}

	onPress()
	{
		Engine.ConfigDB_CreateAndSaveValue(
			"user",
			"gui.session.respoptooltipsort",
			String((+Engine.ConfigDB_GetValue("user", "gui.session.respoptooltipsort") + 2) % 3 - 1));
		this.rebuild();
	}

	rebuild()
	{
		const hidden = g_ViewedPlayer <= 0;
		this.resourceCounts.hidden = hidden;
		if (hidden)
			return;

		const viewedPlayerState = g_SimState.players[g_ViewedPlayer];
		this.allyPlayerStates = {};
		for (const player in g_SimState.players)
			if (player != 0 &&
				player != g_ViewedPlayer &&
				g_Players[player].state != "defeated" &&
				(g_IsObserver ||
					viewedPlayerState.hasSharedLos &&
					g_Players[player].isMutualAlly[g_ViewedPlayer]))
				this.allyPlayerStates[player] = g_SimState.players[player];

		this.selectedOrder = +Engine.ConfigDB_GetValue("user", "gui.session.respoptooltipsort");
		this.orderTooltip = this.getOrderTooltip();

		for (const counter of this.counters)
		{
			const isHidden = g_ViewedPlayer <= 0;
			counter.panel.hidden = isHidden;
			if (!isHidden)
				counter.rebuild(viewedPlayerState, this.getAllyStatTooltip.bind(this));
		}
	}

	getOrderTooltip()
	{
		if (!Object.keys(this.allyPlayerStates).length)
			return "";

		return "\n" + sprintf(translate("%(order)s: %(hotkey)s to change order."), {
			"hotkey": setStringTags("\\[Click]", g_HotkeyTags),
			"order":
				this.selectedOrder == 0 ?
					translate("Unordered") :
					this.selectedOrder == 1 ?
						translate("Descending") :
						translate("Ascending")
		});
	}

	getAllyStatTooltip(getTooltipData)
	{
		const tooltipData = [];

		for (const playerID in this.allyPlayerStates)
		{
			const playername = colorizePlayernameHelper("■", playerID) + " " + g_Players[playerID].name;
			tooltipData.push(getTooltipData(this.allyPlayerStates[playerID], playername));
		}

		if (this.selectedOrder)
			tooltipData.sort((a, b) => this.selectedOrder * (b.orderValue - a.orderValue));

		return this.orderTooltip +
			tooltipData.reduce((result, data) =>
				setStringTags(
					result + "\n" + sprintf(translate(this.AllyStatTooltip), data),
					this.AllyStatTags
				), "");
	}
}

CounterManager.ResourceTitleTags = { "font": "sans-bold-16" };

CounterManager.prototype.AllyStatTooltip = markForTranslation("%(playername)s: %(statValue)s");
CounterManager.prototype.AllyStatTags = { "font": "sans-bold-stroke-14" };
