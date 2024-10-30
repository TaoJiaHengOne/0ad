function init()
{
	if (Engine.ConfigDB_GetValue("user", "lobby.login"))
		loginButton();

	return new Promise(closePageCallback => {
		Engine.GetGUIObjectByName("cancel").onPress = closePageCallback;
	});
}

function loginButton()
{
	Engine.OpenChildPage("page_prelobby_login.xml");
}

function registerButton()
{
	Engine.OpenChildPage("page_prelobby_register.xml");
}
