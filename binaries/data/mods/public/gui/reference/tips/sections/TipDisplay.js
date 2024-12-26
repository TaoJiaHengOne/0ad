/**
 * This class is concerned with chosing and displaying tips about how to play the game.
 * This includes a text and one or more images.
 */
class TipDisplay
{
	/**
	 * @param {boolean} initData.tipScrolling - Whether or not to enable the player to scroll through the tips and the tip images.
	 * @param {Array|undefined} hotloadData.tipFilesData - Hotloaded value storing last time's tipFilesData.
	 * @param {number|undefined} hotloadData.tipIndex - Hotloaded value pointing to a specific tip.
	 * @param {number|undefined} hotloadData.tipImageIndex - Hotloaded value pointing to a specific tip image.
	 */
	constructor(initData, hotloadData)
	{
		this.tipImage = Engine.GetGUIObjectByName("tipImage");
		this.tipTitle = Engine.GetGUIObjectByName("tipTitle");
		this.tipTitleDecoration = Engine.GetGUIObjectByName("tipTitleDecoration");
		this.tipText = Engine.GetGUIObjectByName("tipText");
		this.tipImageText = Engine.GetGUIObjectByName("tipImageText");
		this.previousTipButton = Engine.GetGUIObjectByName("previousTipButton");
		this.imageControlPanel = Engine.GetGUIObjectByName("imageControlPanel");
		this.nextTipButton = Engine.GetGUIObjectByName("nextTipButton");
		this.previousImageButton = Engine.GetGUIObjectByName("previousImageButton");
		this.nextImageButton = Engine.GetGUIObjectByName("nextImageButton");

		this.previousTipButton.caption = this.CaptionPreviousTip;
		this.nextTipButton.caption = this.CaptionNextTip;

		this.previousTipButton.tooltip = colorizeHotkey("%(hotkey)s: ", "item.prev") + this.TooltipPreviousTip;
		this.nextTipButton.tooltip = colorizeHotkey("%(hotkey)s: ", "item.next") + this.TooltipNextTip;
		this.previousImageButton.tooltip = this.TooltipPreviousImage;
		this.nextImageButton.tooltip = this.TooltipNextImage;

		this.tipFilesData =
			hotloadData?.tipFilesData ||
			shuffleArray(
				Engine.ReadJSONFile(this.TipFilesDataFile)
			).map(tip => {
				tip.imageFiles = shuffleArray(tip.imageFiles);
				return tip;
			});

		this.currentTip = {};
		this.tipIndex = -1;
		this.tipImageIndex = -1;

		this.enableTipScrolling = initData.tipScrolling;
		const hideButtons = !initData.tipScrolling || this.tipFilesData.length < 2;
		this.previousTipButton.hidden = hideButtons;
		this.nextTipButton.hidden = hideButtons;

		this.previousTipButton.onPress = () => this.onTipIndexChange(-1);
		this.nextTipButton.onPress = () => this.onTipIndexChange(1);
		this.previousImageButton.onPress = () => this.onTipImageIndexChange(-1);
		this.nextImageButton.onPress = () => this.onTipImageIndexChange(1);

		this.onTipIndexChange(hotloadData?.tipIndex ? hotloadData.tipIndex + 1 : 1);
		if (hotloadData?.tipImageIndex)
			this.onTipImageIndexChange(hotloadData.tipImageIndex + 1);
	}

	getHotloadData()
	{
		return {
			"tipFilesData": this.tipFilesData,
			"tipIndex": this.tipIndex,
			"tipImageIndex": this.tipImageIndex
		};
	}

	onTipIndexChange(number)
	{
		this.tipIndex += number;
		this.tipIndex = Math.max(0, Math.min(this.tipIndex, this.tipFilesData.length - 1));
		this.currentTip = this.tipFilesData[this.tipIndex];

		this.updateTipText();
		this.rebuildTipButtons();
		this.onTipImageIndexChange(-this.tipImageIndex);
	}

	onTipImageIndexChange(number)
	{
		this.tipImageIndex += number;
		this.tipImageIndex = Math.max(0, Math.min(this.tipImageIndex, this.currentTip.imageFiles.length - 1));

		this.updateTipImage();
		this.rebuildTipImageButtons();
	}

	updateTipText()
	{
		const tipText = Engine.TranslateLines(Engine.ReadFile(this.TextPath + this.currentTip.textFile)).split("\n");

		this.tipTitle.caption = tipText.shift();
		this.scaleGuiElementsToFit();
		this.tipText.caption = tipText.map(text =>
			text && "[icon=\"BulletPoint\"] " + text).join("\n\n");
	}

	updateTipImage()
	{
		this.tipImage.sprite = "stretched:" + this.ImagePath + this.currentTip.imageFiles[this.tipImageIndex];
		this.imageControlPanel.hidden = !this.enableTipScrolling || this.currentTip.imageFiles.length === 1;

		if (!this.imageControlPanel.hidden)
			this.tipImageText.caption = (this.tipImageIndex + 1) + "  /  " + this.currentTip.imageFiles.length;
	}

	rebuildTipButtons()
	{
		this.previousTipButton.enabled = !this.previousTipButton.hidden && this.tipIndex !== 0;
		this.nextTipButton.enabled = !this.nextTipButton.hidden && this.tipIndex < this.tipFilesData.length - 1;
	}

	rebuildTipImageButtons()
	{
		this.previousImageButton.enabled = this.tipImageIndex > 0;
		this.nextImageButton.enabled = this.tipImageIndex < this.currentTip.imageFiles.length - 1;
	}

	scaleGuiElementsToFit()
	{
		const titleSize = this.tipTitle.size;
		const titleTextSize = this.tipTitle.getTextSize();

		titleSize.bottom = titleSize.top + titleTextSize.height;
		this.tipTitle.size = titleSize;

		this.tipTitleDecoration.size = new GUISize(
			-(titleTextSize.width / 2 + 12), this.tipTitle.size.bottom - 4, titleTextSize.width / 2 + 12, this.tipTitle.size.bottom + 12,
			50, 0, 50, 0
		);

		const textSize = this.tipText.size;
		textSize.top = this.tipTitleDecoration.size.bottom + 16;
		this.tipText.size = textSize;
	}

}

TipDisplay.prototype.CaptionPreviousTip = translateWithContext("button", "Previous");
TipDisplay.prototype.TooltipPreviousTip = translate("Switch to the previous tip.");
TipDisplay.prototype.CaptionNextTip = translateWithContext("button", "Next");
TipDisplay.prototype.TooltipNextTip = translate("Switch to the next tip.");

TipDisplay.prototype.TooltipPreviousImage = translate("Switch to the previous image.");
TipDisplay.prototype.TooltipNextImage = translate("Switch to the next image.");

/**
 * JSON file assigning one or more tip image files (.png) to each tip text file (.txt).
 */
TipDisplay.prototype.TipFilesDataFile = "gui/reference/tips/tipfiles.json";

/**
 * Directory storing .txt files containing the gameplay tips.
 */
TipDisplay.prototype.TextPath = "gui/reference/tips/texts/";

/**
 * Subdirectory of art/textures/ui storing the .png images illustrating the tips.
 */
TipDisplay.prototype.ImagePath = "tips/";
