// $Id: JSInterface_GUITypes.cpp,v 1.6 2004/09/03 14:12:43 philip Exp $

#include "precompiled.h"

#include "JSInterface_GUITypes.h"
#include "CStr.h"

/**** GUISize ****/

JSClass JSI_GUISize::JSI_class = {
	"GUISize", 0,
		JS_PropertyStub, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub,
		JS_EnumerateStub, JS_ResolveStub,
		JS_ConvertStub, JS_FinalizeStub,
		NULL, NULL, NULL, NULL
};

JSPropertySpec JSI_GUISize::JSI_props[] = 
{
	{ "left",		0,	JSPROP_ENUMERATE},
	{ "top",		1,	JSPROP_ENUMERATE},
	{ "right",		2,	JSPROP_ENUMERATE},
	{ "bottom",		3,	JSPROP_ENUMERATE},
	{ "rleft",		4,	JSPROP_ENUMERATE},
	{ "rtop",		5,	JSPROP_ENUMERATE},
	{ "rright",		6,	JSPROP_ENUMERATE},
	{ "rbottom",	7,	JSPROP_ENUMERATE},
	{ 0 }
};

JSFunctionSpec JSI_GUISize::JSI_methods[] = 
{
	{ "toString", JSI_GUISize::toString, 0, 0, 0 },
	{ 0 }
};

JSBool JSI_GUISize::construct(JSContext* cx, JSObject* obj, unsigned int argc, jsval* argv, jsval* UNUSEDPARAM(rval))
{
	if (argc == 8)
	{
		JS_SetProperty(cx, obj, "left",		&argv[0]);
		JS_SetProperty(cx, obj, "top",		&argv[1]);
		JS_SetProperty(cx, obj, "right",	&argv[2]);
		JS_SetProperty(cx, obj, "bottom",	&argv[3]);
		JS_SetProperty(cx, obj, "rleft",	&argv[4]);
		JS_SetProperty(cx, obj, "rtop",		&argv[5]);
		JS_SetProperty(cx, obj, "rright",	&argv[6]);
		JS_SetProperty(cx, obj, "rbottom",	&argv[7]);
	}
	else if (argc == 4)
	{
		jsval zero = JSVAL_ZERO;
		JS_SetProperty(cx, obj, "left",		&argv[0]);
		JS_SetProperty(cx, obj, "top",		&argv[1]);
		JS_SetProperty(cx, obj, "right",	&argv[2]);
		JS_SetProperty(cx, obj, "bottom",	&argv[3]);
		JS_SetProperty(cx, obj, "rleft",	&zero);
		JS_SetProperty(cx, obj, "rtop",		&zero);
		JS_SetProperty(cx, obj, "rright",	&zero);
		JS_SetProperty(cx, obj, "rbottom",	&zero);
	}
	else
	{
		jsval zero = JSVAL_ZERO;
		JS_SetProperty(cx, obj, "left",		&zero);
		JS_SetProperty(cx, obj, "top",		&zero);
		JS_SetProperty(cx, obj, "right",	&zero);
		JS_SetProperty(cx, obj, "bottom",	&zero);
		JS_SetProperty(cx, obj, "rleft",	&zero);
		JS_SetProperty(cx, obj, "rtop",		&zero);
		JS_SetProperty(cx, obj, "rright",	&zero);
		JS_SetProperty(cx, obj, "rbottom",	&zero);
	}

	return JS_TRUE;
}

// Produces "10", "-10", "50%", "50%-10", "50%+10", etc
CStr ToPercentString(double pix, double per)
{
	if (per == 0)
		return CStr(pix);
	else
		return CStr(per)+CStr("%")+( pix == 0.0 ? CStr() : pix > 0.0 ? CStr("+")+CStr(pix) : CStr(pix) );
}

JSBool JSI_GUISize::toString(JSContext* cx, JSObject* obj, uintN UNUSEDPARAM(argc), jsval* UNUSEDPARAM(argv), jsval* rval)
{
	CStr buffer;

	try
	{
#define SIDE(side) buffer += ToPercentString(g_ScriptingHost.GetObjectProperty_Double(obj, #side), g_ScriptingHost.GetObjectProperty_Double(obj, "r"#side));
		SIDE(left);
		buffer += " ";
		SIDE(top);
		buffer += " ";
		SIDE(right);
		buffer += " ";
		SIDE(bottom);
#undef SIDE
	}
	catch (PSERROR_Scripting_ConversionFailed)
	{
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "<Error converting value to numbers>"));
		return JS_TRUE;
	}

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buffer.c_str()));
	return JS_TRUE;
}


/**** GUIColor ****/


JSClass JSI_GUIColor::JSI_class = {
	"GUIColor", 0,
		JS_PropertyStub, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub,
		JS_EnumerateStub, JS_ResolveStub,
		JS_ConvertStub, JS_FinalizeStub,
		NULL, NULL, NULL, NULL
};

JSPropertySpec JSI_GUIColor::JSI_props[] = 
{
	{ "r",	0,	JSPROP_ENUMERATE},
	{ "g",	1,	JSPROP_ENUMERATE},
	{ "b",	2,	JSPROP_ENUMERATE},
	{ "a",	3,	JSPROP_ENUMERATE},
	{ 0 }
};

