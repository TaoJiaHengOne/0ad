var g_ModsAvailableOnline = [];

/**
 * Indicates if we have encountered an error in one of the network-interaction attempts.
 *
 * We use a global so we don't get multiple messageBoxes appearing (one for each "tick").
 *
 * Set to `true` by showErrorMessageBox
 * Set to `false` by init, updateModList, downloadFile, and cancelRequest
 */
var g_Failure;

/**
 * Indicates if the user has cancelled a request.
 *
 * Primarily used so the user can cancel the mod list fetch, as whenever that get cancelled,
 * the modio state reverts to "ready", even if we've successfully listed mods before.
 *
 * Set to `true` by cancelRequest
 * Set to `false` by updateModList, and downloadFile
 */
var g_RequestCancelled;

var g_RequestStartTime;

var g_ModIOState = {
	/**
	 * Finished status indicators
	 */
	"ready": (progressData, closePageCallback) => {
		// GameID acquired, ready to fetch mod list
		if (!g_RequestCancelled)
			updateModList(closePageCallback);
	},
	"listed": progressData => {
		// List of available mods acquired

		// Only run this once (for each update).
		if (Engine.GetGUIObjectByName("modsAvailableList").list.length)
			return;

		hideDialog();
		Engine.GetGUIObjectByName("refreshButton").enabled = true;
		g_ModsAvailableOnline = Engine.ModIoGetMods();
		displayMods();
	},
	"success": progressData => {
		// Successfully acquired a mod file
		hideDialog();
		Engine.GetGUIObjectByName("downloadButton").enabled = true;
	},
	/**
	 * In-progress status indicators.
	 */
	"gameid": progressData => {
		// Acquiring GameID from mod.io
	},
	"listing": progressData => {
		// Acquiring list of available mods from mod.io
	},
	"downloading": progressData => {
		// Downloading a mod file
		updateProgressBar(progressData.progress, g_ModsAvailableOnline[selectedModIndex()].filesize);
	},
	/**
	 * Error/Failure status indicators.
	 */
	"failed_gameid": async(progressData, closePageCallback) => {
		// Game ID couldn't be retrieved
		const promise = showErrorMessageBox(
			sprintf(translateWithContext("mod.io error message", "Game ID could not be retrieved.\n\n%(technicalDetails)s"), {
				"technicalDetails": progressData.error
			}),
			translateWithContext("mod.io error message", "Initialization Error"),
			[translate("Abort"), translate("Retry")]);
		if (!promise)
			return;
		if (await promise === 0)
			closePageCallback();
		else
			init();
	},
	"failed_listing": async(progressData, closePageCallback) => {
		// Mod list couldn't be retrieved
		const promise = showErrorMessageBox(
			sprintf(translateWithContext("mod.io error message", "Mod List could not be retrieved.\n\n%(technicalDetails)s"), {
				"technicalDetails": progressData.error
			}),
			translateWithContext("mod.io error message", "Fetch Error"),
			[translate("Abort"), translate("Retry")]);
		if (!promise)
			return;
		if (await promise === 0)
			cancelModListUpdate(closePageCallback);
		else
			updateModList(closePageCallback);
	},
	"failed_downloading": async(progressData) => {
		// File couldn't be retrieved
		const promise = showErrorMessageBox(
			sprintf(translateWithContext("mod.io error message", "File download failed.\n\n%(technicalDetails)s"), {
				"technicalDetails": progressData.error
			}),
			translateWithContext("mod.io error message", "Download Error"),
			[translate("Abort"), translate("Retry")]);
		if (!promise)
			return;
		if (await promise === 0)
			cancelRequest();
		else
			downloadMod();
	},
	"failed_filecheck": async(progressData) => {
		// The file is corrupted
		const promise = showErrorMessageBox(
			sprintf(translateWithContext("mod.io error message", "File verification error.\n\n%(technicalDetails)s"), {
				"technicalDetails": progressData.error
			}),
			translateWithContext("mod.io error message", "Verification Error"),
			[translate("Abort")]);
		if (!promise)
			return;
		await promise;
		cancelRequest();
	},
	/**
	 * Default
	 */
	"none": progressData => {
		// Nothing has happened yet.
	}
};

