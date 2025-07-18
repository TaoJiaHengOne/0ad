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

#include "CDropDown.h"

#include "gui/CGUI.h"
#include "gui/IGUIScrollBar.h"
#include "gui/SettingTypes/CGUIColor.h"
#include "gui/SettingTypes/CGUIList.h"
#include "lib/external_libraries/libsdl.h"
#include "lib/timer.h"
#include "ps/Profile.h"

CDropDown::CDropDown(CGUI& pGUI)
	: CList(pGUI),
	  m_Open(),
	  m_HideScrollBar(),
	  m_ElementHighlight(-1),
	  m_ButtonWidth(this, "button_width"),
	  m_DropDownSize(this, "dropdown_size"),
	  m_DropDownBuffer(this, "dropdown_buffer"),
	  m_MinimumVisibleItems(this, "minimum_visible_items"),
	  m_SoundClosed(this, "sound_closed"),
	  m_SoundEnter(this, "sound_enter"),
	  m_SoundLeave(this, "sound_leave"),
	  m_SoundOpened(this, "sound_opened"),
	  // Setting "sprite" is registered by CList and used as the background
	  m_SpriteDisabled(this, "sprite_disabled"),
	  m_SpriteOverlayDisabled(this, "sprite_overlay_disabled"),
	  m_SpriteList(this, "sprite_list"), // Background of the drop down list
	  m_SpriteListOverlay(this, "sprite_list_overlay"), // Overlay above the drop down list
	  m_Sprite2(this, "sprite2"), // Button that sits to the right
	  m_Sprite2Over(this, "sprite2_over"),
	  m_Sprite2Pressed(this, "sprite2_pressed"),
	  m_Sprite2Disabled(this, "sprite2_disabled"),
	  m_TextColorDisabled(this, "textcolor_disabled")
	  // Add these in CList! And implement TODO
	  //m_TextColorOver("textcolor_over");
	  //m_TextColorPressed("textcolor_pressed");
{
	m_ScrollBar.Set(true, true);
}

CDropDown::~CDropDown()
{
}

void CDropDown::SetupText()
{
	SetupListRect();
	CList::SetupText();
}

void CDropDown::UpdateCachedSize()
{
	CList::UpdateCachedSize();
	SetupText();
}

