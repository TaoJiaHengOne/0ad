class BackgroundHandler
{
	constructor(layers)
	{
		this.backgroundLayers = layers.map((layer, i) =>
			new BackgroundLayer(layer, i));

		this.initTime = Date.now();

		this.backgrounds = Engine.GetGUIObjectByName("backgrounds");
		this.backgrounds.onTick = this.onTick.bind(this);
		this.backgrounds.onWindowResized = this.onWindowResized.bind(this);
		this.onWindowResized();
	}

	onWindowResized()
	{
		const size = this.backgrounds.getComputedSize();
		this.backgroundsSize = deepfreeze(new GUISize(size.top, size.left, size.right, size.bottom));
	}

	onTick()
	{
		const time = Date.now() - this.initTime;
		for (const background of this.backgroundLayers)
			background.update(time, this.backgroundsSize);
	}
}

class BackgroundLayer
{
	constructor(layer, i)
	{
		this.layer = layer;

		this.background = Engine.GetGUIObjectByName("background[" + i + "]");
		this.background.sprite = this.layer.sprite;
		this.background.z = i;
		this.background.hidden = false;
	}

	update(time, backgroundsSize)
	{
		const height = backgroundsSize.bottom - backgroundsSize.top;
		const width = height * this.AspectRatio;
		const offset = this.layer.offset(time / 1000, width);

		if (this.layer.tiling)
		{
			const iw = height * 2;
			let left = offset % iw;
			if (left >= 0)
				left -= iw;
			this.background.size = new GUISize(
				left,
				backgroundsSize.top,
				backgroundsSize.right,
				backgroundsSize.bottom);
		}
		else
		{
			const right = backgroundsSize.right / 2 + offset;
			this.background.size = new GUISize(
				right - height,
				backgroundsSize.top,
				right + height,
				backgroundsSize.bottom);
		}
	}
}

BackgroundLayer.prototype.AspectRatio = 16 / 9;
