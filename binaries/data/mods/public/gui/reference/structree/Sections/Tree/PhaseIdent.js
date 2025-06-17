class PhaseIdent
{
	constructor(page, phaseIdx)
	{
		this.page = page;
		this.phaseIdx = +phaseIdx;

		this.Ident = Engine.GetGUIObjectByName("phase[" + this.phaseIdx + "]_ident");
		this.Icon = Engine.GetGUIObjectByName("phase[" + this.phaseIdx + "]_icon");
		this.Bars = Engine.GetGUIObjectByName("phase[" + this.phaseIdx + "]_bars");

		const prodIconSize = ProductionIcon.Size();
		const entityBoxHeight = EntityBox.IconAndCaptionHeight();
		for (let i = 0; i < this.Bars.children.length; ++i)
		{
			const bar = this.Bars.children[i];
			bar.size = {
				"top": entityBoxHeight + prodIconSize.rowHeight * (i + 1),
				"bottom": entityBoxHeight + prodIconSize.rowHeight * (i + 2) - prodIconSize.rowGap
			};
		}
	}

	draw(phaseList, civCode)
	{
		// Position ident
		this.Ident.size.top = TreeSection.getPositionOffset(this.phaseIdx, this.page.TemplateParser);
		this.Ident.size.bottom = TreeSection.getPositionOffset(this.phaseIdx + 1, this.page.TemplateParser);
		this.Ident.hidden = false;

		// Draw main icon
		this.drawPhaseIcon(this.Icon, this.phaseIdx, civCode);

		// Draw the phase bars
		let i = 1;
		for (; i < phaseList.length - this.phaseIdx; ++i)
		{
			const prodBar = this.Bars.children[(i - 1)];
			prodBar.hidden = false;

			this.drawPhaseIcon(prodBar.children[0], this.phaseIdx + i, civCode);
		}
		hideRemaining(this.Bars.name, i - 1);
	}

	drawPhaseIcon(phaseIcon, phaseIndex, civCode)
	{
		const phaseName = this.page.TemplateParser.phaseList[phaseIndex];
		const prodPhaseTemplate =
			this.page.TemplateParser.getTechnology(phaseName + "_" + civCode, civCode) ||
			this.page.TemplateParser.getTechnology(phaseName + "_generic", civCode) ||
			this.page.TemplateParser.getTechnology(phaseName, civCode);

		phaseIcon.sprite = "stretched:" + this.page.IconPath + prodPhaseTemplate.icon;
		phaseIcon.tooltip = getEntityNamesFormatted(prodPhaseTemplate);
	}
}