void CDropDown::HandleMessage(SGUIMessage& Message)
{
	// CList::HandleMessage(Message); placed after the switch!

	switch (Message.type)
	{
	case GUIM_SETTINGS_UPDATED:
	{
		// Update cached list rect
		if (Message.value == "size" ||
			Message.value == "absolute" ||
			Message.value == "dropdown_size" ||
			Message.value == "dropdown_buffer" ||
			Message.value == "minimum_visible_items" ||
			Message.value == "scrollbar_style" ||
			Message.value == "button_width")
		{
			SetupListRect();
		}

		break;
	}

	case GUIM_MOUSE_MOTION:
	{
		if (!m_Open)
			break;

		CVector2D mouse = m_pGUI.GetMousePos();

		if (!GetListRect().PointInside(mouse))
			break;

		const float scroll = m_ScrollBar ? GetScrollBar(0).GetPos() : 0.f;

		CRect rect = GetListRect();
		mouse.Y += scroll;
		int set = -1;
		for (int i = 0; i < static_cast<int>(m_List->m_Items.size()); ++i)
		{
			if (mouse.Y >= rect.top + m_ItemsYPositions[i] &&
				mouse.Y < rect.top + m_ItemsYPositions[i+1] &&
				// mouse is not over scroll-bar
				(m_HideScrollBar ||
					mouse.X < GetScrollBar(0).GetOuterRect().left ||
					mouse.X > GetScrollBar(0).GetOuterRect().right))
			{
				set = i;
			}
		}

		if (set != -1)
		{
			m_ElementHighlight = set;
			//UpdateAutoScroll();
		}

		break;
	}

	case GUIM_MOUSE_ENTER:
	{
		if (m_Enabled)
			PlaySound(m_SoundEnter);
		break;
	}

	case GUIM_MOUSE_LEAVE:
	{
		m_ElementHighlight = m_Selected;

		if (m_Enabled)
			PlaySound(m_SoundLeave);
		break;
	}

	// We can't inherent this routine from CList, because we need to include
	// a mouse click to open the dropdown, also the coordinates are changed.
	case GUIM_MOUSE_PRESS_LEFT:
	{
		if (!m_Enabled)
		{
			PlaySound(m_SoundDisabled);
			break;
		}

		if (!m_Open)
		{
			if (m_List->m_Items.empty())
				return;

			if (m_VisibleArea && !m_VisibleArea.PointInside(m_pGUI.GetMousePos()))
				return;

			m_Open = true;
			GetScrollBar(0).SetZ(GetBufferedZ());
			m_ElementHighlight = m_Selected;

			// Start at the position of the selected item, if possible.
			if (m_ItemsYPositions.empty() || m_ElementHighlight < 0 || static_cast<size_t>(m_ElementHighlight) >= m_ItemsYPositions.size())
				GetScrollBar(0).SetPos(0);
			else
				GetScrollBar(0).SetPos(m_ItemsYPositions[m_ElementHighlight] - 60);

			PlaySound(m_SoundOpened);
			return; // overshadow
		}
		else
		{
			const CVector2D& mouse = m_pGUI.GetMousePos();

			// If the regular area is pressed, then abort, and close.
			if (m_CachedActualSize.PointInside(mouse))
			{
				m_Open = false;
				GetScrollBar(0).SetZ(GetBufferedZ());
				PlaySound(m_SoundClosed);
				return; // overshadow
			}

			if (m_HideScrollBar ||
				mouse.X < GetScrollBar(0).GetOuterRect().left ||
				mouse.X > GetScrollBar(0).GetOuterRect().right ||
				mouse.Y < GetListRect().top)
			{
				m_Open = false;
				GetScrollBar(0).SetZ(GetBufferedZ());
			}
		}
		break;
	}

	case GUIM_MOUSE_WHEEL_DOWN:
	{
		// Don't switch elements by scrolling when open, causes a confusing interaction between this and the scrollbar.
		if (m_Open || !m_Enabled)
			break;

		Message.Skip(false);

		m_ElementHighlight = m_Selected;

		if (m_ElementHighlight + 1 >= (int)m_ItemsYPositions.size() - 1)
			break;

		++m_ElementHighlight;
		m_Selected.Set(m_ElementHighlight, true);
		break;
	}

	case GUIM_MOUSE_WHEEL_UP:
	{
		// Don't switch elements by scrolling when open, causes a confusing interaction between this and the scrollbar.
		if (m_Open || !m_Enabled)
			break;

		Message.Skip(false);

		m_ElementHighlight = m_Selected;
		if (m_ElementHighlight - 1 < 0)
			break;

		--m_ElementHighlight;
		m_Selected.Set(m_ElementHighlight, true);
		break;
	}

	case GUIM_LOST_FOCUS:
	{
		if (m_Open)
			PlaySound(m_SoundClosed);

		m_Open = false;
		break;
	}

	case GUIM_LOAD:
		SetupListRect();
		break;

	default:
		break;
	}

	// Important that this is after, so that overshadowed implementations aren't processed
	CList::HandleMessage(Message);

	// As HandleMessage functions return void, we need to manually verify
	// whether the child list's items were modified.
	if (CList::GetModified())
		SetupText();
}

