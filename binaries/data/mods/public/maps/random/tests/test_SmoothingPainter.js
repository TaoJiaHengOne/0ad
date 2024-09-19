Engine.GetTemplate = (path) => {
	return {
		"Identity": {
			"GenericName": null,
			"Icon": null,
			"History": null
		}
	};
};

Engine.LoadLibrary("rmgen");

function* GenerateMap()
{
	g_MapSettings = { "Size": 512 };

	let min = new Vector2D(5, 5);
	let center = new Vector2D(6, 6);
	let max = new Vector2D(7, 7);

	let minHeight = 20;
	let maxHeight = 25;

	// Test SmoothingPainter
	{
		globalThis.g_Map = new RandomMap(0, "blackness");

		let centerHeight = g_Map.getHeight(center);

		createArea(
			new RectPlacer(min, max),
			[
				new RandomElevationPainter(minHeight, maxHeight),
				new SmoothingPainter(2, 1, 1)
			]);

		TS_ASSERT_GREATER_EQUAL(g_Map.getHeight(center), centerHeight);
		TS_ASSERT_LESS_EQUAL(g_Map.getHeight(center), minHeight);
	}
}