function init(data)
{
	const promise = progressDialog(
		translate("Initializing mod.io interface."),
		translate("Initializing"),
		false,
		translate("Cancel"));

	g_Failure = false;
	Engine.ModIoStartGetGameId();

	return Promise.race([
		promise,
		new Promise(closePageCallback => {
			Engine.GetGUIObjectByName("backButton").onPress = closePageCallback;
			Engine.GetGUIObjectByName("modio").onTick = onTick.bind(null, closePageCallback);
		})
	]);
}

function onTick(closePageCallback)
{
	const progressData = Engine.ModIoGetDownloadProgress();

	const handler = g_ModIOState[progressData.status];
	if (!handler)
	{
		warn("Unrecognized progress status: " + progressData.status);
		return;
	}

	handler(progressData, closePageCallback);
	if (!progressData.status.startsWith("failed"))
		Engine.ModIoAdvanceRequest();
}

function displayMods()
{
	const modsAvailableList = Engine.GetGUIObjectByName("modsAvailableList");
	const selectedMod = modsAvailableList.list[modsAvailableList.selected];
	modsAvailableList.selected = -1;

	let displayedMods = clone(g_ModsAvailableOnline);
	for (let i = 0; i < displayedMods.length; ++i)
		displayedMods[i].i = i;

	const filterColumns = ["name", "name_id", "summary"];
	const filterText = Engine.GetGUIObjectByName("modFilter").caption.toLowerCase();
	if (Engine.GetGUIObjectByName("compatibilityFilter").checked)
		displayedMods = displayedMods.filter(mod => !mod.invalid);
	displayedMods = displayedMods.filter(mod => filterColumns.some(column => mod[column].toLowerCase().indexOf(filterText) != -1));

	displayedMods.sort((mod1, mod2) =>
		modsAvailableList.selected_column_order *
		(modsAvailableList.selected_column == "filesize" ?
			mod1.filesize - mod2.filesize :
			String(mod1[modsAvailableList.selected_column]).localeCompare(String(mod2[modsAvailableList.selected_column]))));

	modsAvailableList.list_name = displayedMods.map(mod => compatibilityColor(mod.name, !mod.invalid));
	modsAvailableList.list_name_id = displayedMods.map(mod => compatibilityColor(mod.name_id, !mod.invalid));
	modsAvailableList.list_version = displayedMods.map(mod => compatibilityColor(mod.version || "", !mod.invalid));
	modsAvailableList.list_filesize = displayedMods.map(mod => compatibilityColor(mod.filesize !== undefined ? filesizeToString(mod.filesize) : filesizeToString(mod.filesize), !mod.invalid));
	modsAvailableList.list_dependencies = displayedMods.map(mod => compatibilityColor((mod.dependencies || []).join(" "), !mod.invalid));
	modsAvailableList.list = displayedMods.map(mod => mod.i);
	modsAvailableList.selected = modsAvailableList.list.indexOf(selectedMod);
}

function clearModList()
{
	const modsAvailableList = Engine.GetGUIObjectByName("modsAvailableList");
	modsAvailableList.selected = -1;
	modsAvailableList.list_name = [];
	modsAvailableList.list_name_id = [];
	modsAvailableList.list_version = [];
	modsAvailableList.list_filesize = [];
	modsAvailableList.list_dependencies = [];
	modsAvailableList.list = [];
}

function selectedModIndex()
{
	const modsAvailableList = Engine.GetGUIObjectByName("modsAvailableList");

	if (modsAvailableList.selected == -1)
		return undefined;

	return +modsAvailableList.list[modsAvailableList.selected];
}

function isSelectedModInvalid(selected)
{
	return selected !== undefined && !!g_ModsAvailableOnline[selected].invalid && g_ModsAvailableOnline[selected].invalid == "true";
}

function showModDescription()
{
	const selected = selectedModIndex();
	const isSelected = selected !== undefined;
	const isInvalid = isSelectedModInvalid(selected);
	Engine.GetGUIObjectByName("downloadButton").enabled = isSelected && !isInvalid;
	Engine.GetGUIObjectByName("modDescription").caption = isSelected && !isInvalid ? g_ModsAvailableOnline[selected].summary : "";
	Engine.GetGUIObjectByName("modError").caption = isSelected && isInvalid ? sprintf(translate("Invalid mod: %(error)s"), { "error": g_ModsAvailableOnline[selected].error }) : "";
}

function cancelModListUpdate(closePageCallback)
{
	cancelRequest();

	if (!g_ModsAvailableOnline.length)
	{
		closePageCallback();
		return;
	}

	displayMods();
	Engine.GetGUIObjectByName('refreshButton').enabled = true;
}

