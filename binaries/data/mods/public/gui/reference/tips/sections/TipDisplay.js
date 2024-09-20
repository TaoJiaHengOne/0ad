/**
 * This class is concerned with chosing and displaying tips about how to play the game.
 * This includes a text and an image.
 */
class TipDisplay
{
	/**
	 * @param {boolean} tipScrolling - Whether or not to enable the player to scroll through the tips.
	 */
	constructor(tipScrolling = true)
	{
		this.tipImage = Engine.GetGUIObjectByName("tipImage");
		this.tipTitle = Engine.GetGUIObjectByName("tipTitle");
		this.tipTitleDecoration = Engine.GetGUIObjectByName("tipTitleDecoration");
		this.tipText = Engine.GetGUIObjectByName("tipText");
		this.previousTipButton = Engine.GetGUIObjectByName("previousTipButton");
		this.nextTipButton = Engine.GetGUIObjectByName("nextTipButton");

		this.previousTipButton.caption = this.CaptionPreviousTip;
		this.previousTipButton.tooltip = colorizeHotkey("%(hotkey)s: ", "item.prev") + this.TooltipPreviousTip;

		this.nextTipButton.caption = this.CaptionNextTip;
		this.nextTipButton.tooltip = colorizeHotkey("%(hotkey)s: ", "item.next") + this.TooltipNextTip;

		this.tipFiles = shuffleArray(listFiles(this.TextPath, ".txt", false));
		if (this.tipFiles.length === 0)
		{
			warn("Failed to find any tips to display.");
			return;
		}

		const hideButtons = !tipScrolling || this.tipFiles.length < 2;
		this.previousTipButton.hidden = hideButtons;
		this.nextTipButton.hidden = hideButtons;
		this.tipFilesIndex = -1;
		this.nextTipButton.onPress = () => this.onIndexChange(1);
		this.previousTipButton.onPress = () => this.onIndexChange(-1);
		this.onIndexChange(1);
	}

	onIndexChange(number)
	{
		this.tipFilesIndex += number;

		this.tipFilesIndex = Math.max(0, Math.min(this.tipFilesIndex, this.tipFiles.length - 1));

		this.displayTip(this.tipFiles[this.tipFilesIndex]);
		this.rebuildButtons();
	}

	rebuildButtons()
	{
		this.previousTipButton.enabled = this.tipFilesIndex !== 0 && !this.previousTipButton.hidden;
		this.nextTipButton.enabled = this.tipFilesIndex < this.tipFiles.length - 1 && !this.nextTipButton.hidden;
	}

	displayTip(tipFile)
	{
		this.tipImage.sprite = "stretched:" + this.ImagePath + tipFile + ".png";

		const tipText = Engine.TranslateLines(Engine.ReadFile(
			this.TextPath + tipFile + ".txt")).split("\n");

		this.tipTitle.caption = tipText.shift();
		this.scaleGuiElementsToFit();
		this.tipText.caption = tipText.map(text =>
			text && "[icon=\"BulletPoint\"] " + text).join("\n\n");
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
		textSize.top = this.tipTitleDecoration.size.bottom + 10;
		this.tipText.size = textSize;
	}

}

TipDisplay.prototype.CaptionPreviousTip = translateWithContext("button", "Previous");
TipDisplay.prototype.TooltipPreviousTip = translate("Switch to the previous tip.");
TipDisplay.prototype.CaptionNextTip = translateWithContext("button", "Next");
TipDisplay.prototype.TooltipNextTip = translate("Switch to the next tip.");

/**
 * Directory storing .txt files containing the gameplay tips.
 */
TipDisplay.prototype.TextPath = "gui/reference/tips/texts/";

/**
 * Directory storing the .png images with filenames corresponding to the tip text files.
 */
TipDisplay.prototype.ImagePath = "tips/";
