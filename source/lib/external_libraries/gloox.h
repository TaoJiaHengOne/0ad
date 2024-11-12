/* Copyright (C) 2024 Wildfire Games.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Bring in gloox header+library, with compatibility fixes.
 */

#ifndef INCLUDED_GLOOX
#define INCLUDED_GLOOX

// Needs NOMINMAX or including windows.h breaks c++ standard library. Include
// our version which has extra fixes.
#include "lib/sysdep/os.h"
#if OS_WIN
#include  "lib/sysdep/os/win/win.h"
#endif

// Conflicts with mozjs
#pragma push_macro("lookup")
#undef lookup

#include <gloox/client.h>
#include <gloox/connectionlistener.h>
#include <gloox/error.h>
#include <gloox/glooxversion.h>
#include <gloox/jinglecontent.h>
#include <gloox/jingleiceudp.h>
#include <gloox/jinglesessionhandler.h>
#include <gloox/jinglesessionmanager.h>
#include <gloox/loghandler.h>
#include <gloox/message.h>
#include <gloox/mucroom.h>
#include <gloox/registration.h>
#include <gloox/registrationhandler.h>
#include <gloox/stanzaextension.h>
#include <gloox/tag.h>
#include <gloox/util.h>

#pragma pop_macro("lookup")

#endif	// #ifndef INCLUDED_GLOOX
