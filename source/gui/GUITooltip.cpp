#include "precompiled.h"

#include "GUITooltip.h"
#include "lib/timer.h"
#include "IGUIObject.h"
#include "CGUI.h"

#include "ps/CLogger.h"

/*
	Tooltips:
	When holding the mouse stationary over an object for some amount of time,
	the tooltip is displayed. If the mouse moves off that object, the tooltip
	disappears. If the mouse re-enters an object within a short time, the new
	tooltip is displayed immediately. (This lets you run the mouse across a
	series of buttons, without waiting ages for the text to pop up every time.)

	See Visual Studio's toolbar buttons for an example.


	Implemented as a state machine:

	(where "*" lines are checked constantly, and "<" lines are handled
	on entry to that state)

	IN MOTION
	* If the mouse stops, check whether it should have a tooltip and move to
	  'STATIONARY, NO TOOLTIP' or 'STATIONARY, TOOLIP'
	* If the mouse enters an object with a tooltip delay of 0, switch to 'SHOWING'

	STATIONARY, NO TOOLTIP
	* If the mouse moves, switch to 'IN MOTION'

	STATIONARY, TOOLTIP
	< Set target time = now + tooltip time
	* If the mouse moves, switch to 'IN MOTION'
	* If now > target time, switch to 'SHOWING'

	SHOWING
	< Start displaying the tooltip
	* If the mouse leaves the object, check whether the new object has a tooltip
	  and switch to 'SHOWING' or 'COOLING'

	COOLING  (since I can't think of a better name)
	< Stop displaying the tooltip
	< Set target time = now + cooldown time
	* If the mouse has moved and is over a tooltipped object, switch to 'SHOWING'
	* If now > target time, switch to 'STATIONARY, NO TOOLTIP'
*/

enum
{
	ST_IN_MOTION,
	ST_STATIONARY_NO_TOOLTIP,
	ST_STATIONARY_TOOLTIP,
	ST_SHOWING,
	ST_COOLING
};

GUITooltip::GUITooltip()
: m_State(ST_IN_MOTION), m_PreviousObject(NULL), m_PreviousTooltipName(NULL)
{
}

const double CooldownTime = 0.25; // TODO: Don't hard-code this value

static bool GetTooltip(IGUIObject* obj, CStr &style)
{
	if (obj && obj->SettingExists("tooltip_style")
		&& GUI<CStr>::GetSetting(obj, "tooltip_style", style) == PS_OK)
	{
		CStr text;
		if (GUI<CStr>::GetSetting(obj, "tooltip", text) == PS_OK)
		{
			if (text.Length() == 0)
			{
				// No tooltip
				return false;
			}

			if (style.Length() == 0)
				// Text, but no style - use default
				style = "default";

			return true;
		}
	}
	// Failed while retrieving tooltip_style or tooltip
	return false;
}

static void ShowTooltip(IGUIObject* obj, CPos pos, CStr& style, CGUI* gui)
{
	assert(obj);

	// Get the object referenced by 'tooltip_style'
	IGUIObject* tooltipobj = gui->FindObjectByName("__tooltip_"+style);
	if (! tooltipobj)
	{
		LOG_ONCE(ERROR, "gui", "Cannot find tooltip named '%s'", (const char*)style);
		return;
	}

	IGUIObject* usedobj = tooltipobj; // object actually used to display the tooltip in

	CStr usedObjectName;
	if (GUI<CStr>::GetSetting(tooltipobj, "use_object", usedObjectName) == PS_OK
		&& usedObjectName.Length())
	{
		usedobj = gui->FindObjectByName(usedObjectName);
		if (! usedobj)
		{
			LOG_ONCE(ERROR, "gui", "Cannot find object named '%s' used by tooltip '%s'", (const char*)usedObjectName, (const char*)style);
			return;
		}

		// Unhide the object. (If it had use_object and hide_object="true",
		// still unhide it, because the used object might be hidden by default)
		GUI<bool>::SetSetting(usedobj, "hidden", false);
	}
	else
	{
		// Unhide the object
		GUI<bool>::SetSetting(usedobj, "hidden", false);

		// Store mouse position inside the CTooltip
		if (GUI<CPos>::SetSetting(usedobj, "_mousepos", pos) != PS_OK)
			debug_warn("Failed to set tooltip mouse position");
	}

	// Retrieve object's 'tooltip' setting
	CStr text;
	if (GUI<CStr>::GetSetting(obj, "tooltip", text) != PS_OK)
		debug_warn("Failed to retrieve tooltip text"); // shouldn't fail

	// Do some minimal processing ("\n" -> newline, etc)
	text = text.UnescapeBackslashes();

	// Set tooltip's caption
	if (usedobj->SetSetting("caption", text) != PS_OK)
		debug_warn("Failed to set tooltip caption"); // shouldn't fail

	// Make the tooltip object regenerate its text
	usedobj->HandleMessage(SGUIMessage(GUIM_SETTINGS_UPDATED, "caption"));
}

