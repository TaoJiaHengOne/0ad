/*
IGUIObject
by Gustav Larsson
gee@pyro.nu
*/

#include "precompiled.h"
#include "GUI.h"

///// janwas: again, including etiquette?
#include "Parser.h"
#include <assert.h>
/////

#include "gui/scripting/JSInterface_IGUIObject.h"
#include "gui/scripting/JSInterface_GUITypes.h"

extern int g_xres, g_yres;

using namespace std;

//-------------------------------------------------------------------
//  Implementation Macros
//-------------------------------------------------------------------

//-------------------------------------------------------------------
//  Constructor / Destructor
//-------------------------------------------------------------------
IGUIObject::IGUIObject() : 
	m_pGUI(NULL), 
	m_pParent(NULL),
	m_MouseHovering(false)
{
	AddSetting(GUIST_bool,			"enabled");
	AddSetting(GUIST_bool,			"hidden");
	AddSetting(GUIST_CClientArea,	"size");
	AddSetting(GUIST_CStr,			"style");
	AddSetting(GUIST_CStr,			"hotkey" );
	AddSetting(GUIST_float,			"z");
	AddSetting(GUIST_bool,			"absolute");
	AddSetting(GUIST_bool,			"ghost");

	// Setup important defaults
	GUI<bool>::SetSetting(this, "hidden", false);
	GUI<bool>::SetSetting(this, "ghost", false);
	GUI<bool>::SetSetting(this, "enabled", true);
	GUI<bool>::SetSetting(this, "absolute", true);



	bool hidden=true;

	GUI<bool>::GetSetting(this, "hidden", hidden);
}

IGUIObject::~IGUIObject()
{
// delete() needs to know the type of the variable - never delete a void*
#define TYPE(t) case GUIST_##t: delete (t*)it->second.m_pSetting; break;
	map<CStr, SGUISetting>::iterator it;
	for (it = m_Settings.begin(); it != m_Settings.end(); ++it)
		switch (it->second.m_Type)
		{
		#include "GUItypes.h"
		default:
			debug_warn("Invalid setting type");
		}
#undef TYPE
}

//-------------------------------------------------------------------
//  Functions
//-------------------------------------------------------------------
void IGUIObject::AddChild(IGUIObject *pChild)
{
	// 
//	assert(pChild);

	pChild->SetParent(this);

	m_Children.push_back(pChild);

	// If this (not the child) object is already attached
	//  to a CGUI, it pGUI pointer will be non-null.
	//  This will mean we'll have to check if we're using
	//  names already used.
	if (pChild->GetGUI())
	{
		try
		{
			// Atomic function, if it fails it won't
			//  have changed anything
			//UpdateObjects();
			pChild->GetGUI()->UpdateObjects();
		}
		catch (PS_RESULT e)
		{
			// If anything went wrong, reverse what we did and throw
			//  an exception telling it never added a child
			m_Children.erase( m_Children.end()-1 );

			// We'll throw the same exception for easier
			//  error handling
			throw e;
		}
	}
	// else do nothing
}

void IGUIObject::AddToPointersMap(map_pObjects &ObjectMap)
{
	// Just don't do anything about the top node
	if (m_pParent == NULL)
		return;

	// Now actually add this one
	//  notice we won't add it if it's doesn't have any parent
	//  (i.e. being the base object)
	if (m_Name.Length() == 0)
	{
		throw PS_NEEDS_NAME;
	}
	if (ObjectMap.count(m_Name) > 0)
	{
		throw PS_NAME_AMBIGUITY;
	}
	else
	{
		ObjectMap[m_Name] = this;
	}
}

void IGUIObject::Destroy()
{
	// Is there anything besides the children to destroy?
}

// Notice if using this, the naming convention of GUIST_ should be strict.
#define TYPE(type)									\
	case GUIST_##type:								\
		m_Settings[Name].m_pSetting = new type();	\
		break;

void IGUIObject::AddSetting(const EGUISettingType &Type, const CStr& Name)
{
	// Is name already taken?
	if (m_Settings.count(Name) >= 1)
		return;

	// Construct, and set type
	m_Settings[Name].m_Type = Type;

	switch (Type)
	{
		// Construct the setting.
		#include "GUItypes.h"

	default:
		debug_warn("IGUIObject::AddSetting failed, type not recognized!");
		break;
	}
}
#undef TYPE


bool IGUIObject::MouseOver()
{
	if(!GetGUI())
		throw PS_NEEDS_PGUI;

	return m_CachedActualSize.PointInside(GetMousePos());
}

CPos IGUIObject::GetMousePos() const
{ 
	return ((GetGUI())?(GetGUI()->m_MousePos):CPos()); 
}

