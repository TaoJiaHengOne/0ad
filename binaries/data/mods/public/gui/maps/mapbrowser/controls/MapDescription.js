MapBrowserPageControls.prototype.MapDescription = class
{
	constructor(mapBrowserPage, gridBrowser)
	{
		this.ImageRatio = 4 / 3;

		this.mapBrowserPage = mapBrowserPage;
		this.gridBrowser = gridBrowser;
		this.mapCache = mapBrowserPage.mapCache;

		this.mapBrowserSelectedName = Engine.GetGUIObjectByName("mapBrowserSelectedName");
		this.mapBrowserSelectedPreview = Engine.GetGUIObjectByName("mapBrowserSelectedPreview");
		this.mapBrowserSelectedDescription = Engine.GetGUIObjectByName("mapBrowserSelectedDescription");

		const computedSize = this.mapBrowserSelectedPreview.getComputedSize();
		const top = this.mapBrowserSelectedName.size.bottom;
		const height = Math.floor((computedSize.right - computedSize.left) / this.ImageRatio);

		{
			const size = this.mapBrowserSelectedPreview.size;
			size.top = top;
			size.bottom = top + height;
			this.mapBrowserSelectedPreview.size = size;
		}

		{
			const size = this.mapBrowserSelectedDescription.size;
			size.top = top + height + 10;
			this.mapBrowserSelectedDescription.size = size;
		}

		gridBrowser.registerSelectionChangeHandler(this.onSelectionChange.bind(this));
	}

	onSelectionChange()
	{
		const map = this.gridBrowser.mapList[this.gridBrowser.selected];
		if (!map)
			return;

		this.mapBrowserSelectedName.caption = map ? map.name : "";
		this.mapBrowserSelectedDescription.caption = map ? map.description : "";

		this.mapBrowserSelectedPreview.sprite =
			this.mapCache.getMapPreview(
				this.mapBrowserPage.controls.MapFiltering.getSelectedMapType(),
				map.file);
	}
};