static void HideTooltip(CStr& style, CGUI* gui)
{
	IGUIObject* tooltipobj = gui->FindObjectByName("__tooltip_"+style);
	if (! tooltipobj)
	{
		LOG_ONCE(ERROR, "gui", "Cannot find tooltip named '%s'", (const char*)style);
		return;
	}

	CStr usedObjectName;
	if (GUI<CStr>::GetSetting(tooltipobj, "use_object", usedObjectName) == PS_OK
		&& usedObjectName.Length())
	{
		IGUIObject* usedobj = gui->FindObjectByName(usedObjectName);
		if (! usedobj)
		{
			LOG_ONCE(ERROR, "gui", "Cannot find object named '%s' used by tooltip '%s'", (const char*)usedObjectName, (const char*)style);
			return;
		}

		// Clear the caption
		usedobj->SetSetting("caption", "");
		usedobj->HandleMessage(SGUIMessage(GUIM_SETTINGS_UPDATED, "caption"));


		bool hideobject = true;
		GUI<bool>::GetSetting(tooltipobj, "hide_object", hideobject);

		// If hide_object was enabled, hide it
		if (hideobject)
			GUI<bool>::SetSetting(usedobj, "hidden", true);
	}
	else
	{
		GUI<bool>::SetSetting(tooltipobj, "hidden", true);
	}

}

static int GetTooltipDelay(CStr& style, CGUI* gui)
{
	int delay = 500; // default value (in msec)

	IGUIObject* tooltipobj = gui->FindObjectByName("__tooltip_"+style);
	if (! tooltipobj)
	{
		LOG_ONCE(ERROR, "gui", "Cannot find tooltip object named '%s'", (const char*)style);
		return delay;
	}
	GUI<int>::GetSetting(tooltipobj, "delay", delay);
	return delay;
}

void GUITooltip::Update(IGUIObject* Nearest, CPos MousePos, CGUI* GUI)
{
	// Called once per frame, so efficiency isn't vital


	double now = get_time();

	CStr style;

	int nextstate = -1;

	switch (m_State)
	{
	case ST_IN_MOTION:
		if (MousePos == m_PreviousMousePos)
		{
			if (GetTooltip(Nearest, style))
				nextstate = ST_STATIONARY_TOOLTIP;
			else
				nextstate = ST_STATIONARY_NO_TOOLTIP;
		}
		else
		{
			// Check for movement onto a zero-delayed tooltip
			if (GetTooltip(Nearest, style) && GetTooltipDelay(style, GUI)==0)

				nextstate = ST_SHOWING;
		}
		break;

	case ST_STATIONARY_NO_TOOLTIP:
		if (MousePos != m_PreviousMousePos)
			nextstate = ST_IN_MOTION;
		break;

	case ST_STATIONARY_TOOLTIP:
		if (MousePos != m_PreviousMousePos)
			nextstate = ST_IN_MOTION;
		else if (now >= m_Time)
		{
			// Make sure the tooltip still exists
			if (GetTooltip(Nearest, style))
				nextstate = ST_SHOWING;
			else
			{
				// Failed to retrieve style - the object has probably been
				// altered, so just restart the process
				nextstate = ST_IN_MOTION;
			}
		}
		break;

	case ST_SHOWING:
		if (Nearest != m_PreviousObject)
		{
			if (GetTooltip(Nearest, style))
				nextstate = ST_SHOWING;
			else
				nextstate = ST_COOLING;
		}
		break;

	case ST_COOLING:
		if (GetTooltip(Nearest, style))
			nextstate = ST_SHOWING;
		else if (now >= m_Time)
			nextstate = ST_IN_MOTION;
		break;
	}

	// Handle state-entry code:

	if (nextstate != -1)
	{
		switch (nextstate)
		{
		case ST_STATIONARY_TOOLTIP:
			m_Time = now + (double)GetTooltipDelay(style, GUI) / 1000.;
			break;

		case ST_SHOWING:
			ShowTooltip(Nearest, MousePos, style, GUI);
			m_PreviousTooltipName = style;
			break;

		case ST_COOLING:
			HideTooltip(m_PreviousTooltipName, GUI);
			m_Time = now + CooldownTime;
			break;
		}

		m_State = nextstate;
	}


	m_PreviousMousePos = MousePos;
	m_PreviousObject = Nearest;

}