async function updateModList(closePageCallback)
{
	clearModList();
	Engine.GetGUIObjectByName("refreshButton").enabled = false;

	const promise = progressDialog(
		translate("Fetching and updating list of available mods."),
		translate("Updating"),
		false,
		translate("Cancel Update"));

	g_Failure = false;
	g_RequestCancelled = false;
	Engine.ModIoStartListMods();

	await promise;
	cancelModListUpdate(closePageCallback);
}

async function downloadMod()
{
	const selected = selectedModIndex();

	if (isSelectedModInvalid(selected))
		return;

	const promise = progressDialog(
		sprintf(translate("Downloading “%(modname)s”"), {
			"modname": g_ModsAvailableOnline[selected].name
		}),
		translate("Downloading"),
		true,
		translate("Cancel Download"));

	Engine.GetGUIObjectByName("downloadButton").enabled = false;

	g_Failure = false;
	g_RequestCancelled = false;
	Engine.ModIoStartDownloadMod(selected);

	await promise;
	Engine.GetGUIObjectByName("downloadButton").enabled = true;
}

function cancelRequest()
{
	g_Failure = false;
	g_RequestCancelled = true;
	Engine.ModIoCancelRequest();
	hideDialog();
}

function showErrorMessageBox(caption, title, buttonCaptions)
{
	if (g_Failure)
		return null;
	g_Failure = true;

	return messageBox(500, 250, caption, title, buttonCaptions);
}

async function progressDialog(dialogCaption, dialogTitle, showProgressBar, buttonCaption)
{
	Engine.GetGUIObjectByName("downloadDialog_title").caption = dialogTitle;

	const downloadDialogCaption = Engine.GetGUIObjectByName("downloadDialog_caption");
	downloadDialogCaption.caption = dialogCaption;
	downloadDialogCaption.size.rbottom = showProgressBar ? 40 : 80;

	Engine.GetGUIObjectByName("downloadDialog_progress").hidden = !showProgressBar;
	Engine.GetGUIObjectByName("downloadDialog_status").hidden = !showProgressBar;

	Engine.GetGUIObjectByName("downloadDialog").hidden = false;

	g_RequestStartTime = Date.now();

	const downloadDialog_button = Engine.GetGUIObjectByName("downloadDialog_button");
	downloadDialog_button.caption = buttonCaption;
	await new Promise(resolve => {
		downloadDialog_button.onPress = resolve;
	});
	cancelRequest();
}

/*
 * The "remaining time" and "average speed" texts both naively assume that
 * the connection remains relatively stable throughout the download.
 */
function updateProgressBar(progress, totalSize)
{
	const progressPercent = Math.ceil(progress * 100);
	Engine.GetGUIObjectByName("downloadDialog_progressBar").progress = progressPercent;

	const transferredSize = progress * totalSize;
	const transferredSizeObj = filesizeToObj(transferredSize);
	// Translation: Mod file download indicator. Current size over expected final size, with percentage complete.
	Engine.GetGUIObjectByName("downloadDialog_progressText").caption = sprintf(translate("%(current)s / %(total)s (%(percent)s%%)"), {
		"current": filesizeToObj(totalSize).unit == transferredSizeObj.unit ? transferredSizeObj.filesize : filesizeToString(transferredSize),
		"total": filesizeToString(totalSize),
		"percent": progressPercent
	});

	const elapsedTime = Date.now() - g_RequestStartTime;
	const remainingTime = progressPercent ? (100 - progressPercent) * elapsedTime / progressPercent : 0;
	const avgSpeed = filesizeToObj(transferredSize / (elapsedTime / 1000));
	// Translation: Mod file download status message.
	Engine.GetGUIObjectByName("downloadDialog_status").caption = sprintf(translate("Time Elapsed: %(elapsed)s\nEstimated Time Remaining: %(remaining)s\nAverage Speed: %(avgSpeed)s"), {
		"elapsed": timeToString(elapsedTime),
		"remaining": remainingTime ? timeToString(remainingTime) : translate("∞"),
		// Translation: Average download speed, used to give the user a very rough and naive idea of the download time. For example: 123.4 KiB/s
		"avgSpeed": sprintf(translate("%(number)s %(unit)s/s"), {
			"number": avgSpeed.filesize,
			"unit": avgSpeed.unit
		})
	});
}

function hideDialog()
{
	Engine.GetGUIObjectByName("downloadDialog").hidden = true;
}
