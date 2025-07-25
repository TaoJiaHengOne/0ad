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

#include "JSInterface_GUISize.h"

#include "ps/CStr.h"
#include "scriptinterface/ScriptInterface.h"
#include "scriptinterface/Object.h"

JSClass JSI_GUISize::JSI_class = {
	"GUISize", 0, &JSI_GUISize::JSI_classops
};

JSClassOps JSI_GUISize::JSI_classops = {
	nullptr, nullptr,
	nullptr, nullptr,
	nullptr, nullptr, nullptr,
	nullptr, JSI_GUISize::construct, nullptr
};

JSFunctionSpec JSI_GUISize::JSI_methods[] =
{
	JS_FN("toString", JSI_GUISize::toString, 0, 0),
	JS_FS_END
};

void JSI_GUISize::RegisterScriptClass(ScriptInterface& scriptInterface)
{
	scriptInterface.DefineCustomObjectType(&JSI_GUISize::JSI_class, JSI_GUISize::construct, 0, nullptr, JSI_GUISize::JSI_methods, nullptr, nullptr);
}

bool JSI_GUISize::construct(JSContext* cx, uint argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	ScriptRequest rq(cx);
	const ScriptInterface& scriptInterface = rq.GetScriptInterface();

	JS::RootedObject obj(rq.cx, scriptInterface.CreateCustomObject("GUISize"));

	if (args.length() == 8)
	{
		JS_SetProperty(rq.cx, obj, "left",		args[0]);
		JS_SetProperty(rq.cx, obj, "top",		args[1]);
		JS_SetProperty(rq.cx, obj, "right",	args[2]);
		JS_SetProperty(rq.cx, obj, "bottom",	args[3]);
		JS_SetProperty(rq.cx, obj, "rleft",	args[4]);
		JS_SetProperty(rq.cx, obj, "rtop",		args[5]);
		JS_SetProperty(rq.cx, obj, "rright",	args[6]);
		JS_SetProperty(rq.cx, obj, "rbottom",	args[7]);
	}
	else if (args.length() == 4)
	{
		JS::RootedValue zero(rq.cx, JS::NumberValue(0));
		JS_SetProperty(rq.cx, obj, "left",		args[0]);
		JS_SetProperty(rq.cx, obj, "top",		args[1]);
		JS_SetProperty(rq.cx, obj, "right",	args[2]);
		JS_SetProperty(rq.cx, obj, "bottom",	args[3]);
		JS_SetProperty(rq.cx, obj, "rleft",	zero);
		JS_SetProperty(rq.cx, obj, "rtop",		zero);
		JS_SetProperty(rq.cx, obj, "rright",	zero);
		JS_SetProperty(rq.cx, obj, "rbottom",	zero);
	}
	else
	{
		JS::RootedValue zero(rq.cx, JS::NumberValue(0));
		JS_SetProperty(rq.cx, obj, "left",		zero);
		JS_SetProperty(rq.cx, obj, "top",		zero);
		JS_SetProperty(rq.cx, obj, "right",	zero);
		JS_SetProperty(rq.cx, obj, "bottom",	zero);
		JS_SetProperty(rq.cx, obj, "rleft",	zero);
		JS_SetProperty(rq.cx, obj, "rtop",		zero);
		JS_SetProperty(rq.cx, obj, "rright",	zero);
		JS_SetProperty(rq.cx, obj, "rbottom",	zero);
	}

	args.rval().setObject(*obj);
	return true;
}

// Produces "10", "-10", "50%", "50%-10", "50%+10", etc
CStr JSI_GUISize::ToPercentString(double pix, double per)
{
	if (per == 0)
		return CStr::FromDouble(pix);

	return CStr::FromDouble(per)+"%"+(pix == 0.0 ? CStr() : pix > 0.0 ? CStr("+")+CStr::FromDouble(pix) : CStr::FromDouble(pix));
}

bool JSI_GUISize::toString(JSContext* cx, uint argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	CStr buffer;

	ScriptRequest rq(cx);
	double val, valr;

#define SIDE(side) \
	Script::GetProperty(rq, args.thisv(), #side, val); \
	Script::GetProperty(rq, args.thisv(), "r"#side, valr); \
	buffer += ToPercentString(val, valr);

	SIDE(left);
	buffer += " ";
	SIDE(top);
	buffer += " ";
	SIDE(right);
	buffer += " ";
	SIDE(bottom);
#undef SIDE

	Script::ToJSVal(rq, args.rval(), buffer);
	return true;
}
