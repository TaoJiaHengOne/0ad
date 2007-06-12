var brushShapes = {
	'circle': {
		width: function (size) { return size },
		height: function (size) { return size },
		data: function (size) {
			var data = [];
			// All calculations are done in units of half-tiles, since that
			// is the required precision
			var mid_x = size-1;
			var mid_y = size-1;
			var scale = 1 / (Math.sqrt(2) - 1);
			for (var y = 0; y < size; ++y)
			{
				for (var x = 0; x < size; ++x)
				{
					var dist_sq = // scaled to 0 in centre, 1 on edge
					    ((2*x - mid_x)*(2*x - mid_x) +
					     (2*y - mid_y)*(2*y - mid_y)) / (size*size);
					if (dist_sq <= 1)
						data.push((Math.sqrt(2 - dist_sq) - 1) * scale);
					else
						data.push(0);
				}
			}
			return data;
		}
	},

	'square': {
		width: function (size) { return size },
		height: function (size) { return size },
		data: function (size) {
			var data = [];
			for (var i = 0; i < size*size; ++i)
				data.push(1);
			return data;
		}
	}
};

var brush = {
	shape: brushShapes['circle'],
	size: 4,
	strength: 1.0,
	active: false,
	send: function () {
		Atlas.Message.Brush(
			this.shape.width(this.size),
			this.shape.height(this.size),
			this.shape.data(this.size)
		);
		// TODO: rather than this hack to make things interact correctly with C++ tools,
		// implement the tools in JS and do something better
		Atlas.SetBrushStrength(this.strength);
	}
};

function init(window)
{
	window.sizer = new wxBoxSizer(wxOrientation.VERTICAL);
		
	var tools = [
		/* text label; internal tool name; button */
		[ 'Modify', 'AlterElevation', undefined ],
		[ 'Flatten', 'FlattenElevation', undefined ],
		[ 'Paint', 'PaintTerrain', undefined ]
	];
	var selectedTool = null; // null if none selected, else an element of 'tools'

	var toolSizer = new wxStaticBoxSizer(new wxStaticBox(window, -1, 'Elevation tools'), wxOrientation.HORIZONTAL);
	window.sizer.add(toolSizer, 0, wxStretch.EXPAND);
	for each (var tool in tools)
	{
		var button = new wxButton(window, -1, tool[0]);
		toolSizer.add(button, 1);
		tool[2] = button;
		
		// Explicitly set the background to the default colour, so that the button
		// is always owner-drawn (by the wxButton code), rather than initially using the
		// native (standard colour) button appearance then changing inconsistently later.
		button.backgroundColour = wxSystemSettings.getColour(wxSystemSettings.COLOUR_BTNFACE);
		
		(function(tool) { // (local scope)
			button.onClicked = function () {
				if (selectedTool == tool)
				{
					// Clicking on one tool twice should disable it
					selectedTool = null;
					this.backgroundColour = wxSystemSettings.getColour(wxSystemSettings.COLOUR_BTNFACE);
					Atlas.SetCurrentTool('');
				}
				else
				{
					if (selectedTool)
						selectedTool[2].backgroundColour = wxSystemSettings.getColour(wxSystemSettings.COLOUR_BTNFACE);
					selectedTool = tool;
					this.backgroundColour = new wxColour(0xEE, 0xCC, 0x55);
					Atlas.SetCurrentTool(tool[1]);
					brush.send();
				}
			};
		})(tool);
	}
	
	var brushSizer = new wxStaticBoxSizer(new wxStaticBox(window, -1, 'Brush'), wxOrientation.VERTICAL);
	window.sizer.add(brushSizer);
	
	var shapes = [
		[ 'Circle', brushShapes['circle'] ],
		[ 'Square', brushShapes['square'] ]
	];
	var shapeNames = [];
	for each (var s in shapes) shapeNames.push(s[0]);
	var shapeBox = new wxRadioBox(window, -1, 'Shape', wxDefaultPosition, wxDefaultSize, shapeNames);
	brushSizer.add(shapeBox);
	shapeBox.onRadioBox = function(evt)
	{
		brush.shape = shapes[evt.integer][1];
		brush.send();
	};
	
	var brushSettingsSizer = new wxFlexGridSizer(2);
	
	brushSizer.add(brushSettingsSizer);
	brushSettingsSizer.add(new wxStaticText(window, -1, 'Size'), 0, wxAlignment.RIGHT);
	var sizeSpinner = new wxSpinCtrl(window, -1, 4, wxDefaultPosition, wxDefaultSize, wxSpinCtrl.ARROW_KEYS, 1, 100);
	brushSettingsSizer.add(sizeSpinner);
	sizeSpinner.onSpinCtrl = function(evt)
	{
		brush.size = evt.position;
		brush.send();
	};
	
	brushSettingsSizer.add(new wxStaticText(window, -1, 'Strength'), wxAlignment.RIGHT);
	var strengthSpinner = new wxSpinCtrl(window, -1, 10, wxDefaultPosition, wxDefaultSize, wxSpinCtrl.ARROW_KEYS, 1, 100);
	brushSettingsSizer.add(strengthSpinner);
	strengthSpinner.onSpinCtrl = function(evt)
	{
		brush.strength = evt.position / 10;
		brush.send();
	};
}