InReaction CDropDown::ManuallyHandleKeys(const SDL_Event_* ev)
{
	InReaction result = IN_PASS;
	bool update_highlight = false;

	if (ev->ev.type == SDL_KEYDOWN)
	{
		int szChar = ev->ev.key.keysym.sym;

		switch (szChar)
		{
		case '\r':
			m_Open = false;
			result = IN_HANDLED;
			break;

		case SDLK_HOME:
		case SDLK_END:
		case SDLK_UP:
		case SDLK_DOWN:
		case SDLK_PAGEUP:
		case SDLK_PAGEDOWN:
			if (!m_Open)
				return IN_PASS;
			// Set current selected item to highlighted, before
			//  then really processing these in CList::ManuallyHandleKeys()
			m_Selected.Set(m_ElementHighlight, true);
			update_highlight = true;
			break;

		default:
			// If we have typed a character try to get the closest element to it.
			// TODO: not too nice and doesn't deal with dashes.
			if (m_Open && ((szChar >= SDLK_a && szChar <= SDLK_z) || szChar == SDLK_SPACE
						   || (szChar >= SDLK_0 && szChar <= SDLK_9)
						   || (szChar >= SDLK_KP_1 && szChar <= SDLK_KP_0)))
			{
				// arbitrary 1 second limit to add to string or start fresh.
				// maximal amount of characters is 100, which imo is far more than enough.
				if (timer_Time() - m_TimeOfLastInput > 1.0 || m_InputBuffer.length() >= 100)
					m_InputBuffer = szChar;
				else
					m_InputBuffer += szChar;

				m_TimeOfLastInput = timer_Time();

				// let's look for the closest element
				// basically it's alphabetic order and "as many letters as we can get".
				int closest = -1;
				int bestIndex = -1;
				int difference = 1250;
				for (int i = 0; i < static_cast<int>(m_List->m_Items.size()); ++i)
				{
					int indexOfDifference = 0;
					int diff = 0;
					for (size_t j = 0; j < m_InputBuffer.length(); ++j)
					{
						diff = std::abs(static_cast<int>(m_List->m_Items[i].GetRawString().LowerCase()[j]) - static_cast<int>(m_InputBuffer[j]));
						if (diff == 0)
							indexOfDifference = j+1;
						else
							break;
					}
					if (indexOfDifference > bestIndex || (indexOfDifference >= bestIndex && diff < difference))
					{
						bestIndex = indexOfDifference;
						closest = i;
						difference = diff;
					}
				}
				// let's select the closest element. There should basically always be one.
				if (closest != -1)
				{
					m_Selected.Set(closest, true);
					update_highlight = true;
					GetScrollBar(0).SetPos(m_ItemsYPositions[closest] - 60);
				}
				result = IN_HANDLED;
			}
			break;
		}
	}

	if (CList::ManuallyHandleKeys(ev) == IN_HANDLED)
		result = IN_HANDLED;

	if (update_highlight)
		m_ElementHighlight = m_Selected;

	return result;
}

void CDropDown::SetupListRect()
{
	const CSize2D windowSize = m_pGUI.GetWindowSize();

	if (m_ItemsYPositions.empty())
	{
		m_CachedListRect = CRect(m_CachedActualSize.left, m_CachedActualSize.bottom + m_DropDownBuffer,
		                         m_CachedActualSize.right, m_CachedActualSize.bottom + m_DropDownBuffer + m_DropDownSize);
		m_HideScrollBar = false;
	}
	// Too many items so use a scrollbar
	else if (m_ItemsYPositions.back() > m_DropDownSize)
	{
		// Place items below if at least some items can be placed below
		if (m_CachedActualSize.bottom + m_DropDownBuffer + m_DropDownSize <= windowSize.Height)
			m_CachedListRect = CRect(m_CachedActualSize.left, m_CachedActualSize.bottom + m_DropDownBuffer,
			                         m_CachedActualSize.right, m_CachedActualSize.bottom + m_DropDownBuffer + m_DropDownSize);
		else if ((m_ItemsYPositions.size() > m_MinimumVisibleItems && windowSize.Height - m_CachedActualSize.bottom - m_DropDownBuffer >= m_ItemsYPositions[m_MinimumVisibleItems]) ||
		         m_CachedActualSize.top < windowSize.Height - m_CachedActualSize.bottom)
			m_CachedListRect = CRect(m_CachedActualSize.left, m_CachedActualSize.bottom + m_DropDownBuffer,
			                         m_CachedActualSize.right, windowSize.Height);
		// Not enough space below, thus place items above
		else
			m_CachedListRect = CRect(m_CachedActualSize.left, std::max(0.f, m_CachedActualSize.top - m_DropDownBuffer - m_DropDownSize),
			                         m_CachedActualSize.right, m_CachedActualSize.top - m_DropDownBuffer);

		m_HideScrollBar = false;
	}
	else
	{
		// Enough space below, no scrollbar needed
		if (m_CachedActualSize.bottom + m_DropDownBuffer + m_ItemsYPositions.back() <= windowSize.Height)
		{
			m_CachedListRect = CRect(m_CachedActualSize.left, m_CachedActualSize.bottom + m_DropDownBuffer,
			                         m_CachedActualSize.right, m_CachedActualSize.bottom + m_DropDownBuffer + m_ItemsYPositions.back());
			m_HideScrollBar = true;
		}
		// Enough space below for some items, but not all, so place items below and use a scrollbar
		else if ((m_ItemsYPositions.size() > m_MinimumVisibleItems && windowSize.Height - m_CachedActualSize.bottom - m_DropDownBuffer >= m_ItemsYPositions[m_MinimumVisibleItems]) ||
		         m_CachedActualSize.top < windowSize.Height - m_CachedActualSize.bottom)
		{
			m_CachedListRect = CRect(m_CachedActualSize.left, m_CachedActualSize.bottom + m_DropDownBuffer,
			                         m_CachedActualSize.right, windowSize.Height);
			m_HideScrollBar = false;
		}
		// Not enough space below, thus place items above. Hide the scrollbar accordingly
		else
		{
			m_CachedListRect = CRect(m_CachedActualSize.left, std::max(0.f, m_CachedActualSize.top - m_DropDownBuffer - m_ItemsYPositions.back()),
			                         m_CachedActualSize.right, m_CachedActualSize.top - m_DropDownBuffer);
			m_HideScrollBar = m_CachedActualSize.top > m_ItemsYPositions.back() + m_DropDownBuffer;
		}
	}
}

