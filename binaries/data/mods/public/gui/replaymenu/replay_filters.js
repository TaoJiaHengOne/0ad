/**
 * Allow to filter replays by duration in 15min / 30min intervals.
 */
var g_DurationFilterIntervals = [
	{ "min": -1, "max": -1 },
	{ "min": -1, "max": 15 },
	{ "min": 15, "max": 30 },
	{ "min": 30, "max": 45 },
	{ "min": 45, "max": 60 },
	{ "min": 60, "max": 90 },
	{ "min": 90, "max": 120 },
	{ "min": 120, "max": -1 }
];

/**
 * Allow to filter by population capacity.
 */
const g_PopulationCapacities = prepareForDropdown(g_Settings && g_Settings.PopulationCapacities);

/**
 * Reloads the selectable values in the filters. The filters depend on g_Settings and g_Replays
 * (including its derivatives g_MapSizes, g_MapNames).
 */
function initFilters(filters)
{
	if (filters && filters.compatibility !== undefined)
		Engine.GetGUIObjectByName("compatibilityFilter").checked = filters.compatibility;

	if (filters && filters.playernames)
		Engine.GetGUIObjectByName("playersFilter").caption = filters.playernames;

	initDateFilter(filters);
	initMapSizeFilter(filters);
	initMapNameFilter(filters);
	initPopCapFilter(filters);
	initDurationFilter(filters);
	initSingleplayerFilter(filters);
	initVictoryConditionFilter(filters);
	initRatedGamesFilter(filters);
}

/**
 * Allow to filter by month. Uses g_Replays.
 */
function initDateFilter(filters)
{
	var months = g_Replays.map(replay => getReplayMonth(replay));
	months = months.filter((month, index) => months.indexOf(month) == index).sort();

	var dateTimeFilter = Engine.GetGUIObjectByName("dateTimeFilter");
	dateTimeFilter.list = [translateWithContext("datetime", "Any")].concat(months);
	dateTimeFilter.list_data = [""].concat(months);

	if (filters && filters.date)
		dateTimeFilter.selected = dateTimeFilter.list_data.indexOf(filters.date);

	if (dateTimeFilter.selected == -1 || dateTimeFilter.selected >= dateTimeFilter.list.length)
		dateTimeFilter.selected = 0;
}

/**
 * Allow to filter by mapsize. Uses g_MapSizes.
 */
function initMapSizeFilter(filters)
{
	var mapSizeFilter = Engine.GetGUIObjectByName("mapSizeFilter");
	mapSizeFilter.list = [translateWithContext("map size", "Any")].concat(g_MapSizes.Name);
	mapSizeFilter.list_data = [-1].concat(g_MapSizes.Tiles);

	if (filters && filters.mapSize)
		mapSizeFilter.selected = mapSizeFilter.list_data.indexOf(filters.mapSize);

	if (mapSizeFilter.selected == -1 || mapSizeFilter.selected >= mapSizeFilter.list.length)
		mapSizeFilter.selected = 0;
}

/**
 * Allow to filter by mapname. Uses g_MapNames.
 */
function initMapNameFilter(filters)
{
	var mapNameFilter = Engine.GetGUIObjectByName("mapNameFilter");
	mapNameFilter.list = [translateWithContext("map name", "Any")].concat(g_MapNames.map(name => translate(name)));
	mapNameFilter.list_data = [""].concat(g_MapNames);

	if (filters && filters.mapName)
		mapNameFilter.selected = mapNameFilter.list_data.indexOf(filters.mapName);

	if (mapNameFilter.selected == -1 || mapNameFilter.selected >= mapNameFilter.list.length)
		mapNameFilter.selected = 0;
}

/**
 * Allow to filter by population capacity.
 */
function initPopCapFilter(filters)
{
	var populationFilter = Engine.GetGUIObjectByName("populationFilter");

	// Merge the pop cap options of all pop cap types into one single list.
	const popCapOptions = g_PopulationCapacities.Options
		.map(item => item.List)
		.flat()
		.reduce((list, cap) => {
			if (!list.includes(cap))
				list.push(cap);
			return list;
		}, [])
		.sort((a, b) => a > b);

	populationFilter.list = [translateWithContext("population capacity", "Any")].concat(popCapOptions.map(cap => cap >= 10000 ? "Unlimited" : cap));
	populationFilter.list_data = [""].concat(popCapOptions);

	if (filters?.popCap)
		populationFilter.selected = populationFilter.list_data.indexOf(filters.popCap);

	if (populationFilter.selected == -1 || populationFilter.selected >= populationFilter.list.length)
		populationFilter.selected = 0;
}

