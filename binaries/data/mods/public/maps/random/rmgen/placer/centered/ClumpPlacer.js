/**
 * Generates a roughly circular clump of points.
 *
 * @param {number} size - The average number of points in the clump. Correlates to the area of the circle.
 * @param {number} coherence - How much the radius of the clump varies (1 = circle, 0 = very random).
 * @param {number} smoothness - How smooth the border of the clump is (1 = few "peaks", 0 = very jagged).
 * @param {number} [failfraction] - Percentage of place attempts allowed to fail.
 * @param {Vector2D} [centerPosition] - Tile coordinates of placer center.
 */
function ClumpPlacer(size, coherence, smoothness, failFraction = 0, centerPosition = undefined)
{
	this.size = size;
	this.coherence = coherence;
	this.smoothness = smoothness;
	this.failFraction = failFraction;
	this.centerPosition = undefined;

	if (centerPosition)
		this.setCenterPosition(centerPosition);
}

ClumpPlacer.prototype.setCenterPosition = function(position)
{
	this.centerPosition = deepfreeze(position.clone().round());
};

ClumpPlacer.prototype.place = function(constraint)
{
	// Preliminary bounds check
	if (!g_Map.inMapBounds(this.centerPosition) || !constraint.allows(this.centerPosition))
		return undefined;

	const points = [];

	const size = g_Map.getSize();
	const gotRet = new Array(size).fill(0).map(p => new Uint8Array(size)); // booleans
	const radius = Math.sqrt(this.size / Math.PI);
	const perim = 4 * radius * 2 * Math.PI;
	const intPerim = Math.ceil(perim);

	let ctrlPts = 1 + Math.floor(1.0/Math.max(this.smoothness, 1.0/intPerim));

	if (ctrlPts > radius * 2 * Math.PI)
		ctrlPts = Math.floor(radius * 2 * Math.PI) + 1;

	const noise = new Float32Array(intPerim);		// float32
	const ctrlCoords = new Float32Array(ctrlPts+1);	// float32
	const ctrlVals = new Float32Array(ctrlPts+1);	// float32

	// Generate some interpolated noise
	for (let i=0; i < ctrlPts; i++)
	{
		ctrlCoords[i] = i * perim / ctrlPts;
		ctrlVals[i] = randFloat(0, 2);
	}

	let c = 0;
	let looped = 0;
	for (let i = 0; i < intPerim; ++i)
	{
		if (ctrlCoords[(c+1) % ctrlPts] < i && !looped)
		{
			c = (c+1) % ctrlPts;
			if (c == ctrlPts-1)
				looped = 1;
		}

		noise[i] = cubicInterpolation(
			1,
			(i - ctrlCoords[c]) / ((looped ? perim : ctrlCoords[(c + 1) % ctrlPts]) - ctrlCoords[c]),
			ctrlVals[(c + ctrlPts - 1) % ctrlPts],
			ctrlVals[c],
			ctrlVals[(c + 1) % ctrlPts],
			ctrlVals[(c + 2) % ctrlPts]);
	}

	let failed = 0;
	let count = 0;
	for (let stepAngle = 0; stepAngle < intPerim; ++stepAngle)
	{
		const position = this.centerPosition.clone();
		const radiusUnitVector = new Vector2D(0, 1).rotate(-2 * Math.PI * stepAngle / perim);
		const maxRadiusSteps = Math.ceil(radius * (1 + (1 - this.coherence) * noise[stepAngle]));

		count += maxRadiusSteps;
		for (let stepRadius = 0; stepRadius < maxRadiusSteps; ++stepRadius)
		{
			const tilePos = position.clone().floor();

			if (g_Map.inMapBounds(tilePos) && constraint.allows(tilePos))
			{
				if (!gotRet[tilePos.x][tilePos.y])
				{
					gotRet[tilePos.x][tilePos.y] = 1;
					points.push(tilePos);
				}
			}
			else
				++failed;

			position.add(radiusUnitVector);
		}
	}

	return failed > count * this.failFraction ? undefined : points;
};
