function layoutSelectionSingle()
{
	getGUIObjectByName("detailsAreaSingle").hidden = false;
	getGUIObjectByName("detailsAreaMultiple").hidden = true;
}

function layoutSelectionMultiple()
{
	getGUIObjectByName("detailsAreaMultiple").hidden = false;
	getGUIObjectByName("detailsAreaSingle").hidden = true;
}

// Fills out information that most entities have
function displaySingle(entState, template)
{
	// Get general unit and player data
	var specificName = template.name.specific;
	var genericName = template.name.generic != template.name.specific? template.name.generic : "";

	var civName = g_CivData[g_Players[entState.player].civ].Name;

	var playerName = g_Players[entState.player].name;
	var playerColor = g_Players[entState.player].color.r + " " + g_Players[entState.player].color.g + " " +
								g_Players[entState.player].color.b+ " " + g_Players[entState.player].color.a;

	// Indicate disconnected players by prefixing their name
	if (g_Players[entState.player].offline)
	{
		playerName = "[OFFLINE] " + playerName;
	}

	// Rank					
	getGUIObjectByName("rankIcon").sprite = getRankIconSprite(entState);					
								
	// Hitpoints
	var hitpoints = "";

	if (entState.hitpoints)
	{
		var unitHealthBar = getGUIObjectByName("healthBar");
		var healthSize = unitHealthBar.size;
		healthSize.rtop = 100-100*Math.max(0, Math.min(1, entState.hitpoints / entState.maxHitpoints));
		unitHealthBar.size = healthSize;

		hitpoints = "[font=\"serif-bold-13\"]Health [/font]" + entState.hitpoints + "/" + entState.maxHitpoints;
		getGUIObjectByName("health").tooltip = hitpoints;
		getGUIObjectByName("health").hidden = false;
	}
	else
	{
		getGUIObjectByName("health").hidden = true;
	}
	
	// TODO: Stamina
	// getGUIObjectByName("staminaBar");
	var player = Engine.GetPlayerID();
	if (entState.player == player || g_DevSettings.controlAll)
	{
		//if (entState.stamina !== undefined)
			getGUIObjectByName("stamina").hidden = false;
		//else
		//	getGUIObjectByName("stamina").hidden = true;
	}
	else
	{
		getGUIObjectByName("stamina").hidden = true;
	}

	
	// Experience
	if (entState.promotion)
	{
		var experienceBar = getGUIObjectByName("experienceBar");
		var experienceSize = experienceBar.size;
		experienceSize.rtop = 100 - 100 * Math.max(0, Math.min(1, 1.0 * entState.promotion.curr / entState.promotion.req));
		experienceBar.size = experienceSize;
 
		var experience = "[font=\"serif-bold-13\"]Experience [/font]" + entState.promotion.curr;
		if (entState.promotion.curr < entState.promotion.req)
			experience += "/" + entState.promotion.req;
		getGUIObjectByName("experience").tooltip = experience;
		getGUIObjectByName("experience").hidden = false;
	}
	else
	{
		getGUIObjectByName("experience").hidden = true;
	}

	// Resource stats
	var resources = "";
	var resourceType = "";

	if (entState.resourceSupply)
	{
		resources = Math.ceil(+entState.resourceSupply.amount) + "/" + entState.resourceSupply.max;
		resourceType = entState.resourceSupply.type["generic"];
		if (resourceType == "treasure")
			resourceType = entState.resourceSupply.type["specific"];

		var unitResourceBar = getGUIObjectByName("resourceBar");
		var resourceSize = unitResourceBar.size;

		resourceSize.rtop = 100-100*Math.max(0, Math.min(1, +entState.resourceSupply.amount / entState.resourceSupply.max));
		unitResourceBar.size = resourceSize;

		var unitResources = getGUIObjectByName("resources");
		unitResources.tooltip = "[font=\"serif-bold-13\"]Resources: [/font]" + resources + " " + resourceType;

		if (!entState.hitpoints)
			unitResources.size = getGUIObjectByName("health").size;
		else
			unitResources.size = getGUIObjectByName("stamina").size;

		getGUIObjectByName("resources").hidden = false;
	}
	else
	{
		getGUIObjectByName("resources").hidden = true;
	}

	// Resource carrying
	if (entState.resourceCarrying && entState.resourceCarrying.length)
	{
		// We should only be carrying one resource type at once, so just display the first
		var carried = entState.resourceCarrying[0];

		getGUIObjectByName("resourceCarryingIcon").hidden = false;
		getGUIObjectByName("resourceCarryingText").hidden = false;
		getGUIObjectByName("resourceCarryingIcon").sprite = "stretched:session/icons/resources/"+carried.type+".png";
		getGUIObjectByName("resourceCarryingText").caption = carried.amount + "/" + carried.max;
	}
	// Use the same indicators for traders
	else if (entState.trader && entState.trader.goods.amount > 0)
	{
		getGUIObjectByName("resourceCarryingIcon").hidden = false;
		getGUIObjectByName("resourceCarryingText").hidden = false;
		getGUIObjectByName("resourceCarryingIcon").sprite = "stretched:session/icons/resources/"+entState.trader.goods.type+".png";
		getGUIObjectByName("resourceCarryingText").caption = entState.trader.goods.amount;
	}
	else
	{
		getGUIObjectByName("resourceCarryingIcon").hidden = true;
		getGUIObjectByName("resourceCarryingText").hidden = true;
	}

	// Set Captions
	getGUIObjectByName("specific").caption = specificName;
	getGUIObjectByName("player").caption = playerName;
	getGUIObjectByName("player").textcolor = playerColor;

	// Icon image
	if (template.icon)
	{
		getGUIObjectByName("icon").sprite = "stretched:session/portraits/" + template.icon;
	}
	else
	{
		// TODO: we should require all entities to have icons, so this case never occurs
		getGUIObjectByName("icon").sprite = "bkFillBlack";
	}

	// Tooltips
	getGUIObjectByName("specific").tooltip = genericName;
	getGUIObjectByName("player").tooltip = civName;
	getGUIObjectByName("health").tooltip = hitpoints;
	getGUIObjectByName("armourIcon").tooltip = "[font=\"serif-bold-16\"]Attack: [/font]" + damageTypesToText(entState.attack) + 
		"\n[font=\"serif-bold-16\"]Armor: [/font]" + damageTypesToText(entState.armour);

	// Icon Tooltip
	var iconTooltip = "";

	if (genericName)
		iconTooltip = "[font=\"serif-bold-16\"]" + genericName + "[/font]";

	if (template.tooltip)
		iconTooltip += "\n[font=\"serif-13\"]" + template.tooltip + "[/font]";

	getGUIObjectByName("iconBorder").tooltip = iconTooltip;

	// Unhide Details Area
	getGUIObjectByName("detailsAreaSingle").hidden = false;
	getGUIObjectByName("detailsAreaMultiple").hidden = true;
}

