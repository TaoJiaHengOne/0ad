const UNIT_TEMPLATE = "units/athen/infantry_marine_archer_b";
const FAST_UNIT_TEMPLATE = "units/athen/cavalry_swordsman_a";
const LARGE_UNIT_TEMPLATE = "units/brit/siege_ram";
const SMALL_STRUCTURE_TEMPLATE = "structures/athen/house";

var QuickSpawn = function(x, z, template, owner = 1)
{
	const ent = Engine.AddEntity(template);

	const cmpEntOwnership = Engine.QueryInterface(ent, IID_Ownership);
	if (cmpEntOwnership)
		cmpEntOwnership.SetOwner(owner);

	const cmpEntPosition = Engine.QueryInterface(ent, IID_Position);
	cmpEntPosition.JumpTo(x, z);
	return ent;
};

var Rotate = function(angle, ent)
{
	const cmpEntPosition = Engine.QueryInterface(ent, IID_Position);
	cmpEntPosition.SetYRotation(angle);
	return ent;
};

var WalkTo = function(x, z, ent)
{
	ProcessCommand(1, {
		"type": "walk",
		"entities": Array.isArray(ent) ? ent : [ent],
		"x": x,
		"z": z,
		"queued": false
	});
	return ent;
};

var Guard = function(target, ent)
{
	ProcessCommand(1, {
		"type": "guard",
		"entities": Array.isArray(ent) ? ent : [ent],
		"target": target,
		"queued": false
	});
	return ent;
};

var Do = function(name, data, ent)
{
	const comm = {
		"type": name,
		"entities": Array.isArray(ent) ? ent : [ent],
		"queued": false
	};
	for (const k in data)
		comm[k] = data[k];
	ProcessCommand(1, comm);
};

var gx = 100;
var gy = 100;
var experiments = {};

experiments.single_unit = {
	"spawn": () => {
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
	}
};

experiments.two_followers = {
	"spawn": () => {
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
		const follow = WalkTo(gx, gy + 102, QuickSpawn(gx, gy + 2, UNIT_TEMPLATE));
		Guard(follow, QuickSpawn(gx, gy - 2, UNIT_TEMPLATE));
	}
};

experiments.follow_faster = {
	"spawn": () => {
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, FAST_UNIT_TEMPLATE));
		WalkTo(gx, gy + 104, QuickSpawn(gx, gy + 4, UNIT_TEMPLATE));
	}
};

experiments.group = {
	"spawn": () => {
		for (let i = -3; i <= 3; ++i)
			for (let j = -3; j <= 3; ++j)
				WalkTo(gx, gy + 100, QuickSpawn(gx + i * 2, gy + j * 2, UNIT_TEMPLATE));
	}
};

experiments.single_unit_building = {
	"spawn": () => {
		QuickSpawn(gx, gy + 40, SMALL_STRUCTURE_TEMPLATE);
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
	}
};

experiments.single_unit_pass_nopass = {
	"spawn": () => {
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
		QuickSpawn(gx - 10, gy + 40, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx + 10, gy + 40, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 8, gy + 70, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx + 8, gy + 70, SMALL_STRUCTURE_TEMPLATE);
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy - 10, UNIT_TEMPLATE));
	}
};


experiments.units_dense_forest = {
	"spawn": () => {
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx - 2, gy, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx + 2, gy + 2, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx - 2, gy, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx + 2, gy + 2, UNIT_TEMPLATE));
		for (let i = -16; i <= 16; i += 4)
			for (let j = -16; j <= 16; j += 4)
				QuickSpawn(gx + i, gy + 50 + j, "gaia/tree/acacia", 0);
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy - 20, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx - 2, gy - 20, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx + 2, gy - 18, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx - 2, gy - 20, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx + 2, gy - 18, UNIT_TEMPLATE));
	}
};

experiments.units_sparse_forest = {
	"spawn": () => {
		gx += 10;
		for (let i = -4; i <= 4; i += 2)
			for (let j = -4; j <= 4; j += 2)
				WalkTo(gx, gy + 100, QuickSpawn(gx + i, gy + j, UNIT_TEMPLATE));

		for (let i = -16; i <= 16; i += 8)
			for (let j = -16; j <= 16; j += 8)
				QuickSpawn(gx + i, gy + 50 + j, "gaia/tree/acacia", 0);

		for (let i = -4; i <= 4; i += 2)
			for (let j = -4; j <= 4; j += 2)
				WalkTo(gx, gy + 100, QuickSpawn(gx + i, gy - 20 + j, UNIT_TEMPLATE));
	}
};

experiments.units_running_into_eachother = {
	"spawn": () => {
		for (let i = -4; i <= 4; i += 2)
			for (let j = -4; j <= 4; j += 2)
				WalkTo(gx, gy + 100, QuickSpawn(gx + i, gy + j, UNIT_TEMPLATE));
		for (let i = -4; i <= 4; i += 2)
			for (let j = -4; j <= 4; j += 2)
				WalkTo(gx, gy, QuickSpawn(gx + i, gy + 100 + j, UNIT_TEMPLATE));
	}
};

