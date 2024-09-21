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
	Engine.PushGuiPage("page_prelobby_login.xml");
}

function registerButton()
{
	Engine.PushGuiPage("page_prelobby_register.xml");
}
