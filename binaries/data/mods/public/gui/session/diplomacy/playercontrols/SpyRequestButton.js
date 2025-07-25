/**
 * This class is concerned with providing the spy request button that once pressed will
 * attempt to bribe a unit of the selected enemy for temporary vision sharing against resources.
 */
DiplomacyDialogPlayerControl.prototype.SpyRequestButton = class
{
	constructor(playerID)
	{
		this.playerID = playerID;

		// Players who requested a spy against this playerID.
		this.spyRequests = new Set();

		const id = "[" + (playerID - 1) + "]";
		this.diplomacySpyRequest = Engine.GetGUIObjectByName("diplomacySpyRequest" + id);
		this.diplomacySpyRequestImage = Engine.GetGUIObjectByName("diplomacySpyRequestImage" + id);

		this.diplomacySpyRequest.onPress = this.onPress.bind(this);
	}

	onPress()
	{
		Engine.PostNetworkCommand({
			"type": "spy-request",
			"player": this.playerID
		});
		this.spyRequests.add(g_ViewedPlayer);
		this.update(false);
	}

	/**
	 * Called from GUIInterface notification.
	 * @param player is the one who requested a spy.
	 * @param notification.target is the player who shall be spied upon.
	 */
	onSpyResponse(notification, player, playerInactive)
	{
		// Update the state if the response was against the current row (target player)
		if (notification.target == this.playerID)
		{
			this.spyRequests.delete(player);

			// Update UI if the currently viewed player sent the request
			if (player == g_ViewedPlayer)
				this.update(false);
		}
	}

	update(playerInactive)
	{
		const template = GetTemplateData(this.TemplateName);

		const hidden =
			playerInactive ||
			!template ||
			!!GetSimState().players[g_ViewedPlayer].disabledTemplates[this.TemplateName] ||
			g_Players[this.playerID].isMutualAlly[g_ViewedPlayer] &&
				!GetSimState().players[g_ViewedPlayer].hasSharedLos;

		this.diplomacySpyRequest.hidden = hidden;

		if (hidden)
			return;

		let tooltip = translate(this.Tooltip);

		if (template.requirements &&
			!Engine.GuiInterfaceCall("AreRequirementsMet", {
				"requirements": template.requirements,
				"player": g_ViewedPlayer
			}))
		{
			tooltip += "\n" + getRequirementsTooltip(
				false,
				template.requirements,
				GetSimState().players[g_ViewedPlayer].civ);

			this.diplomacySpyRequest.enabled = false;
			this.diplomacySpyRequest.tooltip = tooltip;
			this.diplomacySpyRequestImage.sprite = this.SpriteModifierDisabled + this.Sprite;
			return;
		}

		if (template.cost)
		{
			const modifiedTemplate = clone(template);

			for (const res in template.cost)
				modifiedTemplate.cost[res] =
					Math.floor(GetSimState().players[this.playerID].spyCostMultiplier * template.cost[res]);

			tooltip += "\n" + getEntityCostTooltip(modifiedTemplate);

			const neededResources = Engine.GuiInterfaceCall("GetNeededResources", {
				"cost": modifiedTemplate.cost,
				"player": g_ViewedPlayer
			});

			if (neededResources)
			{
				tooltip += "\n" + getNeededResourcesTooltip(neededResources);
				this.diplomacySpyRequest.enabled = false;
				this.diplomacySpyRequest.tooltip = tooltip;
				this.diplomacySpyRequestImage.sprite =
					resourcesToAlphaMask(neededResources) + ":" + this.Sprite;
				return;
			}

			const costRatio = Engine.GetTemplate(this.TemplateName).VisionSharing.FailureCostRatio;
			if (costRatio)
			{
				for (const res in modifiedTemplate.cost)
					modifiedTemplate.cost[res] = Math.floor(costRatio * modifiedTemplate.cost[res]);

				tooltip +=
					"\n" + translate(this.TooltipFailed) +
					"\n" + getEntityCostTooltip(modifiedTemplate);
			}
		}

		const enabled = !this.spyRequests.has(g_ViewedPlayer);
		this.diplomacySpyRequest.enabled = enabled && controlsPlayer(g_ViewedPlayer);
		this.diplomacySpyRequest.tooltip = tooltip;
		this.diplomacySpyRequestImage.sprite = (enabled ? "" : this.SpriteModifierDisabled) + this.Sprite;
	}
};

DiplomacyDialogPlayerControl.prototype.SpyRequestButton.prototype.TemplateName =
	"special/spy";

DiplomacyDialogPlayerControl.prototype.SpyRequestButton.prototype.Sprite =
	"stretched:" + "session/icons/bribes.png";

DiplomacyDialogPlayerControl.prototype.SpyRequestButton.prototype.SpriteModifierDisabled =
	"color:0 0 0 127:grayscale:";

DiplomacyDialogPlayerControl.prototype.SpyRequestButton.prototype.Tooltip =
	markForTranslation("Bribe a random unit from this player and share its vision during a limited period.");

DiplomacyDialogPlayerControl.prototype.SpyRequestButton.prototype.TooltipFailed =
	markForTranslation("A failed bribe will cost you:");
