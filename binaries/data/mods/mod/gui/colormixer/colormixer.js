/**
 * @class ColorMixer
 * This class allows to mix color using sliders one for each of three channels in range of 0 to 255 with step of 1, returning sanitized color when closing
 */
class ColorMixer
{
	constructor()
	{
		this.panel = Engine.GetGUIObjectByName("main");
		this.colorDisplay = Engine.GetGUIObjectByName("colorDisplay");

		this.color = [0, 0, 0];
		this.sliders = [];
		this.valuesText = [];
	}

	/**
	 * @param {String} color - initial color as RGB string e.g. 100 0 200
	 */
	async run(color)
	{
		Engine.GetGUIObjectByName("titleBar").caption = translate("Color");
		Engine.GetGUIObjectByName("infoLabel").caption = translate("Move the sliders to change the Red, Green and Blue components of the Color");

		const cancelHotkey = Engine.GetGUIObjectByName("cancelHotkey");

		const button = [];
		const closePromise =
			setButtonCaptionsAndVisibility(button, this.captions, cancelHotkey, "cmButton");
		distributeButtonsHorizontally(button, this.captions);

		const c = color.split(" ");

		const object0 = Engine.GetGUIObjectByName("color[0]");
		const height0 = object0.size.bottom - object0.size.top;
		for (let i = 0; i < this.color.length; i++)
		{
			this.color[i] = Math.floor(+c[i] || 0);
			const object = Engine.GetGUIObjectByName("color[" + i + "]");
			if (!object)
				continue;

			object.size.top = i * height0;
			object.size.bottom = (i + 1) * height0;

			this.sliders[i] = Engine.GetGUIObjectByName("colorSlider[" + i + "]");
			const slider = this.sliders[i];
			slider.min_value = 0;
			slider.max_value = 255;
			slider.value = this.color[i];
			slider.onValueChange = () => {this.updateFromSlider(i);};

			this.valuesText[i] = Engine.GetGUIObjectByName("colorValue[" + i + "]");
			this.valuesText[i].caption = this.color[i];

			Engine.GetGUIObjectByName("colorLabel[" + i + "]").caption = this.labels[i];
		}

		this.updateColorPreview();
		// Update return color on cancel to prevent malformed values from initial input.
		color = this.color.join(" ");

		return await closePromise === 0 ? color : this.color.join(" ");
	}

	updateFromSlider(index)
	{
		this.color[index] = Math.floor(this.sliders[index].value);
		this.valuesText[index].caption = this.color[index];
		this.updateColorPreview();
	}

	updateColorPreview()
	{
		this.colorDisplay.sprite = "color:" + this.color.join(" ");
	}
}

ColorMixer.prototype.labels = [translate("Red"), translate("Green"), translate("Blue")];
ColorMixer.prototype.captions = [translate("Cancel"), translate("Save")];

function init(color)
{
	return new ColorMixer().run(color);
}
