/**
 * This code wraps the gui representing buildable structures within the structree.
 *
 * An instance of this class is created for each child of the gui element named "structures".
 */
class StructureBox extends EntityBox
{
	constructor(page, structureIdx)
	{
		super(page);
		this.gui = Engine.GetGUIObjectByName("structure[" + structureIdx + "]");
		this.ProductionRows = new ProductionRowManager(this.page, "structure[" + structureIdx + "]_productionRows", true);
	}

	draw(templateName, civCode, runningWidths)
	{
		super.draw(templateName, civCode);

		this.phaseIdx = this.page.TemplateParser.phaseList.indexOf(this.template.phase);

		// Draw the production rows
		this.ProductionRows.draw(this.template, civCode, this.phaseIdx);

		const boxWidth = Math.max(this.MinWidth, this.captionWidth(), this.ProductionRows.width);

		// Set position of the Structure Box
		Object.assign(this.gui.size, {
			"left": this.HMargin + runningWidths[this.phaseIdx], "top": TreeSection.getPositionOffset(this.phaseIdx, this.page.TemplateParser),
			"right": this.HMargin + runningWidths[this.phaseIdx] + boxWidth, "bottom": TreeSection.getPositionOffset(this.phaseIdx + 1, this.page.TemplateParser) - this.VMargin
		});

		// Update new right-side-edge dimension
		runningWidths[this.phaseIdx] += boxWidth + this.HMargin / 2;
	}
}