CRect CDropDown::GetListRect() const
{
	return m_CachedListRect;
}

bool CDropDown::IsMouseOver() const
{
	if (m_Open)
	{
		CRect rect(m_CachedActualSize.left, std::min(m_CachedActualSize.top, GetListRect().top),
		           m_CachedActualSize.right, std::max(m_CachedActualSize.bottom, GetListRect().bottom));
		return rect.PointInside(m_pGUI.GetMousePos());
	}
	else
		return IGUIObject::IsMouseOver();
}

void CDropDown::Draw(CCanvas2D& canvas)
{
	const CGUISpriteInstance& sprite = m_Enabled ? m_Sprite : m_SpriteDisabled;
	const CGUISpriteInstance& spriteOverlay = m_Enabled ? m_SpriteOverlay : m_SpriteOverlayDisabled;

	m_pGUI.DrawSprite(sprite, canvas, m_CachedActualSize, m_VisibleArea);

	if (m_ButtonWidth > 0.f)
	{
		CRect rect(m_CachedActualSize.right - m_ButtonWidth, m_CachedActualSize.top,
				   m_CachedActualSize.right, m_CachedActualSize.bottom);

		if (!m_Enabled)
		{
			m_pGUI.DrawSprite(*m_Sprite2Disabled ? m_Sprite2Disabled : m_Sprite2, canvas, rect, m_VisibleArea);
		}
		else if (m_Open)
		{
			m_pGUI.DrawSprite(*m_Sprite2Pressed ? m_Sprite2Pressed : m_Sprite2, canvas, rect, m_VisibleArea);
		}
		else if (m_MouseHovering)
		{
			m_pGUI.DrawSprite(*m_Sprite2Over ? m_Sprite2Over : m_Sprite2, canvas, rect, m_VisibleArea);
		}
		else
			m_pGUI.DrawSprite(m_Sprite2, canvas, rect, m_VisibleArea);
	}

	if (m_Selected != -1) // TODO: Maybe check validity completely?
	{
		CRect cliparea = m_VisibleArea != CRect() ? m_VisibleArea : m_CachedActualSize;
		if (cliparea.right > m_CachedActualSize.right - m_ButtonWidth)
			cliparea.right = m_CachedActualSize.right - m_ButtonWidth;

		CVector2D pos(m_CachedActualSize.left, m_CachedActualSize.top);
		DrawText(canvas, m_Selected, m_Enabled ? m_TextColorSelected : m_TextColorDisabled, pos, cliparea);
	}

	if (m_Open)
	{
		// Disable scrollbar during drawing without sending a setting-changed message
		const bool old = m_ScrollBar;

		// TODO: drawScrollbar as an argument of DrawList?
		if (m_HideScrollBar)
			m_ScrollBar.Set(false, false);

		DrawList(canvas, m_ElementHighlight, m_SpriteList, m_SpriteListOverlay, m_SpriteSelectArea, m_SpriteSelectAreaOverlay, m_TextColor);

		if (m_HideScrollBar)
			m_ScrollBar.Set(old, false);
	}
	m_pGUI.DrawSprite(spriteOverlay, canvas, m_CachedActualSize, m_VisibleArea);
}

// When a dropdown list is opened, it needs to be visible above all the other
// controls on the page. The only way I can think of to do this is to increase
// its z value when opened, so that it's probably on top.
float CDropDown::GetBufferedZ() const
{
	float bz = CList::GetBufferedZ();
	if (m_Open)
		return std::min(bz + 500.f, 1000.f); // TODO - don't use magic number for max z value
	else
		return bz;
}
