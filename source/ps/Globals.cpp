#include "precompiled.h"

#include "lib/input.h"
#include "Globals.h"


bool g_app_minimized = false;
bool g_app_has_focus = true;

bool g_keys[SDLK_LAST] = {0};
int g_mouse_x = 50, g_mouse_y = 50;

// unused, left, right, middle, wheel up, wheel down
// (order is given by SDL_BUTTON_* constants).
bool g_mouse_buttons[6] = {0};


// updates the state of the above; never swallows messages.
InReaction GlobalsInputHandler(const SDL_Event* ev)
{
	int c;

	switch(ev->type)
	{
	case SDL_ACTIVEEVENT:
		if(ev->active.state & SDL_APPACTIVE)
			g_app_minimized = (ev->active.gain == 0);	// negated
		if(ev->active.state & SDL_APPINPUTFOCUS)
			g_app_has_focus = (ev->active.gain != 0);
		return IN_PASS;

	case SDL_MOUSEMOTION:
		g_mouse_x = ev->motion.x;
		g_mouse_y = ev->motion.y;
		return IN_PASS;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		c = ev->button.button;
		if(c < ARRAY_SIZE(g_mouse_buttons))
			g_mouse_buttons[c] = (ev->type == SDL_MOUSEBUTTONDOWN);
		else
			debug_warn("invalid mouse button");
		return IN_PASS;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		c = ev->key.keysym.sym;
		if(c < ARRAY_SIZE(g_keys))
			g_keys[c] = (ev->type == SDL_KEYDOWN);
		else
		{
			// don't complain: this happens when the hotkey system
			// spoofs keys (it assigns values starting from SDLK_LAST)
			//debug_warn("invalid key");
		}
		return IN_PASS;

	default:
		return IN_PASS;
	}

	UNREACHABLE;
}