/**
 * Allow to filter by game duration. Uses g_DurationFilterIntervals.
 */
function initDurationFilter(filters)
{
	var durationFilter = Engine.GetGUIObjectByName("durationFilter");
	durationFilter.list = g_DurationFilterIntervals.map((interval, index) => {

		if (index == 0)
			return translateWithContext("duration", "Any");

		if (index == 1)
			// Translation: Shorter duration than max minutes.
			return sprintf(translatePluralWithContext("duration filter", "< %(max)s min", "< %(max)s min", interval.max), interval);

		if (index == g_DurationFilterIntervals.length - 1)
			// Translation: Longer duration than min minutes.
			return sprintf(translatePluralWithContext("duration filter", "> %(min)s min", "> %(min)s min", interval.min), interval);

		// Translation: Duration between min and max minutes.
		return sprintf(translateWithContext("duration filter", "%(min)s - %(max)s min"), interval);
	});
	durationFilter.list_data = g_DurationFilterIntervals.map((interval, index) => index);

	if (filters && filters.duration)
		durationFilter.selected = durationFilter.list_data.indexOf(filters.duration);

	if (durationFilter.selected == -1 || durationFilter.selected >= g_DurationFilterIntervals.length)
		durationFilter.selected = 0;
}

function initSingleplayerFilter(filters)
{
	const singleplayerFilter = Engine.GetGUIObjectByName("singleplayerFilter");
	singleplayerFilter.list = [
		translateWithContext("replay filter", "Any"),
		translateWithContext("replay filter", "Single-player"),
		translateWithContext("replay filter", "Multiplayer"),
		translateWithContext("replay filter", "Campaigns")
	];
	singleplayerFilter.list_data = ["", "Single-player", "Multiplayer", "Campaigns"];

	if (filters && filters.singleplayer)
		singleplayerFilter.selected = singleplayerFilter.list_data.indexOf(filters.singleplayer);

	if (singleplayerFilter.selected < 0 || singleplayerFilter.selected >= singleplayerFilter.list.length)
		singleplayerFilter.selected = 0;
}

function initVictoryConditionFilter(filters)
{
	const victoryConditionFilter = Engine.GetGUIObjectByName("victoryConditionFilter");
	victoryConditionFilter.list = [translate("Any victory condition")].concat(g_VictoryConditions.map(victoryCondition => translateVictoryCondition(victoryCondition.Name)));
	victoryConditionFilter.list_data = [""].concat(g_VictoryConditions.map(victoryCondition => victoryCondition.Name));

	if (filters && filters.victoryCondition)
		victoryConditionFilter.selected = victoryConditionFilter.list_data.indexOf(filters.victoryCondition);

	if (victoryConditionFilter.selected < 0 || victoryConditionFilter.selected >= victoryConditionFilter.list.length)
		victoryConditionFilter.selected = 0;
}

function initRatedGamesFilter(filters)
{
	const ratedGamesFilter = Engine.GetGUIObjectByName("ratedGamesFilter");
	ratedGamesFilter.list = [translate("Rated and unrated games"), translate("Rated games"), translate("Unrated games")];
	ratedGamesFilter.list_data = ["", "rated", "not rated"];

	if (filters && filters.ratedGames)
		ratedGamesFilter.selected = ratedGamesFilter.list_data.indexOf(filters.ratedGames);

	if (ratedGamesFilter.selected < 0 || ratedGamesFilter.selected >= ratedGamesFilter.list.length)
		ratedGamesFilter.selected = 0;
}

/**
 * Initializes g_ReplaysFiltered with replays that are not filtered out and sort it.
 */
function filterReplays()
{
	const sortKey = Engine.GetGUIObjectByName("replaySelection").selected_column;
	const sortOrder = Engine.GetGUIObjectByName("replaySelection").selected_column_order;

	g_ReplaysFiltered = g_Replays.filter(replay => filterReplay(replay)).sort((a, b) => {
		let cmpA, cmpB, cmpA_secondary, cmpB_secondary;
		switch (sortKey)
		{
		case 'months':
			cmpA = +a.attribs.timestamp;
			cmpB = +b.attribs.timestamp;
			break;
		case 'duration':
			cmpA = +a.duration;
			cmpB = +b.duration;
			break;
		case 'players':
			cmpA = +a.attribs.settings.PlayerData.length;
			cmpB = +b.attribs.settings.PlayerData.length;
			break;
		case 'mapName':
			cmpA = getReplayMapName(a);
			cmpB = getReplayMapName(b);
			break;
		case 'mapSize':
			cmpA = +a.attribs.settings.Size;
			cmpB = +b.attribs.settings.Size;
			break;
		case 'popCapacity':
			cmpA = +a.attribs.settings.PopulationCap;
			cmpB = +b.attribs.settings.PopulationCap;
			cmpA_secondary = g_PopulationCapacities.Name.indexOf(a.attribs.settings.PopulationCapType);
			cmpB_secondary = g_PopulationCapacities.Name.indexOf(b.attribs.settings.PopulationCapType);
			break;
		default:
			error("Unknown sortKey in filterReplays: " + sortKey);
		}

		if (cmpA < cmpB)
			return -sortOrder;
		else if (cmpA > cmpB)
			return +sortOrder;

		else if(cmpA_secondary && cmpB_secondary)
			if (cmpA_secondary < cmpB_secondary)
				return -sortOrder;
			else if(cmpA_secondary > cmpB_secondary)
				return +sortOrder;

		return 0;
	});
}