experiments.enclosed = {
	"spawn": () => {
		QuickSpawn(gx, gy - 8, "structures/palisades_long");
		QuickSpawn(gx, gy + 8, "structures/palisades_long");
		Rotate(Math.PI / 2, QuickSpawn(gx - 8, gy, "structures/palisades_long"));
		Rotate(Math.PI / 2, QuickSpawn(gx + 8, gy, "structures/palisades_long"));

		WalkTo(gx + 100, gy + 100, QuickSpawn(gx - 1, gy - 1, UNIT_TEMPLATE));
		WalkTo(gx - 100, gy + 100, QuickSpawn(gx + 1, gy - 1, UNIT_TEMPLATE));
		WalkTo(gx - 100, gy - 100, QuickSpawn(gx + 1, gy + 1, UNIT_TEMPLATE));
		WalkTo(gx + 100, gy - 100, QuickSpawn(gx - 1, gy + 1, UNIT_TEMPLATE));

		QuickSpawn(gx, gy + 20, "structures/palisades_long");
		experiments.enclosed.remove = QuickSpawn(gx, gy + 36, "structures/palisades_long");
		Rotate(Math.PI / 2, QuickSpawn(gx - 8, gy + 28, "structures/palisades_long"));
		Rotate(Math.PI / 2, QuickSpawn(gx + 8, gy + 28, "structures/palisades_long"));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy + 28, UNIT_TEMPLATE));
	},

	"timeout": () => {
		Engine.DestroyEntity(experiments.enclosed.remove);
	}
};

experiments.line_of_rams = {
	"spawn": () => {
		for (let i = -20; i <= 20; i+=4)
			WalkTo(gx + i, gy + 50, QuickSpawn(gx + i, gy, LARGE_UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy - 10, UNIT_TEMPLATE));
	}
};


experiments.units_sparse_forest_of_units = {
	"spawn": () => {
		for (let i = -16; i <= 16; i += 8)
			for (let j = -16; j <= 16; j += 8)
				Do("stance", { "name": "standground" }, QuickSpawn(gx + i, gy + 50 + j, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy-10, LARGE_UNIT_TEMPLATE));
	}
};

experiments.units_dense_forest_of_units = {
	"spawn": () => {
		for (let i = -16; i <= 16; i += 4)
			for (let j = -16; j <= 16; j += 4)
				Do("stance", { "name": "standground" }, QuickSpawn(gx + i, gy + 50 + j, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy-10, LARGE_UNIT_TEMPLATE));
	}
};

experiments.units_superdense_forest_of_units = {
	"spawn": () => {
		for (let i = -6; i <= 6; i += 2)
			for (let j = -6; j <= 6; j += 2)
				Do("stance", { "name": "standground" }, QuickSpawn(gx + i, gy + 50 + j, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy, UNIT_TEMPLATE));
		WalkTo(gx, gy + 100, QuickSpawn(gx, gy-10, LARGE_UNIT_TEMPLATE));
	}
};

experiments.u_shape_tight_exit = {
	"spawn": () => {
		Rotate(Math.PI / 2, QuickSpawn(gx + 8, gy, "structures/palisades_long"));
		Rotate(Math.PI / 2, QuickSpawn(gx + 8, gy + 12, "structures/palisades_long"));
		QuickSpawn(gx, gy + 16, "structures/palisades_long");
		Rotate(Math.PI / 2, QuickSpawn(gx, gy + 4, "structures/palisades_medium"));
		Rotate(Math.PI / 2, QuickSpawn(gx - 8, gy + 12, "structures/palisades_long"));
		Rotate(Math.PI / 2, QuickSpawn(gx - 16, gy + 8, "structures/palisades_long"));
		QuickSpawn(gx - 4, gy, "structures/palisades_medium");
		QuickSpawn(gx - 12, gy, "structures/palisades_medium");
		Rotate(Math.PI/4, QuickSpawn(gx - 10, gy + 8, "structures/palisades_short"));
		QuickSpawn(gx, gy - 10, "structures/palisades_long");
		Rotate(Math.PI / 2, QuickSpawn(gx + 8, gy - 5, "structures/palisades_long"));
		Rotate(Math.PI / 2, QuickSpawn(gx - 8, gy - 7, "structures/palisades_long"));

		const tree = QuickSpawn(gx, gy + 80, "gaia/tree/acacia");

		for (let i = -3; i <= 3; i += 1)
		{
			for (let j = -2; j <= 3; j += 1)
				WalkTo(gx, gy + 100, QuickSpawn(gx + i, gy + j - 5, UNIT_TEMPLATE));
			Do("gather", { "target": tree }, QuickSpawn(gx + i, gy - 3 - 5, UNIT_TEMPLATE));
		}
	}
};