JSFunctionSpec JSI_GUIColor::JSI_methods[] = 
{
	{ "toString", JSI_GUIColor::toString, 0, 0, 0 },
	{ 0 }
};

JSBool JSI_GUIColor::construct(JSContext* cx, JSObject* obj, unsigned int argc, jsval* argv, jsval* UNUSEDPARAM(rval))
{
	if (argc == 4)
	{
		JS_SetProperty(cx, obj, "r", &argv[0]);
		JS_SetProperty(cx, obj, "g", &argv[1]);
		JS_SetProperty(cx, obj, "b", &argv[2]);
		JS_SetProperty(cx, obj, "a", &argv[3]);
	}
	else
	{
		// Nice magenta:
		jsval r = DOUBLE_TO_JSVAL(JS_NewDouble(cx, 1.0));
		JS_SetProperty(cx, obj, "r", &r);
		jsval g = DOUBLE_TO_JSVAL(JS_NewDouble(cx, 0.0));
		JS_SetProperty(cx, obj, "g", &g);
		jsval b = DOUBLE_TO_JSVAL(JS_NewDouble(cx, 1.0));
		JS_SetProperty(cx, obj, "b", &b);
		jsval a = DOUBLE_TO_JSVAL(JS_NewDouble(cx, 1.0));
		JS_SetProperty(cx, obj, "a", &a);
	}
	return JS_TRUE;
}

JSBool JSI_GUIColor::toString(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	char buffer[256];
	// Convert to integers, to be compatible with the GUI's string SetSetting
	snprintf(buffer, 256, "%i %i %i %i",
		(int)( 255.0 * *JSVAL_TO_DOUBLE(g_ScriptingHost.GetObjectProperty(obj, "r")) ),
		(int)( 255.0 * *JSVAL_TO_DOUBLE(g_ScriptingHost.GetObjectProperty(obj, "g")) ),
		(int)( 255.0 * *JSVAL_TO_DOUBLE(g_ScriptingHost.GetObjectProperty(obj, "b")) ),
		(int)( 255.0 * *JSVAL_TO_DOUBLE(g_ScriptingHost.GetObjectProperty(obj, "a")) ));
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buffer));
	return JS_TRUE;
}

/**** GUIMouse ****/


JSClass JSI_GUIMouse::JSI_class = {
	"GUIMouse", 0,
		JS_PropertyStub, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub,
		JS_EnumerateStub, JS_ResolveStub,
		JS_ConvertStub, JS_FinalizeStub,
		NULL, NULL, NULL, NULL
};

JSPropertySpec JSI_GUIMouse::JSI_props[] = 
{
	{ "x",			0,	JSPROP_ENUMERATE},
	{ "y",			1,	JSPROP_ENUMERATE},
	{ "buttons",	2,	JSPROP_ENUMERATE},
	{ 0 }
};

JSFunctionSpec JSI_GUIMouse::JSI_methods[] = 
{
	{ "toString", JSI_GUIMouse::toString, 0, 0, 0 },
	{ 0 }
};

JSBool JSI_GUIMouse::construct(JSContext* cx, JSObject* obj, unsigned int argc, jsval* argv, jsval* rval)
{
	if (argc == 3)
	{
		JS_SetProperty(cx, obj, "x", &argv[0]);
		JS_SetProperty(cx, obj, "y", &argv[1]);
		JS_SetProperty(cx, obj, "buttons", &argv[2]);
	}
	else
	{
		jsval zero = JSVAL_ZERO;
		JS_SetProperty(cx, obj, "x", &zero);
		JS_SetProperty(cx, obj, "y", &zero);
		JS_SetProperty(cx, obj, "buttons", &zero);
	}
	return JS_TRUE;
}

JSBool JSI_GUIMouse::toString(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	char buffer[256];
	snprintf(buffer, 256, "%i %i %i",
		JSVAL_TO_INT(g_ScriptingHost.GetObjectProperty(obj, "x")),
		JSVAL_TO_INT(g_ScriptingHost.GetObjectProperty(obj, "y")),
		JSVAL_TO_INT(g_ScriptingHost.GetObjectProperty(obj, "buttons")) );
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buffer));
	return JS_TRUE;
}


// Initialise all the types at once:
void JSI_GUITypes::init()
{
	g_ScriptingHost.DefineCustomObjectType(&JSI_GUISize::JSI_class,  JSI_GUISize::construct,  1, JSI_GUISize::JSI_props,  JSI_GUISize::JSI_methods,  NULL, NULL);
	g_ScriptingHost.DefineCustomObjectType(&JSI_GUIColor::JSI_class, JSI_GUIColor::construct, 1, JSI_GUIColor::JSI_props, JSI_GUIColor::JSI_methods, NULL, NULL);
	g_ScriptingHost.DefineCustomObjectType(&JSI_GUIMouse::JSI_class, JSI_GUIMouse::construct, 1, JSI_GUIMouse::JSI_props, JSI_GUIMouse::JSI_methods, NULL, NULL);
}
