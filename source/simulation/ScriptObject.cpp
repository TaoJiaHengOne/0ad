#include "precompiled.h"

#include "ScriptObject.h"
#include "Entity.h"

CScriptObject::CScriptObject()
{
	Function = NULL;
}

CScriptObject::~CScriptObject()
{
	Uproot();
}

void CScriptObject::Root()
{
	if( !Function )
		return;

	FunctionObject = JS_GetFunctionObject( Function );

	JS_AddRoot( g_ScriptingHost.GetContext(), &FunctionObject );
}

void CScriptObject::Uproot()
{
	if( Function )
		JS_RemoveRoot( g_ScriptingHost.GetContext(), &FunctionObject );
}

CScriptObject::CScriptObject( JSFunction* _Function )
{
	Function = NULL;
	SetFunction( _Function );
}

CScriptObject::CScriptObject( jsval v )
{
	Function = NULL;
	SetJSVal( v );
}

CScriptObject::CScriptObject( const CScriptObject& copy )
{
	Function = NULL;
	SetFunction( copy.Function );
}

void CScriptObject::SetFunction( JSFunction* _Function )
{
	Uproot();

	Function = _Function;
	
	Root();
}

void CScriptObject::SetJSVal( jsval v )
{
	CStrW Source;
	switch( JS_TypeOfValue( g_ScriptingHost.GetContext(), v ) )
	{
	case JSTYPE_STRING:
		Source = g_ScriptingHost.ValueToUCString( v );
		Compile( L"unknown", Source );
		break;
	case JSTYPE_FUNCTION:
		SetFunction( JS_ValueToFunction( g_ScriptingHost.GetContext(), v ) );
		break;
	default:
		Function = NULL;
	}
}

JSObject* CScriptObject::GetFunctionObject()
{
	if( Function )
		return( FunctionObject );
	return( NULL );
}

// Executes a script attached to a JS object.
// Returns false if the script isn't defined, if the script can't be executed,
// otherwise true. Script return value is in rval.
bool CScriptObject::Run( JSObject* Context, jsval* rval, uintN argc, jsval* argv )
{
	if( !Function )
		return( false );
	return( JS_TRUE == JS_CallFunction( g_ScriptingHost.GetContext(), Context, Function, argc, argv, rval ) );
}

// This variant casts script return value to a boolean, and passes it back.
bool CScriptObject::Run( JSObject* Context, uintN argc, jsval* argv )
{
	jsval Temp;
	if( !Run( Context, &Temp, argc, argv ) )
		return( false );
	return( g_ScriptingHost.ValueToBool( Temp ) );
}

// Treat this as an event handler and dispatch an event to it. Return !evt->m_cancelled, as a convenience.
bool CScriptObject::DispatchEvent( JSObject* Context, CScriptEvent* evt )
{
	if( Function )
	{
		jsval Temp;
		jsval EventObject = OBJECT_TO_JSVAL( evt->GetScript() );
		JS_CallFunction( g_ScriptingHost.GetContext(), Context, Function, 1, &EventObject, &Temp );
	}
	return( evt->m_Blocked );
}

void CScriptObject::Compile( CStrW FileNameTag, CStrW FunctionBody )
{
	if( Function )
		JS_RemoveRoot( g_ScriptingHost.GetContext(), &Function );

	const char* argnames[] = { "evt" };
	utf16string str16=FunctionBody.utf16();
	Function = JS_CompileUCFunction( g_ScriptingHost.GetContext(), NULL, NULL, 1, argnames, str16.c_str(), str16.size(), (CStr)FileNameTag, 0 );
	
	Root();
}
