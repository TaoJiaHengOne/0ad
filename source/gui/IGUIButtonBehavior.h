/*
GUI Object Base - Button Behavior
by Gustav Larsson
gee@pyro.nu

--Overview--

	Interface class that enhance the IGUIObject with
	 buttony behavior (click and release to click a button),
	 and the GUI message GUIM_PRESSED.
	When creating a class with extended settings and
	 buttony behavior, just do a multiple inheritance.

--More info--

	Check GUI.h

*/

#ifndef IGUIButtonBehavior_H
#define IGUIButtonBehavior_H

//--------------------------------------------------------
//  Includes / Compiler directives
//--------------------------------------------------------
#include "GUI.h"

class CGUISpriteInstance;

//--------------------------------------------------------
//  Macros
//--------------------------------------------------------

//--------------------------------------------------------
//  Types
//--------------------------------------------------------

//--------------------------------------------------------
//  Declarations
//--------------------------------------------------------

/**
 * @author Gustav Larsson
 *
 * Appends button behaviours to the IGUIObject.
 * Can be used with multiple inheritance alongside
 * IGUISettingsObject and such.
 *
 * @see IGUIObject
 */
class IGUIButtonBehavior : virtual public IGUIObject
{
public:
	IGUIButtonBehavior();
	virtual ~IGUIButtonBehavior();

	/**
	 * @see IGUIObject#HandleMessage()
	 */
	virtual void HandleMessage(const SGUIMessage &Message);

	/**
	 * This is a function that lets a button being drawn,
	 * it regards if it's over, disabled, pressed and such.
	 * You input sprite names and area and it'll output
	 * it accordingly.
	 *
	 * This class is meant to be used manually in Draw()
	 *
	 * @param rect Rectangle in which the sprite should be drawn
	 * @param z Z-value
	 * @param sprite Sprite drawn when not pressed, hovered or disabled
	 * @param sprite_over Sprite drawn when m_MouseHovering is true
	 * @param sprite_pressed Sprite drawn when m_Pressed is true
	 * @param sprite_disabled Sprite drawn when "enabled" is false
	 * @param cell_id Identifies the icon to be used (if the sprite contains
	 *                cell-using images)
	 */
	void DrawButton(const CRect &rect,
					const float &z,
					CGUISpriteInstance& sprite,
					CGUISpriteInstance& sprite_over,
					CGUISpriteInstance& sprite_pressed,
					CGUISpriteInstance& sprite_disabled,
					int cell_id);

	/**
	 * Choosing which color of the following according to 
	 */
	CColor ChooseColor();


protected:
	virtual void ResetStates()
	{
		m_MouseHovering = false;
		m_Pressed = false;
	}

	/**
	 * Everybody knows how a button works, you don't simply press it,
	 * you have to first press the button, and then release it...
	 * in between those two steps you can actually leave the button
	 * area, as long as you release it within the button area... Anyway
	 * this lets us know we are done with step one (clicking).
	 */
	bool							m_Pressed;
};

#endif
