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

export function* generateMap()
{
	g_MapSettings = { "Size": 512 };
	globalThis.g_Map = new RandomMap(0, "blackness");

	yield 50;

	const min = new Vector2D(5, 5);
	const max = new Vector2D(7, 7);

	const area = createArea(new RectPlacer(min, max));

	TS_ASSERT(!area.contains(new Vector2D(-1, -1).add(min)));
	TS_ASSERT(area.contains(min));
	TS_ASSERT(area.contains(max));
	TS_ASSERT(area.contains(max.clone()));
	TS_ASSERT(area.contains(Vector2D.average([min, max])));
}
