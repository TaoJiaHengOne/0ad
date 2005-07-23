/*
CList
by Gustav Larsson
gee@pyro.nu
*/

#include "precompiled.h"
#include "GUI.h"
#include "CList.h"

#include "ps/CLogger.h"

using namespace std;

//-------------------------------------------------------------------
//  Constructor / Destructor
//-------------------------------------------------------------------
CList::CList()
{
	// Add sprite_disabled! TODO

	AddSetting(GUIST_float,					"buffer_zone");
	//AddSetting(GUIST_CGUIString,			"caption"); will it break removing this? If I know my system, then no, but test just in case TODO (Gee).
	AddSetting(GUIST_CStr,					"font");
	AddSetting(GUIST_bool,					"scrollbar");
	AddSetting(GUIST_CStr,					"scrollbar_style");
	AddSetting(GUIST_CGUISpriteInstance,	"sprite");
	AddSetting(GUIST_CGUISpriteInstance,	"sprite_selectarea");
	AddSetting(GUIST_int,					"cell_id");
	AddSetting(GUIST_EAlign,				"text_align");
	AddSetting(GUIST_CColor,				"textcolor");
	AddSetting(GUIST_CColor,				"textcolor_selected");
	AddSetting(GUIST_int,					"selected");	// Index selected. -1 is none.
	//AddSetting(GUIST_CStr,					"tooltip");
	//AddSetting(GUIST_CStr,					"tooltip_style");
	AddSetting(GUIST_CGUIList,				"list");

	GUI<bool>::SetSetting(this, "scrollbar", false);

	// Nothing is selected as default.
	GUI<int>::SetSetting(this, "selected", -1);

	// Add scroll-bar
	CGUIScrollBarVertical * bar = new CGUIScrollBarVertical();
	bar->SetRightAligned(true);
	bar->SetUseEdgeButtons(true);
	AddScrollBar(bar);
}

CList::~CList()
{
}

void CList::SetupText()
{
	if (!GetGUI())
		return;

	CGUIList *pList;
	GUI<CGUIList>::GetSettingPointer(this, "list", pList);

	//LOG(ERROR, LOG_CATEGORY, "SetupText() %s", GetPresentableName().c_str());

	//debug_assert(m_GeneratedTexts.size()>=1);

	m_ItemsYPositions.resize( pList->m_Items.size()+1 );

	// Delete all generated texts. Some could probably be saved,
	//  but this is easier, and this function will never be called
	//  continuously, or even often, so it'll probably be okay.
	vector<SGUIText*>::iterator it;
	for (it=m_GeneratedTexts.begin(); it!=m_GeneratedTexts.end(); ++it)
	{
		if (*it)
			delete *it;
	}
	m_GeneratedTexts.clear();

	CStr font;
	if (GUI<CStr>::GetSetting(this, "font", font) != PS_OK || font.Length()==0)
		// Use the default if none is specified
		// TODO Gee: (2004-08-14) Don't define standard like this. Do it with the default style.
		font = "default";

	//CGUIString caption;
	bool scrollbar;
	//GUI<CGUIString>::GetSetting(this, "caption", caption);
	GUI<bool>::GetSetting(this, "scrollbar", scrollbar);

	float width = GetListRect().GetWidth();
	// remove scrollbar if applicable
	if (scrollbar && GetScrollBar(0).GetStyle())
		width -= GetScrollBar(0).GetStyle()->m_Width;

	float buffer_zone=0.f;
	GUI<float>::GetSetting(this, "buffer_zone", buffer_zone);

	// Generate texts
	float buffered_y = 0.f;
	
	for (int i=0; i<(int)pList->m_Items.size(); ++i)
	{
		// Create a new SGUIText. Later on, input it using AddText()
		SGUIText *text = new SGUIText();

		*text = GetGUI()->GenerateText(pList->m_Items[i], font, width, buffer_zone, this);

		m_ItemsYPositions[i] = buffered_y;
		buffered_y += text->m_Size.cy;

		AddText(text);
	}

	m_ItemsYPositions[pList->m_Items.size()] = buffered_y;
	
	//if (! scrollbar)
	//	CalculateTextPosition(m_CachedActualSize, m_TextPos, *m_GeneratedTexts[0]);

	// Setup scrollbar
	if (scrollbar)
	{
		GetScrollBar(0).SetScrollRange( m_ItemsYPositions.back() );
		GetScrollBar(0).SetScrollSpace( GetListRect().GetHeight() );
	}
}