experiments.cluttered_around_tree = {
	"spawn": () => {
		const tree = QuickSpawn(gx, gy + 50, "gaia/tree/acacia");

		for (let i = -3; i <= 3; i += 1)
			for (let j = -3; j <= 3; j += 1)
				if (Math.abs(j) == 3 || Math.abs(i) == 3)
					QuickSpawn(gx + i, gy + j + 50, UNIT_TEMPLATE);

		Do("gather", { "target": tree }, QuickSpawn(gx, gy, UNIT_TEMPLATE));
	}
};

experiments.formation_move = {
	"spawn": () => {
		const ents = [];
		for (let i = -3; i <= 3; ++i)
			for (let j = -3; j <= 3; ++j)
				ents.push(QuickSpawn(gx + i * 2, gy + j * 2, UNIT_TEMPLATE));
		Do("formation", { "name": "special/formations/box" }, ents);
		WalkTo(gx, gy + 100, ents);
	}
};

experiments.formation_attack = {
	"spawn": () => {
		const ents = [];
		for (let i = -3; i <= 3; ++i)
			for (let j = -3; j <= 3; ++j)
				ents.push(QuickSpawn(gx + i * 2, gy + j * 2, UNIT_TEMPLATE));
		Do("formation", { "name": "special/formations/box" }, ents);
		Do("attack", { "target": 5 }, ents);
	}
};

experiments.multiple_resources = {
	"spawn": () => {
		QuickSpawn(gx, gy + 80, "structures/athen/civil_centre");

		const chicken = QuickSpawn(gx, gy + 50, "gaia/fauna_chicken");
		QuickSpawn(gx + 3, gy + 50, "gaia/fauna_chicken");
		QuickSpawn(gx - 3, gy + 50, "gaia/fauna_chicken");

		Do("gather", { "target": chicken }, QuickSpawn(gx, gy, UNIT_TEMPLATE));
	}
};


/**
 * The unit is, from the long pathfinder's perspective,
 * stuck inside, and also can't reach the goal.
 * However, the vertex pathfinder should be able to find a way out, then a way in.
 */
experiments.locked_within = {
	"spawn": () => {
		QuickSpawn(gx + 10, gy + 7, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 10, gy + 7, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx, gy, SMALL_STRUCTURE_TEMPLATE);

		QuickSpawn(gx + 8, gy + 20, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 8, gy + 20, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx + 8, gy + 30, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 8, gy + 30, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx + 8, gy + 40, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 8, gy + 40, SMALL_STRUCTURE_TEMPLATE);

		QuickSpawn(gx + 10, gy + 93, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 10, gy + 93, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx, gy + 100, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx + 8, gy + 80, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 8, gy + 80, SMALL_STRUCTURE_TEMPLATE);

		WalkTo(gx, gy + 90, QuickSpawn(gx, gy + 10, UNIT_TEMPLATE));
	}
};


/**
 * The long-pathfinder finds a direct path, but it's blocked by units.
 * so the short-pathfinder has to backtrack.
 */
experiments.need_to_backtrack = {
	"spawn": () => {
		gx += 50;
		QuickSpawn(gx + 10, gy + 60, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 10, gy + 60, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx + 20, gy + 50, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 20, gy + 50, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx + 30, gy + 40, SMALL_STRUCTURE_TEMPLATE);
		QuickSpawn(gx - 30, gy + 40, SMALL_STRUCTURE_TEMPLATE);
		for (let i = -4; i <= 4; ++i)
			QuickSpawn(gx + i, gy + 65, UNIT_TEMPLATE);

		WalkTo(gx, gy + 90, QuickSpawn(gx, gy + 10, UNIT_TEMPLATE));
	}
};


/**
 * Regression test for #5795
 * Before the fix, the units were pathing to the bottom-right dead-end,
 * before going back through the corridor.
 * After the fix, that should mostly not happen: units should just wait.
 * (note that it's not an entirely perfect fix, but it should just be a few units, not half)
 */
experiments.small_exit_of_hill = {
	"spawn": () => {
		const x = 350;
		const y = 615;
		for (let i = -5; i <= 5; i += 1)
			for (let j = -5; j <= 5; j += 1)
				WalkTo(350, 500, QuickSpawn(x + i, y + j, UNIT_TEMPLATE));
	}
};

var cmpTrigger = Engine.QueryInterface(SYSTEM_ENTITY, IID_Trigger);

Trigger.prototype.SetupUnits = function()
{
	for (const key in experiments)
	{
		experiments[key].spawn();
		gx += 40;
		if (gx >= 540)
		{
			gx = 100;
			gy += 140;
		}
		if (experiments[key].timeout)
		{
			Trigger.prototype[key + '_timeout'] = experiments[key].timeout;
			cmpTrigger.DoAfterDelay(4000, key + '_timeout', {});
		}

	}
};

cmpTrigger.DoAfterDelay(3000, "SetupUnits", {});
