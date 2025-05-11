const CAPTYPE_PLAYER_POPULATION = "player";
const CAPTYPE_TEAM_POPULATION = "team";
const CAPTYPE_WORLD_POPULATION = "world";

function PopulationCapManager() {}

PopulationCapManager.prototype.Schema =
	"<a:component type='system'/><empty/>";

PopulationCapManager.prototype.Init = function()
{
};

/**
 * Set the pop cap type and, if possible, initialize the first distribution.
 * @param {string} type
 */
PopulationCapManager.prototype.SetPopulationCapType = function(type)
{
	if ([CAPTYPE_PLAYER_POPULATION, CAPTYPE_TEAM_POPULATION, CAPTYPE_WORLD_POPULATION].includes(type))
		this.popCapType = type;
	else
	{
		warn(`Attempted to set an unknown population capacity type: '${type}'. Continuing with type 'Player Population'...`);
		this.popCapType = CAPTYPE_PLAYER_POPULATION;
	}
	if (this.popCap)
		this.InitializePopCaps();
};

/**
 * Get the current pop cap type.
 * @returns {string}
 */
PopulationCapManager.prototype.GetPopulationCapType = function()
{
	return this.popCapType;
};

/**
 * Set the pop cap and, if possible, initialize the first distribution.
 * @param {number} cap
 */
PopulationCapManager.prototype.SetPopulationCap = function(cap)
{
	this.popCap = cap;
	if (this.popCapType)
		this.InitializePopCaps();
};

/**
 * Get the current pop cap.
 * @returns {number}
 */
PopulationCapManager.prototype.GetPopulationCap = function()
{
	return this.popCap;
};

/**
 * Calculate and distribute the pop caps for the first time. Called as soon as cap and cap type are set.
 */
PopulationCapManager.prototype.InitializePopCaps = function()
{
	switch(this.popCapType)
	{
	case CAPTYPE_PLAYER_POPULATION:
		this.InitializePlayerPopCaps();
		break;

	case CAPTYPE_TEAM_POPULATION:
		this.InitializeTeamPopCaps();
		break;

	case CAPTYPE_WORLD_POPULATION:
		this.RedistributeWorldPopCap();
		break;

	default:
		break;
	}
};

/**
 * Assign all players the same, fixed pop cap.
 */
PopulationCapManager.prototype.InitializePlayerPopCaps = function()
{
	const players = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetActivePlayers();
	for (const player of players)
		QueryPlayerIDInterface(player, IID_Player)
			.SetMaxPopulation(this.popCap);
};

/**
 * Loop through all teams and distribute the fixed pop cap among their living members.
 */
PopulationCapManager.prototype.InitializeTeamPopCaps = function()
{
	const players = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetActivePlayers();
	const processedTeams = [];
	for (const player of players)
	{
		const team = QueryPlayerIDInterface(player, IID_Diplomacy).GetTeam();
		if (processedTeams.includes(team))
			continue;
		processedTeams.push(team);
		this.RedistributeTeamPopCap(team);
	}
};

/**
 * Recalculate and update a single team's members' pop caps.
 * @param {number} team - ID specifying the team.
 */
PopulationCapManager.prototype.RedistributeTeamPopCap = function(team)
{
	const activePlayers = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetActivePlayers();
	const teamMembers = activePlayers.reduce((list, player) => {
		if (QueryPlayerIDInterface(player, IID_Diplomacy).GetTeam() === team)
			list.push(player);
		return list;
	}, []);

	// Players of team -1 aren't part of any team and need to be assigned the full team pop cap.
	const newPopulationCap = team === -1 ? this.popCap : Math.round(this.popCap / teamMembers.length);
	for (const teamMember of teamMembers)
		QueryPlayerIDInterface(teamMember, IID_Player)
			.SetMaxPopulation(newPopulationCap);
};

/**
 * Recalculate the players' new pop cap and assign it to all of them.
 */
PopulationCapManager.prototype.RedistributeWorldPopCap = function()
{
	const activePlayers = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager).GetActivePlayers();
	if (!activePlayers.length)
		return;

	const newPopulationCap = Math.round(this.popCap / activePlayers.length);
	for (const player of activePlayers)
		QueryPlayerIDInterface(player, IID_Player).SetMaxPopulation(newPopulationCap);
};

/**
 * Redistribute the pop caps depending on the pop cap type.
 * @param {number} msg.playerId - the defeated player's ID.
 */
PopulationCapManager.prototype.OnGlobalPlayerDefeated = function(msg)
{
	switch(this.popCapType)
	{
	case CAPTYPE_TEAM_POPULATION:
		const team = QueryPlayerIDInterface(msg.playerId, IID_Diplomacy).GetTeam();
		if (team != -1)
			this.RedistributeTeamPopCap(team);
		break;

	case CAPTYPE_WORLD_POPULATION:
		this.RedistributeWorldPopCap();
		break;

	case CAPTYPE_PLAYER_POPULATION:
	default: break;
	}
};

/**
 * Redistribute pop caps when a player is moved from one team to another.
 * @param {number} msg.player - the ID of the player.
 * @param {number} msg.oldTeam - the ID of the team the player was previously part of.
 * @param {number} msg.newTeam - the ID of the team the player is moved to.
 */
PopulationCapManager.prototype.OnTeamChanged = function(msg)
{
	if (this.popCapType != CAPTYPE_TEAM_POPULATION)
		return;

	this.RedistributeTeamPopCap(msg.oldTeam);
	this.RedistributeTeamPopCap(msg.newTeam);
};

Engine.RegisterSystemComponentType(IID_PopulationCapManager, "PopulationCapManager", PopulationCapManager);