void CList::HandleMessage(const SGUIMessage &Message)
{
	IGUIScrollBarOwner::HandleMessage(Message);
	//IGUITextOwner::HandleMessage(Message); <== placed it after the switch instead!

	switch (Message.type)
	{
	case GUIM_SETTINGS_UPDATED:
		bool scrollbar;
		GUI<bool>::GetSetting(this, "scrollbar", scrollbar);

		// Update scroll-bar
		// TODO Gee: (2004-09-01) Is this really updated each time it should?
		if (scrollbar && 
		    (Message.value == CStr("size") || 
			 Message.value == CStr("z") ||
			 Message.value == CStr("absolute")))
		{
			CRect rect = GetListRect();
			GetScrollBar(0).SetX( rect.right );
			GetScrollBar(0).SetY( rect.top );
			GetScrollBar(0).SetZ( GetBufferedZ() );
			GetScrollBar(0).SetLength( rect.bottom - rect.top );
		}

		if (Message.value == "list")
		{
			SetupText();
		}

		// If selected is changed, call "SelectionChange"
		if (Message.value == "selected")
		{
			// TODO: Check range

			// TODO only works if lower-case, shouldn't it be made case sensitive instead?
			ScriptEvent("selectionchange"); 
		}

		if (Message.value == CStr("scrollbar"))
		{
			SetupText();
		}

		// Update scrollbar
		if (Message.value == CStr("scrollbar_style"))
		{
			CStr scrollbar_style;
			GUI<CStr>::GetSetting(this, Message.value, scrollbar_style);

			GetScrollBar(0).SetScrollBarStyle( scrollbar_style );

			SetupText();
		}

		break;

	case GUIM_MOUSE_PRESS_LEFT:
	{
		bool scrollbar;
		CGUIList *pList;
		GUI<bool>::GetSetting(this, "scrollbar", scrollbar);
		GUI<CGUIList>::GetSettingPointer(this, "list", pList);
		float scroll=0.f;
		if (scrollbar)
		{
			scroll = GetScrollBar(0).GetPos();
		}

		CRect rect = GetListRect();
		CPos mouse = GetMousePos();
		mouse.y += scroll;
		int set=-1;
		for (int i=0; i<(int)pList->m_Items.size(); ++i)
		{
			if (mouse.y >= rect.top + m_ItemsYPositions[i] &&
				mouse.y < rect.top + m_ItemsYPositions[i+1] &&
				// mouse is not over scroll-bar
				!(mouse.x >= GetScrollBar(0).GetOuterRect().left &&
				mouse.x <= GetScrollBar(0).GetOuterRect().right))
			{
				set = i;
			}
		}
		
		if (set != -1)
		{
			GUI<int>::SetSetting(this, "selected", set);
			UpdateAutoScroll();
		}
	}	break;

	case GUIM_MOUSE_WHEEL_DOWN:
		GetScrollBar(0).ScrollMinus();
		// Since the scroll was changed, let's simulate a mouse movement
		//  to check if scrollbar now is hovered
		HandleMessage(SGUIMessage(GUIM_MOUSE_MOTION));
		break;

	case GUIM_MOUSE_WHEEL_UP:
		GetScrollBar(0).ScrollPlus();
		// Since the scroll was changed, let's simulate a mouse movement
		//  to check if scrollbar now is hovered
		HandleMessage(SGUIMessage(GUIM_MOUSE_MOTION));
		break;

	case GUIM_LOAD:
		{
		CRect rect = GetListRect();
		GetScrollBar(0).SetX( rect.right );
		GetScrollBar(0).SetY( rect.top );
		GetScrollBar(0).SetZ( GetBufferedZ() );
		GetScrollBar(0).SetLength( rect.bottom - rect.top );

		CStr scrollbar_style;
		GUI<CStr>::GetSetting(this, "scrollbar_style", scrollbar_style);
		GetScrollBar(0).SetScrollBarStyle( scrollbar_style );
		}
		break;

	default:
		break;
	}

	IGUITextOwner::HandleMessage(Message);
}

int CList::ManuallyHandleEvent(const SDL_Event* ev)
{
	int szChar = ev->key.keysym.sym;

	switch (szChar)
	{
	case SDLK_HOME:
		SelectFirstElement();
		UpdateAutoScroll();
		break;

	case SDLK_END:
		SelectLastElement();
		UpdateAutoScroll();
		break;

	case SDLK_UP:
		SelectPrevElement();
		UpdateAutoScroll();
		break;

	case SDLK_DOWN:
		SelectNextElement();
		UpdateAutoScroll();
		break;

	case SDLK_PAGEUP:
		GetScrollBar(0).ScrollMinusPlenty();
		break;

	case SDLK_PAGEDOWN:
		GetScrollBar(0).ScrollPlusPlenty();
		break;

	default: // Do nothing
		break;
	}

	return EV_HANDLED;
}

void CList::Draw() 
{
	int selected;
	GUI<int>::GetSetting(this, "selected", selected);
    
	DrawList(selected, "sprite", "sprite_selectarea", "textcolor");
}

