/**
 * This class represents the Template Viewer GUI page.
 */
class ViewerPage extends ReferencePage
{
	constructor(closePageCallback)
	{
		super(closePageCallback);

		this.currentTemplate = undefined;

		this.guiElements = {
			"entityName": Engine.GetGUIObjectByName("entityName"),
			"entityIcon": Engine.GetGUIObjectByName("entityIcon"),
			"entityStats": Engine.GetGUIObjectByName("entityStats"),
			"entityInfo": Engine.GetGUIObjectByName("entityInfo"),
			"entityRankGlyph": Engine.GetGUIObjectByName("entityRankGlyph"),
		};

		const closeButton = new CloseButton(this);
	}

	selectTemplate(data)
	{
		if (!data || !data.templateName)
		{
			error("Viewer: No template provided");
			this.closePage();
			return;
		}

		const templateName = removeFiltersFromTemplateName(data.templateName);
		const isTech = TechnologyTemplateExists(templateName);

		// Attempt to get the civ code from the template, or, if
		// it's a technology, from the researcher's template.
		if (!isTech)
			this.setActiveCiv(this.TemplateLoader.loadEntityTemplate(templateName, this.TemplateLoader.DefaultCiv).Identity.Civ);

		if (this.activeCiv == this.TemplateLoader.DefaultCiv && data.civ)
			this.setActiveCiv(data.civ);

		this.currentTemplate = isTech ? this.TemplateParser.getTechnology(templateName, this.activeCiv) : this.TemplateParser.getEntity(templateName, this.activeCiv);
		if (!this.currentTemplate)
		{
			error("Viewer: unable to recognize or load template (" + templateName + ")");
			this.closePage();
			return;
		}

		// Here we compile lists of the names of all the entities that can build/train/research this template,
		//    and the entities and technologies that this template can build/train/research.
		// We do that here, so we don't do it later in the tooltip callback functions, as that would be messier.
		if (this.activeCiv != this.TemplateLoader.DefaultCiv)
		{
			let currentTemplateName = this.currentTemplate.name.internal;
			if (!isTech)
			{
				// If template is a non-promotion, non-upgrade variant of another (e.g.
				// units/{civ}/support_female_citizen_house), we wish to use the name of the base template,
				// not that of the variant template, when interacting with the compiled Template Lists.
				const templateVariance = this.TemplateLoader.getVariantBaseAndType(this.currentTemplate.name.internal, this.TemplateLoader.DefaultCiv);
				if (templateVariance[1].passthru)
					currentTemplateName = templateVariance[0];
			}

			const templateLists = this.TemplateLister.getTemplateLists(this.activeCiv);

			const builders = templateLists.structures.get(currentTemplateName);
			if (builders && builders.length)
				this.currentTemplate.builtByListOfNames = builders.map(builder => getEntityNames(this.TemplateParser.getEntity(builder, this.activeCiv)));

			const trainers = templateLists.units.get(currentTemplateName);
			if (trainers && trainers.length)
				this.currentTemplate.trainedByListOfNames = trainers.map(trainer => getEntityNames(this.TemplateParser.getEntity(trainer, this.activeCiv)));

			const researchers = templateLists.techs.get(currentTemplateName);
			if (researchers && researchers.length)
			{
				this.currentTemplate.researchedByListOfNames = researchers.map(researcher => getEntityNames(this.TemplateParser.getEntity(researcher, this.activeCiv)));
				const { techCostMultiplier } = this.TemplateParser.getEntity(researchers[0], this.activeCiv);
				for (const res in this.currentTemplate.cost)
					if (this.currentTemplate.cost[res])
						this.currentTemplate.cost[res] *= techCostMultiplier[res];
			}
		}

		if (this.currentTemplate.builder && this.currentTemplate.builder.length)
			this.currentTemplate.buildListOfNames = this.currentTemplate.builder.map(prod => getEntityNames(this.TemplateParser.getEntity(prod, this.activeCiv)));

		if (this.currentTemplate.production)
		{
			if (this.currentTemplate.production.units && this.currentTemplate.production.units.length)
				this.currentTemplate.trainListOfNames = this.currentTemplate.production.units.map(prod => getEntityNames(this.TemplateParser.getEntity(prod, this.activeCiv)));

			if (this.currentTemplate.production.techs && this.currentTemplate.production.techs.length)
			{
				this.currentTemplate.researchListOfNames = [];
				for (const tech of this.currentTemplate.production.techs)
				{
					const techTemplate = this.TemplateParser.getTechnology(tech, this.activeCiv);
					if (techTemplate.reqs)
						this.currentTemplate.researchListOfNames.push(getEntityNames(techTemplate));
				}
			}
		}

		if (this.currentTemplate.upgrades)
			this.currentTemplate.upgradeListOfNames = this.currentTemplate.upgrades.map(upgrade =>
				getEntityNames(upgrade.name ? upgrade : this.TemplateParser.getEntity(upgrade.entity, this.activeCiv))
			);

		this.draw();
	}

	/**
	 * Populate the UI elements.
	 */
	draw()
	{
		this.guiElements.entityName.caption = getEntityNamesFormatted(this.currentTemplate);

		const entityIcon = this.guiElements.entityIcon;
		entityIcon.sprite = "stretched:" + this.IconPath + this.currentTemplate.icon;

		const entityStats = this.guiElements.entityStats;
		entityStats.caption = this.constructor.buildText(this.currentTemplate, this.StatsFunctions);

		const infoTopMargin = 8;
		this.guiElements.entityInfo.size.top = Math.max(entityIcon.size.bottom + infoTopMargin, entityStats.size.top + entityStats.getTextSize().height + infoTopMargin);

		this.guiElements.entityInfo.caption = this.constructor.buildText(this.currentTemplate, this.InfoFunctions, "\n\n");

		if (this.currentTemplate.promotion)
			this.guiElements.entityRankGlyph.sprite = "stretched:" + this.RankIconPath + this.currentTemplate.promotion.current_rank + ".png";
		this.guiElements.entityRankGlyph.hidden = !this.currentTemplate.promotion;
	}

	closePage()
	{
		this.closePageCallback({ "civ": this.activeCiv, "page": "page_viewer.xml" });
	}
}

/**
 * The result of these functions appear to the right of the entity icon
 * on the page.
 */
ViewerPage.prototype.StatsFunctions = [getEntityCostTooltip].concat(ReferencePage.prototype.StatsFunctions);

/**
 * Used to display textual information and the build/train lists of the
 * template being displayed.
 *
 * At present, these are drawn in the main body of the page.
 *
 * The functions listed can be found in gui/common/tooltips.js or
 * gui/reference/common/tooltips.js
 */
ViewerPage.prototype.InfoFunctions = [
	getEntityTooltip,
	getHistoryTooltip,
	getDescriptionTooltip,
	getAurasTooltip,
	getVisibleEntityClassesFormatted,
	getBuiltByText,
	getTrainedByText,
	getResearchedByText,
	getBuildText,
	getTrainText,
	getResearchText,
	getUpgradeText
];

ViewerPage.prototype.CloseButtonTooltip =
	translate("%(hotkey)s: Close Template Viewer");

ViewerPage.prototype.RankIconPath = "session/icons/ranks/";
