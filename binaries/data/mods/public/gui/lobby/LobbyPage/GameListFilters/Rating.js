GameListFilters.Rating = class
{
	constructor(onFilterChange)
	{
		this.enabled = undefined;
		this.onFilterChange = onFilterChange;
		this.filter = () => true;

		this.gameRatingFilter = Engine.GetGUIObjectByName("gameRatingFilter");
		this.gameRatingFilter.list = [
			translateWithContext("map", "Any"),
			...this.RatingFilters
		];
		this.gameRatingFilter.list_data = [
			"",
			...this.RatingFilters.map(r =>
				sprintf(
					r[0] == ">" ?
						translateWithContext("gamelist filter", "> %(rating)s") :
						translateWithContext("gamelist filter", "< %(rating)s"),
					{
						"rating": r.substr(1)
					}))
		];

		this.gameRatingFilter.selected = 0;
		this.gameRatingFilter.onSelectionChange = this.onSelectionChange.bind(this);

		this.setEnabled(Engine.ConfigDB_GetValue("user", "lobby.columns.gamerating") == "true");
	}

	setEnabled(enabled)
	{
		this.enabled = enabled;
		this.gameRatingFilter.hidden = !this.enabled;
		if (!enabled)
			return;

		// TODO: COList should expose the precise column width
		// Hide element to compensate width
		const mapTypeFilter = Engine.GetGUIObjectByName("mapTypeFilter");
		mapTypeFilter.hidden = enabled;
		const playersNumberFilter = Engine.GetGUIObjectByName("playersNumberFilter");
		playersNumberFilter.size.rleft = mapTypeFilter.size.rleft;
		playersNumberFilter.size.rright = this.gameRatingFilter.size.rleft;
	}

	onSelectionChange()
	{
		const selectedType = this.gameRatingFilter.list_data[this.gameRatingFilter.selected];
		const selectedRating = +selectedType.substr(1);

		this.filter =
			(!this.enabled || !selectedType) ?
				() => true :
				selectedType.startsWith(">") ?
					game => game.gameRating >= selectedRating :
					game => game.gameRating < selectedRating;

		this.onFilterChange();
	}
};

GameListFilters.Rating.prototype.RatingFilters = [
	">1500",
	">1400",
	">1300",
	">1200",
	"<1200",
	"<1100",
	"<1000"
];
