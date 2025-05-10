/**
 * Base class for all controls on the developer overlay.
 */
class DeveloperOverlayControl
{
	constructor(handler, i)
	{
		this.handler = handler;
		this.handler.update = () => this.update();

		this.body = Engine.GetGUIObjectByName("dev_command[" + i + "]");
		this.resize(i);

		this.updater = this.update.bind(this);

		if (this.handler.checked)
			registerPlayersInitHandler(this.updater);
	}

	getHeight()
	{
		return this.body.size.bottom - this.body.size.top;
	}

	resize(i)
	{
		const size = this.body.size;
		const height = size.bottom;
		size.top = height * i;
		size.bottom = height * (i + 1);
		this.body.size = size;
		this.body.hidden = false;
	}
}

