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

 /*
	 A GUI Scrollbar, this class doesn't present all functionality
	 to the scrollbar, it just controls the drawing and a wrapper
	 for interaction with it.
 */

#ifndef INCLUDED_CGUISCROLLBARHORIZONTAL
#define INCLUDED_CGUISCROLLBARHORIZONTAL

#include "IGUIScrollBar.h"

 /**
  * Horizontal implementation of IGUIScrollBar
  *
  * @see IGUIScrollBar
  */
class CGUIScrollBarHorizontal : public IGUIScrollBar
{
public:
	CGUIScrollBarHorizontal(CGUI& pGUI);
	virtual ~CGUIScrollBarHorizontal();

	/**
	 * Draw the scroll-bar
	 */
	virtual void Draw(CCanvas2D& canvas);

	/**
	 * If an object that contains a scrollbar has got messages, send
	 * them to the scroll-bar and it will see if the message regarded
	 * itself.
	 *
	 * @see IGUIObject#HandleMessage()
	 */
	virtual void HandleMessage(SGUIMessage& Message);

	/**
	 * Set m_Pos with g_mouse_x/y input, i.e. when dragging.
	 */
	virtual void SetPosFromMousePos(const CVector2D& mouse);

	virtual void SetScrollPlentyFromMousePos(const CVector2D& mouse);

	/**
	 * @see IGUIScrollBar#HoveringButtonMinus
	 */
	virtual bool HoveringButtonMinus(const CVector2D& mouse);

	/**
	 * @see IGUIScrollBar#HoveringButtonPlus
	 */
	virtual bool HoveringButtonPlus(const CVector2D& mouse);

	/**
	 * Set Right Aligned
	 * @param align Alignment
	 */
	void SetBottomAligned(const bool& align) { m_BottomAligned = align; }

	/**
	 * Get the rectangle of the actual BAR.
	 * @return Rectangle, CRect
	 */
	virtual CRect GetBarRect() const;

	/**
	 * Get the rectangle of the outline of the scrollbar, every component of the
	 * scroll-bar should be inside this area.
	 * @return Rectangle, CRect
	 */
	virtual CRect GetOuterRect() const;

protected:
	/**
	 * Should the scroll bar proceed to the top or to the bottom of the m_Y value.
	 * Notice, this has nothing to do with where the owner places it.
	 */
	bool m_BottomAligned;
};

#endif // INCLUDED_CGUISCROLLBARHORIZONTAL
