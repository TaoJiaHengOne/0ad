/*
GUI Core, stuff that the whole GUI uses
by Gustav Larsson
gee@pyro.nu

--Overview--

	Contains defines, includes, types etc that the whole 
	 GUI should have included.

--More info--

	Check GUI.h

*/

#ifndef GUIbase_H
#define GUIbase_H


//--------------------------------------------------------
//  Includes / Compiler directives
//--------------------------------------------------------
#include <map>
#include <vector>

// I would like to just forward declare CSize, but it doesn't
//  seem to be defined anywhere in the predefined header.
#include "ps/Overlay.h"

#include "ps/Pyrogenesis.h"	// deprecated DECLARE_ERROR

//--------------------------------------------------------
//  Forward declarations
//--------------------------------------------------------
class IGUIObject;

//--------------------------------------------------------
//  Macros
//--------------------------------------------------------

// Global CGUI
#define g_GUI CGUI::GetSingleton()

// Object settings setups

// Setup an object's ConstructObject function
#define GUI_OBJECT(obj)													\
public:																	\
	static IGUIObject *ConstructObject() { return new obj(); }


//--------------------------------------------------------
//  Types
//--------------------------------------------------------
/** 
 * @enum EGUIMessage
 * Message types
 *
 * @see SGUIMessage
 */
enum EGUIMessageType
{
	GUIM_PREPROCESS,		// TODO questionable
	GUIM_POSTPROCESS,		// TODO questionable
	GUIM_MOUSE_OVER,
	GUIM_MOUSE_ENTER,
	GUIM_MOUSE_LEAVE,
	GUIM_MOUSE_PRESS_LEFT,
	GUIM_MOUSE_PRESS_RIGHT,
	GUIM_MOUSE_DOWN_LEFT,
	GUIM_MOUSE_DOWN_RIGHT,
	GUIM_MOUSE_DBLCLICK_LEFT,
	GUIM_MOUSE_DBLCLICK_RIGHT,
	GUIM_MOUSE_RELEASE_LEFT,
	GUIM_MOUSE_RELEASE_RIGHT,
	GUIM_MOUSE_WHEEL_UP,
	GUIM_MOUSE_WHEEL_DOWN,
	GUIM_SETTINGS_UPDATED,	// SGUIMessage.m_Value = name of setting
	GUIM_PRESSED,
	GUIM_MOUSE_MOTION,
	GUIM_LOAD,				// Called when an object is added to the GUI.
	GUIM_GOT_FOCUS,
	GUIM_LOST_FOCUS
};

/**
 * @author Gustav Larsson
 *
 * Message send to IGUIObject::HandleMessage() in order
 * to give life to Objects manually with
 * a derived HandleMessage().
 */
struct SGUIMessage
{
	SGUIMessage() {}
	SGUIMessage(const EGUIMessageType &_type) : type(_type) {}
	SGUIMessage(const EGUIMessageType &_type, const CStr& _value) : type(_type), value(_value) {}
	~SGUIMessage() {}

	/**
	 * Describes what the message regards
	 */
	EGUIMessageType type;

	/**
	 * Optional data
	 */
	CStr value;
};

/**
 * Recurse restrictions, when we recurse, if an object
 * is hidden for instance, you might want it to skip
 * the children also
 * Notice these are flags! and we don't really need one
 * for no restrictions, because then you'll just enter 0
 */
enum
{
	GUIRR_HIDDEN		= 0x00000001,
	GUIRR_DISABLED		= 0x00000010,
	GUIRR_GHOST			= 0x00000100
};

/**
 * @enum EGUISettingsStruct
 * 
 * Stored in SGUISetting, tells us in which struct
 * the setting is located, that way we can query
 * for the structs address.
 */
enum EGUISettingsStruct
{
	GUISS_BASE,
	GUISS_EXTENDED
};

// Text alignments
enum EAlign { EAlign_Left, EAlign_Right, EAlign_Center };
enum EVAlign { EVAlign_Top, EVAlign_Bottom, EVAlign_Center };

// Typedefs
typedef	std::map<CStr, IGUIObject*> map_pObjects;
typedef std::vector<IGUIObject*> vector_pObjects;

// Icon, you create them in the XML file with root element <setup>
//  you use them in text owned by different objects... Such as CText.
struct SGUIIcon
{
	SGUIIcon() : m_CellID(0) {}

	// Sprite name of icon
	CStr m_SpriteName;

	// Size
	CSize m_Size;

	// Cell of texture to use; ignored unless the texture has specified cell-size
	int m_CellID;
};

/**
 * @author Gustav Larsson
 *
 * Client Area is a rectangle relative to a parent rectangle
 *
 * You can input the whole value of the Client Area by
 * string. Like used in the GUI.
 */
class CClientArea
{
public:
	CClientArea();
	CClientArea(const CStr& Value);

	/// Pixel modifiers
	CRect pixel;

	/// Percent modifiers
	CRect percent;

	/**
	 * Get client area rectangle when the parent is given
	 */
	CRect GetClientArea(const CRect &parent) const;

	/**
	 * The ClientArea can be set from a string looking like:
	 *
	 * "0 0 100% 100%"
	 * "50%-10 50%-10 50%+10 50%+10"
	 * 
	 * i.e. First percent modifier, then + or - and the pixel modifier.
	 * Although you can use just the percent or the pixel modifier. Notice
	 * though that the percent modifier must always be the first when
	 * both modifiers are inputted.
	 *
	 * @return true if success, false if failure. If false then the client area
	 *			will be unchanged.
	 */
	bool SetClientArea(const CStr& Value);
};

//--------------------------------------------------------
//  Error declarations
//--------------------------------------------------------
DECLARE_ERROR(PS_NAME_TAKEN);
DECLARE_ERROR(PS_OBJECT_FAIL);
DECLARE_ERROR(PS_SETTING_FAIL);
DECLARE_ERROR(PS_VALUE_INVALID);
DECLARE_ERROR(PS_NEEDS_PGUI);
DECLARE_ERROR(PS_NAME_AMBIGUITY);
DECLARE_ERROR(PS_NEEDS_NAME);

DECLARE_ERROR(PS_LEXICAL_FAIL);
DECLARE_ERROR(PS_SYNTACTICAL_FAIL);

#endif