void CList::DrawList(const int &selected,
					 const CStr &_sprite, 
					 const CStr &_sprite_selected, 
					 const CStr &_textcolor)
{
	float bz = GetBufferedZ();

	// First call draw on ScrollBarOwner
	bool scrollbar;
	GUI<bool>::GetSetting(this, "scrollbar", scrollbar);

	if (scrollbar)
	{
		// Draw scrollbar
		IGUIScrollBarOwner::Draw();
	}

	if (GetGUI())
	{
		CRect rect = GetListRect();

		CGUISpriteInstance *sprite=NULL, *sprite_selectarea=NULL;
		int cell_id;
		GUI<CGUISpriteInstance>::GetSettingPointer(this, _sprite, sprite);
		GUI<CGUISpriteInstance>::GetSettingPointer(this, _sprite_selected, sprite_selectarea);
		GUI<int>::GetSetting(this, "cell_id", cell_id);

		CGUIList *pList;
		GUI<CGUIList>::GetSettingPointer(this, "list", pList);

		GetGUI()->DrawSprite(*sprite, cell_id, bz, rect);

		float scroll=0.f;
		if (scrollbar)
		{
			scroll = GetScrollBar(0).GetPos();
		}

		if (selected != -1)
		{
			debug_assert(selected >= 0 && selected+1 < (int)m_ItemsYPositions.size());

			// Get rectangle of selection:
			CRect rect_sel(rect.left, rect.top + m_ItemsYPositions[selected] - scroll,
					       rect.right, rect.top + m_ItemsYPositions[selected+1] - scroll);

			if (rect_sel.top <= rect.bottom &&
				rect_sel.bottom >= rect.top)
			{
				if (rect_sel.bottom > rect.bottom)
					rect_sel.bottom = rect.bottom;
				if (rect_sel.top < rect.top)
					rect_sel.top = rect.top;

				if (scrollbar)
				{
					// Remove any overlapping area of the scrollbar.

					if (rect_sel.right > GetScrollBar(0).GetOuterRect().left &&
						rect_sel.right <= GetScrollBar(0).GetOuterRect().right)
						rect_sel.right = GetScrollBar(0).GetOuterRect().left;

					if (rect_sel.left >= GetScrollBar(0).GetOuterRect().left &&
						rect_sel.left < GetScrollBar(0).GetOuterRect().right)
						rect_sel.left = GetScrollBar(0).GetOuterRect().right;
				}

				GetGUI()->DrawSprite(*sprite_selectarea, cell_id, bz+0.05f, rect_sel);
			}
		}

		CColor color;
		GUI<CColor>::GetSetting(this, _textcolor, color);

		for (int i=0; i<(int)pList->m_Items.size(); ++i)
		{
			if (m_ItemsYPositions[i+1] - scroll < 0 ||
				m_ItemsYPositions[i] - scroll > rect.GetHeight())
				continue;

			IGUITextOwner::Draw(i, color, rect.TopLeft() - CPos(0.f, scroll - m_ItemsYPositions[i]), bz+0.1f);
		}
	}
}

void CList::AddItem(const CStr& str) 
{
	CGUIList *pList;
	GUI<CGUIList>::GetSettingPointer(this, "list", pList);

	CGUIString gui_string; 
	gui_string.SetValue(str); 
	pList->m_Items.push_back( gui_string );

	// TODO Temp
	SetupText();
}

bool CList::HandleAdditionalChildren(const XMBElement& child, CXeromyces* pFile)
{
	int elmt_item = pFile->getElementID("item");

	if (child.getNodeName() == elmt_item)
	{
		AddItem((CStr)child.getText());
		
		return true;
	}
	else
	{
		return false;
	}
}

void CList::SelectNextElement()
{
	int selected;
	GUI<int>::GetSetting(this, "selected", selected);

	CGUIList *pList;
	GUI<CGUIList>::GetSettingPointer(this, "list", pList);

	if (selected != pList->m_Items.size()-1)
	{
		++selected;
		GUI<int>::SetSetting(this, "selected", selected);
	}
}
	
void CList::SelectPrevElement()
{
	int selected;
	GUI<int>::GetSetting(this, "selected", selected);

	if (selected != 0)
	{
		--selected;
		GUI<int>::SetSetting(this, "selected", selected);
	}
}

void CList::SelectFirstElement()
{
	int selected;
	GUI<int>::GetSetting(this, "selected", selected);

	if (selected != 0)
	{
		GUI<int>::SetSetting(this, "selected", 0);
	}
}
	
void CList::SelectLastElement()
{
	int selected;
	GUI<int>::GetSetting(this, "selected", selected);

	CGUIList *pList;
	GUI<CGUIList>::GetSettingPointer(this, "list", pList);

	if (selected != pList->m_Items.size()-1)
	{
		GUI<int>::SetSetting(this, "selected", (int)pList->m_Items.size()-1);
	}
}

void CList::UpdateAutoScroll()
{
	int selected;
	bool scrollbar;
	float scroll;
	GUI<int>::GetSetting(this, "selected", selected);
	GUI<bool>::GetSetting(this, "scrollbar", scrollbar);

	CRect rect = GetListRect();

	// No scrollbar, no scrolling (at least it's not made to work properly).
	if (!scrollbar)
		return;

	scroll = GetScrollBar(0).GetPos();

	// Check upper boundary
	if (m_ItemsYPositions[selected] < scroll)
	{
		GetScrollBar(0).SetPos(m_ItemsYPositions[selected]);
		return; // this means, if it wants to align both up and down at the same time
				//  this will have precedence.
	}

	// Check lower boundary
	if (m_ItemsYPositions[selected+1]-rect.GetHeight() > scroll)
	{
		GetScrollBar(0).SetPos(m_ItemsYPositions[selected+1]-rect.GetHeight());
	}
}
