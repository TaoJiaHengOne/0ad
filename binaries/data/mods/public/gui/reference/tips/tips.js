var g_TipsPage;

function init(initData, hotloadData)
{
	g_TipsPage = new TipsPage(initData, hotloadData);
}

function getHotloadData()
{
	return g_TipsPage?.tipDisplay.getHotloadData();
}