void IGUIObject::UpdateMouseOver(IGUIObject * const &pMouseOver)
{
	// Check if this is the object being hovered.
	if (pMouseOver == this)
	{
		if (!m_MouseHovering)
		{
			// It wasn't hovering, so that must mean it just entered
			HandleMessage(GUIM_MOUSE_ENTER);
			ScriptEvent("mouseenter");
		}

		// Either way, set to true
		m_MouseHovering = true;

		// call mouse over
		HandleMessage(GUIM_MOUSE_OVER);
		ScriptEvent("mouseover");
	}
	else // Some other object (or none) is hovered
	{
		if (m_MouseHovering)
		{
			m_MouseHovering = false;
			HandleMessage(GUIM_MOUSE_LEAVE);
			ScriptEvent("mouseleave");
		}
	}
}

bool IGUIObject::SettingExists(const CStr& Setting) const
{
	// Because GetOffsets will direct dynamically defined
	//  classes with polymorphism to respective ms_SettingsInfo
	//  we need to make no further updates on this function
	//  in derived classes.
	//return (GetSettingsInfo().count(Setting) >= 1);
	return (m_Settings.count(Setting) >= 1);
}

#define TYPE(type)										\
	else												\
	if (set.m_Type == GUIST_##type)						\
	{													\
		type _Value;									\
		if (!GUI<type>::ParseString(Value, _Value))		\
			return PS_FAIL;								\
														\
		GUI<type>::SetSetting(this, Setting, _Value, SkipMessage);	\
	}

PS_RESULT IGUIObject::SetSetting(const CStr& Setting, const CStr& Value, const bool& SkipMessage)
{
	if (!SettingExists(Setting))
	{
		return PS_FAIL;
	}

	// Get setting
	SGUISetting set = m_Settings[Setting];

	if (0);
	// else...
#include "GUItypes.h"
	else
	{
		return PS_FAIL;
	}
	return PS_OK;
}

#undef TYPE


PS_RESULT IGUIObject::GetSettingType(const CStr& Setting, EGUISettingType &Type) const
{
	if (!SettingExists(Setting))
		return PS_SETTING_FAIL;

	if (m_Settings.find(Setting) == m_Settings.end())
		return PS_FAIL;

	Type = m_Settings.find(Setting)->second.m_Type;

	return PS_OK;
}


void IGUIObject::ChooseMouseOverAndClosest(IGUIObject* &pObject)
{
	if (MouseOver())
	{
		// Check if we've got competition at all
		if (pObject == NULL)
		{
			pObject = this;
			return;
		}

		// Or if it's closer
		if (GetBufferedZ() >= pObject->GetBufferedZ())
		{
			pObject = this;
			return;
		}
	}
}

IGUIObject *IGUIObject::GetParent() const
{
	// Important, we're not using GetParent() for these
	//  checks, that could screw it up
	if (m_pParent)
	{
		if (m_pParent->m_pParent == NULL)
			return NULL;
	}

	return m_pParent;
}

void IGUIObject::UpdateCachedSize()
{
	bool absolute;
	GUI<bool>::GetSetting(this, "absolute", absolute);

	CClientArea ca;
	GUI<CClientArea>::GetSetting(this, "size", ca);
	
	// If absolute="false" and the object has got a parent,
	//  use its cached size instead of the screen. Notice
	//  it must have just been cached for it to work.
	if (absolute == false && m_pParent)
		m_CachedActualSize = ca.GetClientArea(m_pParent->m_CachedActualSize);
	else
		m_CachedActualSize = ca.GetClientArea(CRect(0.f, 0.f, (float)g_xres, (float)g_yres));

}

void IGUIObject::LoadStyle(CGUI &GUIinstance, const CStr& StyleName)
{
	// Fetch style
	if (GUIinstance.m_Styles.count(StyleName)==1)
	{
		LoadStyle(GUIinstance.m_Styles[StyleName]);
	}
	else
	{
		debug_warn("IGUIObject::LoadStyle failed");
	}
}

void IGUIObject::LoadStyle(const SGUIStyle &Style)
{
	// Iterate settings, it won't be able to set them all probably, but that doesn't matter
	std::map<CStr, CStr>::const_iterator cit;
	for (cit = Style.m_SettingsDefaults.begin(); cit != Style.m_SettingsDefaults.end(); ++cit)
	{
		// Try set setting in object
		SetSetting(cit->first, cit->second);

		// It doesn't matter if it fail, it's not suppose to be able to set every setting.
		//  since it's generic.

		// The beauty with styles is that it can contain more settings
		//  than exists for the objects using it. So if the SetSetting
		//  fails, don't care.
	}
}

float IGUIObject::GetBufferedZ() const
{
	bool absolute;
	GUI<bool>::GetSetting(this, "absolute", absolute);

	float Z;
	GUI<float>::GetSetting(this, "z", Z);

	if (absolute)
		return Z;
	else
	{
		if (GetParent())
			return GetParent()->GetBufferedZ() + Z;
		else
		{
			debug_warn("IGUIObject::LoadStyle failed");
			// TODO Gee: Error, no object should be relative without a parent!
			return Z;
		}
	}
}

// TODO Gee: keep this function and all???
void IGUIObject::CheckSettingsValidity()
{
	bool hidden;
	GUI<bool>::GetSetting(this, "hidden", hidden);

	// If we hide an object, reset many of its parts
	if (hidden)
	{
		// Simulate that no object is hovered for this object and all its children
		//  why? because it's 
		try
		{
			GUI<IGUIObject*>::RecurseObject(0, this, &IGUIObject::UpdateMouseOver, NULL);
		}
		catch (...) {}
	}

	try
	{
		// Send message to itself
		HandleMessage(GUIM_SETTINGS_UPDATED);
		ScriptEvent("update");
	}
	catch (...)
	{
	}
}

void IGUIObject::RegisterScriptHandler(const CStr& Action, const CStr& Code, CGUI* pGUI)
{
	const int paramCount = 1;
	const char* paramNames[paramCount] = { "mouse" };

	// Location to report errors from
	CStr CodeName = GetName()+" "+Action;

	// Generate a unique name
	static int x=0;
	char buf[64];
	sprintf(buf, "__eventhandler%d", x++);

	JSFunction* func = JS_CompileFunction(g_ScriptingHost.getContext(), pGUI->m_ScriptObject, buf, paramCount, paramNames, (const char*)Code, Code.Length(), CodeName, 0);
	assert(func); // TODO: Handle errors

	m_ScriptHandlers[Action] = func;
}

void IGUIObject::ScriptEvent(const CStr& Action)
{
	map<CStr, JSFunction*>::iterator it = m_ScriptHandlers.find(Action);
	if (it == m_ScriptHandlers.end())
		return;

	// PRIVATE_TO_JSVAL assumes two-byte alignment,
	// so make sure that's always true
	assert(! ((jsval)this & JSVAL_INT));

	// The IGUIObject needs to be stored inside the script's object
	jsval guiObject = PRIVATE_TO_JSVAL(this);

	// Make a 'this', allowing access to the IGUIObject
	JSObject* jsGuiObject = JS_ConstructObjectWithArguments(g_ScriptingHost.getContext(), &JSI_IGUIObject::JSI_class, NULL, m_pGUI->m_ScriptObject, 1, &guiObject);
	assert(jsGuiObject); // TODO: Handle errors
	
	// Prevent it from being garbage-collected before it's passed into the function
	JS_AddRoot(g_ScriptingHost.getContext(), &jsGuiObject);

	// Set up the 'mouse' parameter
	jsval mouseParams[3];
	mouseParams[0] = INT_TO_JSVAL(m_pGUI->m_MousePos.x);
	mouseParams[1] = INT_TO_JSVAL(m_pGUI->m_MousePos.y);
	mouseParams[2] = INT_TO_JSVAL(m_pGUI->m_MouseButtons);
	JSObject* mouseObj = JS_ConstructObjectWithArguments(g_ScriptingHost.getContext(), &JSI_GUIMouse::JSI_class, NULL, m_pGUI->m_ScriptObject, 3, mouseParams);
	assert(mouseObj); // TODO: Handle errors

	// Don't garbage collect the mouse
	JS_AddRoot(g_ScriptingHost.getContext(), &mouseObj);

	const int paramCount = 1;
	jsval paramData[paramCount];
	paramData[0] = OBJECT_TO_JSVAL(mouseObj);

	jsval result;
	JSBool ok = JS_CallFunction(g_ScriptingHost.getContext(), jsGuiObject, it->second, 1, paramData, &result);
	if (!ok)
	{
		JS_ReportError(g_ScriptingHost.getContext(), "Errors executing script action \"%s\"", Action.c_str());
	}

	// Allow the temporary parameters to be garbage-collected
	JS_RemoveRoot(g_ScriptingHost.getContext(), &mouseObj);
	JS_RemoveRoot(g_ScriptingHost.getContext(), &jsGuiObject);
}

CStr IGUIObject::GetPresentableName() const
{
	// __internal(), must be at least 13 letters to be able to be
	//  an internal name
	if (m_Name.Length() <= 12)
		return m_Name;

	if (m_Name.GetSubstring(0, 10) == "__internal")
		return CStr("[unnamed object]");
	else
		return m_Name;
}

void IGUIObject::SetFocus()
{
	GetGUI()->m_FocusedObject = this; 
}

bool IGUIObject::IsFocused() const
{
	return GetGUI()->m_FocusedObject == this; 
}