/**
 * Decides whether the replay should be listed.
 *
 * @returns {bool} - true if replay should be visible
 */
function filterReplay(replay)
{
	// Check for compatibility first (most likely to filter)
	const compatibilityFilter = Engine.GetGUIObjectByName("compatibilityFilter");
	if (compatibilityFilter.checked && !isReplayCompatible(replay))
		return false;

	// Filter by single-player or multiplayer.
	const singleplayerFilter = Engine.GetGUIObjectByName("singleplayerFilter");
	const selectedSingleplayerFilter = singleplayerFilter.list_data[singleplayerFilter.selected] || "";
	if (selectedSingleplayerFilter == "Campaigns" && !replay.isCampaign ||
	    selectedSingleplayerFilter == "Single-player" && (replay.isMultiplayer || replay.isCampaign) ||
	    selectedSingleplayerFilter == "Multiplayer" && (!replay.isMultiplayer || replay.isCampaign))
		return false;

	// Filter by victory condition
	const victoryConditionFilter = Engine.GetGUIObjectByName("victoryConditionFilter");
	if (victoryConditionFilter.selected > 0 &&
	    replay.attribs.settings.VictoryConditions.indexOf(victoryConditionFilter.list_data[victoryConditionFilter.selected]) == -1)
		return false;

	// Filter by rating
	const ratedGamesFilter = Engine.GetGUIObjectByName("ratedGamesFilter");
	const selectedRatedGamesFilter = ratedGamesFilter.list_data[ratedGamesFilter.selected] || "";
	if (selectedRatedGamesFilter == "rated" && !replay.isRated ||
	    selectedRatedGamesFilter == "not rated" && replay.isRated)
		return false;

	// Filter date/time (select a month)
	const dateTimeFilter = Engine.GetGUIObjectByName("dateTimeFilter");
	if (dateTimeFilter.selected > 0 && getReplayMonth(replay) != dateTimeFilter.list_data[dateTimeFilter.selected])
		return false;

	// Filter by playernames
	const playersFilter = Engine.GetGUIObjectByName("playersFilter");
	const keywords = playersFilter.caption.toLowerCase().split(" ");
	if (keywords.length)
	{
		// We just check if all typed words are somewhere in the playerlist of that replay
		const playerList = replay.attribs.settings.PlayerData.map(player => player ? player.Name : "").join(" ").toLowerCase();
		if (!keywords.every(keyword => playerList.indexOf(keyword) != -1))
			return false;
	}

	// Filter by map name
	const mapNameFilter = Engine.GetGUIObjectByName("mapNameFilter");
	if (mapNameFilter.selected > 0 && replay.attribs.settings.mapName != mapNameFilter.list_data[mapNameFilter.selected])
		return false;

	// Filter by map size
	const mapSizeFilter = Engine.GetGUIObjectByName("mapSizeFilter");
	if (mapSizeFilter.selected > 0 && replay.attribs.settings.Size != mapSizeFilter.list_data[mapSizeFilter.selected])
		return false;

	// Filter by population capacity
	const populationFilter = Engine.GetGUIObjectByName("populationFilter");
	if (populationFilter.selected > 0 && replay.attribs.settings.PopulationCap != populationFilter.list_data[populationFilter.selected])
		return false;

	// Filter by game duration
	const durationFilter = Engine.GetGUIObjectByName("durationFilter");
	if (durationFilter.selected > 0)
	{
		const interval = g_DurationFilterIntervals[durationFilter.selected];

		if ((interval.min > -1 && replay.duration < interval.min * 60) ||
			(interval.max > -1 && replay.duration > interval.max * 60))
			return false;
	}

	return true;
}
