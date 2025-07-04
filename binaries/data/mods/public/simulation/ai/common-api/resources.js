Resources = new Resources();

API3 = function(m)
{

m.Resources = function(amounts = {}, population = 0)
{
	for (const key of Resources.GetCodes())
		this[key] = amounts[key] || 0;

	this.population = population > 0 ? population : 0;
};

m.Resources.prototype.reset = function()
{
	for (const key of Resources.GetCodes())
		this[key] = 0;
	this.population = 0;
};

m.Resources.prototype.canAfford = function(that)
{
	for (const key of Resources.GetCodes())
		if (this[key] < that[key])
			return false;
	return true;
};

m.Resources.prototype.add = function(that)
{
	for (const key of Resources.GetCodes())
		this[key] += that[key];
	this.population += that.population;
};

m.Resources.prototype.subtract = function(that)
{
	for (const key of Resources.GetCodes())
		this[key] -= that[key];
	this.population += that.population;
};

m.Resources.prototype.multiply = function(n)
{
	for (const key of Resources.GetCodes())
		this[key] *= n;
	this.population *= n;
};

m.Resources.prototype.Serialize = function()
{
	const amounts = {};
	for (const key of Resources.GetCodes())
		amounts[key] = this[key];
	return { "amounts": amounts, "population": this.population };
};

m.Resources.prototype.Deserialize = function(data)
{
	for (const key in data.amounts)
		this[key] = data.amounts[key];
	this.population = data.population;
};

return m;

}(API3);
