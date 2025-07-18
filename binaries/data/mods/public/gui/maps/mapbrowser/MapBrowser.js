class MapBrowser
{
	constructor(mapCache, mapFilters, setupWindow = undefined)
	{
		this.openPageHandlers = new Set();
		this.closePageHandlers = new Set();

		this.mapCache = mapCache;
		this.mapFilters = mapFilters;

		this.mapBrowserPage = Engine.GetGUIObjectByName("mapBrowserPage");
		this.mapBrowserPageDialog = Engine.GetGUIObjectByName("mapBrowserPageDialog");

		this.gridBrowser = new MapGridBrowser(this, setupWindow);
		this.controls = new MapBrowserPageControls(this, this.gridBrowser);

		this.open = false;
	}

	submitMapSelection()
	{
		if (!this.allowSelection)
			return;

		let file = this.gridBrowser.getSelected();
		let type = this.controls.MapFiltering.getSelectedMapType();
		let filter = this.controls.MapFiltering.getSelectedMapFilter();
		if (file)
		{
			type = file.mapType;
			filter = file.filter;
			file = file.file;
		}
		this.onSubmitMapSelection(
			file,
			type,
			filter
		);
		this.closePage();
	}

	// TODO: this is mostly gamesetup specific stuff.
	registerOpenPageHandler(handler)
	{
		this.openPageHandlers.add(handler);
	}

	registerClosePageHandler(handler)
	{
		this.closePageHandlers.add(handler);
	}

	openPage(allowSelection)
	{
		if (this.open)
			return;
		for (const handler of this.openPageHandlers)
			handler(allowSelection);

		this.allowSelection = allowSelection;
		this.open = true;
	}

	closePage()
	{
		if (!this.open)
			return;
		for (const handler of this.closePageHandlers)
			handler();
		this.open = false;
	}
}