// Fills out information for multiple entities
function displayMultiple(selection, template)
{
	var averageHealth = 0;
	var maxHealth = 0;

	for (var i = 0; i < selection.length; i++)
	{
		var entState = GetEntityState(selection[i])
		if (entState)
		{
			if (entState.hitpoints)
			{
				averageHealth += entState.hitpoints;
				maxHealth += entState.maxHitpoints;
			}
		}
	}

	if (averageHealth > 0)
	{
		var unitHealthBar = getGUIObjectByName("healthBarMultiple");
		var healthSize = unitHealthBar.size;	
		healthSize.rtop = 100-100*Math.max(0, Math.min(1, averageHealth / maxHealth));
		unitHealthBar.size = healthSize;

		var hitpoints = "[font=\"serif-bold-13\"]Hitpoints [/font]" + averageHealth + "/" + maxHealth;
		var healthMultiple = getGUIObjectByName("healthMultiple");
		healthMultiple.tooltip = hitpoints;
		healthMultiple.hidden = false;
	}
	else
	{
		getGUIObjectByName("healthMultiple").hidden = true;
	}
	
	// TODO: Stamina
	// getGUIObjectByName("staminaBarMultiple");

	getGUIObjectByName("numberOfUnits").caption = selection.length;

	// Unhide Details Area
	getGUIObjectByName("detailsAreaMultiple").hidden = false;
	getGUIObjectByName("detailsAreaSingle").hidden = true;
}

// Updates middle entity Selection Details Panel
function updateSelectionDetails()
{
	var supplementalDetailsPanel = getGUIObjectByName("supplementalSelectionDetails");
	var detailsPanel = getGUIObjectByName("selectionDetails");
	var commandsPanel = getGUIObjectByName("unitCommands");

	g_Selection.update();
	var selection = g_Selection.toList();
	
	if (selection.length == 0)
	{
		getGUIObjectByName("detailsAreaMultiple").hidden = true;
		getGUIObjectByName("detailsAreaSingle").hidden = true;
		hideUnitCommands();
	
		supplementalDetailsPanel.hidden = true;
		detailsPanel.hidden = true;
		commandsPanel.hidden = true;
		return;
	}

	/* If the unit has no data (e.g. it was killed), don't try displaying any
	 data for it. (TODO: it should probably be removed from the selection too;
	 also need to handle multi-unit selections) */
	var entState = GetEntityState(selection[0]);
	if (!entState)
		return;

	var template = GetTemplateData(entState.template);

	// Fill out general info and display it
	if (selection.length == 1)
		displaySingle(entState, template);
	else
		displayMultiple(selection, template);

	// Show Panels
	supplementalDetailsPanel.hidden = false;
	detailsPanel.hidden = false;
	commandsPanel.hidden = false;

	// Fill out commands panel for specific unit selected (or first unit of primary group)
	updateUnitCommands(entState, supplementalDetailsPanel, commandsPanel, selection);
}
