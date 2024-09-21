/* Copyright (C) 2024 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "GUIManager.h"

#include "gui/CGUI.h"
#include "lib/timer.h"
#include "lobby/IXmppClient.h"
#include "ps/CLogger.h"
#include "ps/Filesystem.h"
#include "ps/GameSetup/Config.h"
#include "ps/Profile.h"
#include "ps/VideoMode.h"
#include "ps/XML/Xeromyces.h"
#include "scriptinterface/FunctionWrapper.h"
#include "scriptinterface/Object.h"
#include "scriptinterface/Promises.h"
#include "scriptinterface/ScriptContext.h"
#include "scriptinterface/ScriptInterface.h"
#include "scriptinterface/StructuredClone.h"

namespace
{

const CStr EVENT_NAME_GAME_LOAD_PROGRESS = "GameLoadProgress";
const CStr EVENT_NAME_WINDOW_RESIZED = "WindowResized";

} // anonymous namespace

CGUIManager* g_GUI = nullptr;

// General TODOs:
//
// A lot of the CGUI data could (and should) be shared between
// multiple pages, instead of treating them as completely independent, to save
// memory and loading time.


// called from main loop when (input) events are received.
// event is passed to other handlers if false is returned.
// trampoline: we don't want to make the HandleEvent implementation static
InReaction gui_handler(const SDL_Event_* ev)
{
	if (!g_GUI)
		return IN_PASS;

	PROFILE("GUI event handler");
	return g_GUI->HandleEvent(ev);
}

static Status ReloadChangedFileCB(void* param, const VfsPath& path)
{
	return static_cast<CGUIManager*>(param)->ReloadChangedFile(path);
}

CGUIManager::CGUIManager(ScriptContext& scriptContext, ScriptInterface& scriptInterface) :
	m_ScriptContext{scriptContext},
	m_ScriptInterface{scriptInterface}
{
	m_ScriptInterface.SetCallbackData(this);
	m_ScriptInterface.LoadGlobalScripts();

	if (!g_Xeromyces.AddValidator(g_VFS, "gui_page", "gui/gui_page.rng"))
		LOGERROR("CGUIManager: failed to load GUI page grammar file 'gui/gui_page.rng'");
	if (!g_Xeromyces.AddValidator(g_VFS, "gui", "gui/gui.rng"))
		LOGERROR("CGUIManager: failed to load GUI XML grammar file 'gui/gui.rng'");

	RegisterFileReloadFunc(ReloadChangedFileCB, this);
}

CGUIManager::~CGUIManager()
{
	UnregisterFileReloadFunc(ReloadChangedFileCB, this);
}

size_t CGUIManager::GetPageCount() const
{
	return m_PageStack.size();
}

void CGUIManager::SwitchPage(const CStrW& pageName, const ScriptInterface* srcScriptInterface, JS::HandleValue initData)
{
	// The page stack is cleared (including the script context where initData came from),
	// therefore we have to clone initData.

	Script::StructuredClone initDataClone;
	if (!initData.isUndefined())
	{
		ScriptRequest rq(srcScriptInterface);
		initDataClone = Script::WriteStructuredClone(rq, initData);
	}

	if (!m_PageStack.empty())
	{
		// Make sure we unfocus anything on the current page.
		m_PageStack.back().gui->SendFocusMessage(GUIM_LOST_FOCUS);
		m_PageStack.clear();
	}

	PushPage(pageName, initDataClone);
}

JS::Value CGUIManager::PushPage(const CStrW& pageName, Script::StructuredClone initData)
{
	// Store the callback handler in the current GUI page before opening the new one
	JS::RootedValue promise{m_ScriptInterface.GetGeneralJSContext(), [&]
		{
			if (m_PageStack.empty())
				return JS::UndefinedValue();

			CGUI& currentPage = *m_PageStack.back().gui;
			// Make sure we unfocus anything on the current page.
			currentPage.SendFocusMessage(GUIM_LOST_FOCUS);
			return m_PageStack.back().ReplacePromise(*currentPage.GetScriptInterface());
		}()};

	// Push the page prior to loading its contents, because that may push
	// another GUI page on init which should be pushed on top of this new page.
	m_PageStack.emplace_back(pageName, initData);
	m_PageStack.back().LoadPage(m_ScriptContext);

	return promise;
}

void CGUIManager::PopPage(JS::HandleValue arg)
{
	SGUIPage& topmostPage{m_PageStack.back()};
	const ScriptRequest rq{topmostPage.gui->GetScriptInterface()};
	JS::ResolvePromise(rq.cx, *topmostPage.sendingPromise, arg);
}

CGUIManager::SGUIPage::SGUIPage(const CStrW& pageName, const Script::StructuredClone initData)
	: m_Name(pageName), initData(initData)
{
}

void CGUIManager::SGUIPage::LoadPage(ScriptContext& scriptContext)
{
	// If we're hotloading then try to grab some data from the previous page
	Script::StructuredClone hotloadData;
	if (gui)
	{
		std::shared_ptr<ScriptInterface> scriptInterface = gui->GetScriptInterface();
		ScriptRequest rq(scriptInterface);

		JS::RootedValue global(rq.cx, rq.globalValue());
		JS::RootedValue hotloadDataVal(rq.cx);
		ScriptFunction::Call(rq, global, "getHotloadData", &hotloadDataVal);
		hotloadData = Script::WriteStructuredClone(rq, hotloadDataVal);
	}

	g_VideoMode.ResetCursor();
	inputs.clear();
	gui.reset(new CGUI(scriptContext));
	const ScriptRequest rq{gui->GetScriptInterface()};
	sendingPromise = std::make_shared<JS::PersistentRootedObject>(rq.cx,
		JS::NewPromiseObject(rq.cx, nullptr));

	gui->AddObjectTypes();

	VfsPath path = VfsPath("gui") / m_Name;
	inputs.insert(path);

	CXeromyces xero;
	if (xero.Load(g_VFS, path, "gui_page") != PSRETURN_OK)
		// Fail silently (Xeromyces reported the error)
		return;

	int elmt_page = xero.GetElementID("page");
	int elmt_include = xero.GetElementID("include");

	XMBElement root = xero.GetRoot();

	if (root.GetNodeName() != elmt_page)
	{
		LOGERROR("GUI page '%s' must have root element <page>", utf8_from_wstring(m_Name));
		return;
	}

	XERO_ITER_EL(root, node)
	{
		if (node.GetNodeName() != elmt_include)
		{
			LOGERROR("GUI page '%s' must only have <include> elements inside <page>", utf8_from_wstring(m_Name));
			continue;
		}

		CStr8 name = node.GetText();
		CStrW nameW = node.GetText().FromUTF8();

		PROFILE2("load gui xml");
		PROFILE2_ATTR("name: %s", name.c_str());

		TIMER(nameW.c_str());
		if (name.back() == '/')
		{
			VfsPath currentDirectory = VfsPath("gui") / nameW;
			VfsPaths directories;
			vfs::GetPathnames(g_VFS, currentDirectory, L"*.xml", directories);
			for (const VfsPath& directory : directories)
				gui->LoadXmlFile(directory, inputs);
		}
		else
		{
			VfsPath directory = VfsPath("gui") / nameW;
			gui->LoadXmlFile(directory, inputs);
		}
	}

	gui->LoadedXmlFiles();

	JS::RootedValue initDataVal(rq.cx);
	JS::RootedValue hotloadDataVal(rq.cx);
	JS::RootedValue global(rq.cx, rq.globalValue());

	if (initData)
		Script::ReadStructuredClone(rq, initData, &initDataVal);

	if (hotloadData)
		Script::ReadStructuredClone(rq, hotloadData, &hotloadDataVal);

	if (!Script::HasProperty(rq, global, "init"))
		return;

	JS::RootedValue returnValue{rq.cx};
	if (!ScriptFunction::Call(rq, global, "init", &returnValue, initDataVal, hotloadDataVal))
	{
		LOGERROR("GUI page '%s': Failed to call init() function", utf8_from_wstring(m_Name));
		return;
	}

	if (!returnValue.isObject())
		return;

	JS::RootedObject returnObject{rq.cx, &returnValue.toObject()};
	if (!JS::IsPromiseObject(returnObject))
		return;

	sendingPromise = std::make_shared<JS::PersistentRootedObject>(rq.cx, returnObject);
}

JS::Value CGUIManager::SGUIPage::ReplacePromise(ScriptInterface& scriptInterface)
{
	const ScriptRequest rq{scriptInterface};
	receivingPromise = std::make_shared<JS::PersistentRootedObject>(rq.cx,
			JS::NewPromiseObject(rq.cx, nullptr));

	return JS::ObjectValue(**receivingPromise);
}

std::optional<CGUIManager::SGUIPage::CloseResult> CGUIManager::SGUIPage::MaybeClose()
{
	if (JS::GetPromiseState(*sendingPromise) == JS::PromiseState::Pending)
		return std::nullopt;

	// Make sure we unfocus anything on the current page.
	gui->SendFocusMessage(GUIM_LOST_FOCUS);

	const ScriptRequest rq{gui->GetScriptInterface()};
	JS::RootedValue arg{rq.cx, JS::GetPromiseResult(*sendingPromise)};
	return CGUIManager::SGUIPage::CloseResult{Script::WriteStructuredClone(rq, arg),
		JS::GetPromiseState(*sendingPromise) == JS::PromiseState::Rejected};
}

void CGUIManager::SGUIPage::Refocus(const CloseResult& result)
{
	ENSURE(receivingPromise);

	std::shared_ptr<ScriptInterface> scriptInterface = gui->GetScriptInterface();
	ScriptRequest rq(scriptInterface);

	JS::RootedObject globalObj(rq.cx, rq.glob);

	JS::RootedObject recv(rq.cx, *std::exchange(receivingPromise, nullptr));

	JS::RootedValue argVal(rq.cx);
	Script::ReadStructuredClone(rq, result.arg, &argVal);

	// This only resolves the promise, it doesn't call the continuation.
	(result.rejected ? JS::RejectPromise : JS::ResolvePromise)(rq.cx, recv, argVal);

	// We return to a page where some object might have been focused.
	gui->SendFocusMessage(GUIM_GOT_FOCUS);
}

Status CGUIManager::ReloadChangedFile(const VfsPath& path)
{
	for (SGUIPage& p : m_PageStack)
		if (p.inputs.find(path) != p.inputs.end())
		{
			LOGMESSAGE("GUI file '%s' changed - reloading page '%s'", path.string8(), utf8_from_wstring(p.m_Name));
			p.LoadPage(m_ScriptContext);
			// TODO: this can crash if LoadPage runs an init script which modifies the page stack and breaks our iterators
		}

	return INFO::OK;
}

Status CGUIManager::ReloadAllPages()
{
	// TODO: this can crash if LoadPage runs an init script which modifies the page stack and breaks our iterators
	for (SGUIPage& p : m_PageStack)
		p.LoadPage(m_ScriptContext);

	return INFO::OK;
}

InReaction CGUIManager::HandleEvent(const SDL_Event_* ev)
{
	// We want scripts to have access to the raw input events, so they can do complex
	// processing when necessary (e.g. for unit selection and camera movement).
	// Sometimes they'll want to be layered behind the GUI widgets (e.g. to detect mousedowns on the
	// visible game area), sometimes they'll want to intercepts events before the GUI (e.g.
	// to capture all mouse events until a mouseup after dragging).
	// So we call two separate handler functions:

	bool handled = false;

	{
		PROFILE("handleInputBeforeGui");
		ScriptRequest rq(*top()->GetScriptInterface());

		JS::RootedValue global(rq.cx, rq.globalValue());
		if (ScriptFunction::Call(rq, global, "handleInputBeforeGui", handled, *ev, top()->FindObjectUnderMouse()))
			if (handled)
				return IN_HANDLED;
	}

	{
		PROFILE("handle event in native GUI");
		InReaction r = top()->HandleEvent(ev);
		if (r != IN_PASS)
			return r;
	}

	{
		// We can't take the following lines out of this scope because top() may be another gui page than it was when calling handleInputBeforeGui!
		ScriptRequest rq(*top()->GetScriptInterface());
		JS::RootedValue global(rq.cx, rq.globalValue());

		PROFILE("handleInputAfterGui");
		if (ScriptFunction::Call(rq, global, "handleInputAfterGui", handled, *ev))
			if (handled)
				return IN_HANDLED;
	}

	return IN_PASS;
}

void CGUIManager::SendEventToAll(const CStr& eventName) const
{
	// Save an immutable copy so iterators aren't invalidated by handlers
	PageStackType pageStack = m_PageStack;

	for (const SGUIPage& p : pageStack)
		p.gui->SendEventToAll(eventName);

}

void CGUIManager::SendEventToAll(const CStr& eventName, JS::HandleValueArray paramData) const
{
	// Save an immutable copy so iterators aren't invalidated by handlers
	PageStackType pageStack = m_PageStack;

	for (const SGUIPage& p : pageStack)
		p.gui->SendEventToAll(eventName, paramData);
}

void CGUIManager::TickObjects()
{
	PROFILE3("gui tick");

	// We share the script context with everything else that runs in the same thread.
	// This call makes sure we trigger GC regularly even if the simulation is not running.
	m_ScriptContext.MaybeIncrementalGC(1.0f);

	// Save an immutable copy so iterators aren't invalidated by tick handlers
	PageStackType pageStack = m_PageStack;

	for (const SGUIPage& p : pageStack)
		p.gui->TickObjects();

	m_ScriptContext.RunJobs();

	while (!m_PageStack.empty())
	{
		const size_t stackSize{m_PageStack.size()};
		const std::optional<SGUIPage::CloseResult> result{m_PageStack.back().MaybeClose()};
		if (!result.has_value())
			break;
		ENSURE(m_PageStack.size() == stackSize);
		m_PageStack.pop_back();
		if (!m_PageStack.empty())
			m_PageStack.back().Refocus(result.value());

		m_ScriptContext.RunJobs();
	}
}

void CGUIManager::Draw(CCanvas2D& canvas) const
{
	PROFILE3_GPU("gui");

	for (const SGUIPage& p : m_PageStack)
		p.gui->Draw(canvas);
}

void CGUIManager::UpdateResolution()
{
	// Save an immutable copy so iterators aren't invalidated by event handlers
	PageStackType pageStack = m_PageStack;

	for (const SGUIPage& p : pageStack)
	{
		p.gui->UpdateResolution();
		p.gui->SendEventToAll(EVENT_NAME_WINDOW_RESIZED);
	}
}

bool CGUIManager::TemplateExists(const std::string& templateName) const
{
	return m_TemplateLoader.TemplateExists(templateName);
}

const CParamNode& CGUIManager::GetTemplate(const std::string& templateName)
{
	const CParamNode& templateRoot = m_TemplateLoader.GetTemplateFileData(templateName).GetOnlyChild();
	if (!templateRoot.IsOk())
		LOGERROR("Invalid template found for '%s'", templateName.c_str());

	return templateRoot;
}

void CGUIManager::DisplayLoadProgress(int percent, const wchar_t* pending_task)
{
	const ScriptInterface& scriptInterface = *(GetActiveGUI()->GetScriptInterface());
	ScriptRequest rq(scriptInterface);

	JS::RootedValueVector paramData(rq.cx);

	ignore_result(paramData.append(JS::NumberValue(percent)));

	JS::RootedValue valPendingTask(rq.cx);
	Script::ToJSVal(rq, &valPendingTask, pending_task);
	ignore_result(paramData.append(valPendingTask));

	SendEventToAll(EVENT_NAME_GAME_LOAD_PROGRESS, paramData);
}

// This returns a shared_ptr to make sure the CGUI doesn't get deallocated
// while we're in the middle of calling a function on it (e.g. if a GUI script
// calls SwitchPage)
std::shared_ptr<CGUI> CGUIManager::top() const
{
	ENSURE(m_PageStack.size());
	return m_PageStack.back().gui;
}
