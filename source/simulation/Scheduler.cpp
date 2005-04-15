#include "precompiled.h"
#include "Scheduler.h"
#include "Entity.h"

size_t simulationTime;
size_t frameCount;

/*
void CScheduler::pushTime( size_t delay, const HEntity& destination, const CMessage* message )
{
	timeMessage.push( SDispatchObjectMessage( destination, simulationTime + delay, message ) );
}

void CScheduler::pushFrame( size_t delay, const HEntity& destination, const CMessage* message )
{
	frameMessage.push( SDispatchObjectMessage( destination, frameCount + delay, message ) );
}
*/

void CScheduler::pushTime( size_t delay, const CStrW& fragment, JSObject* operateOn )
{
	timeScript.push( SDispatchObjectScript( fragment, simulationTime + delay, operateOn ) );
}

void CScheduler::pushFrame( size_t delay, const CStrW& fragment, JSObject* operateOn )
{
	frameScript.push( SDispatchObjectScript( fragment, frameCount + delay, operateOn ) );
}

void CScheduler::pushInterval( size_t first, size_t interval, const CStrW& fragment, JSObject* operateOn )
{
	timeScript.push( SDispatchObjectScript( fragment, simulationTime + first, operateOn, interval ) );
}

void CScheduler::pushTime( size_t delay, JSFunction* script, JSObject* operateOn )
{
	timeFunction.push( SDispatchObjectFunction( script, simulationTime + delay, operateOn ) );
}

void CScheduler::pushFrame( size_t delay, JSFunction* script, JSObject* operateOn )
{
	frameFunction.push( SDispatchObjectFunction( script, frameCount + delay, operateOn ) );
}

void CScheduler::pushInterval( size_t first, size_t interval, JSFunction* function, JSObject* operateOn )
{
	timeFunction.push( SDispatchObjectFunction( function, simulationTime + first, operateOn, interval ) );
}

void CScheduler::pushProgressTimer( CJSProgressTimer* progressTimer )
{
	progressTimers.push_back( progressTimer );
}

void CScheduler::update(size_t simElapsed)
{
	simulationTime += simElapsed;
    frameCount++;
	jsval rval;

	while( !timeScript.empty() )
	{
		SDispatchObjectScript top = timeScript.top();
		if( top.deliveryTime > simulationTime )
			break;
		timeScript.pop();
		m_abortInterval = false;
		g_ScriptingHost.ExecuteScript( top.script, CStrW( L"timer" ), top.operateOn );
		if( top.isRecurrent && !m_abortInterval )
			pushInterval( top.delay, top.delay, top.script, top.operateOn );
	}
	while( !frameScript.empty() )
	{
		SDispatchObjectScript top = frameScript.top();
		if( top.deliveryTime > frameCount )
			break;
		frameScript.pop();
		g_ScriptingHost.ExecuteScript( top.script, CStrW( L"timer" ), top.operateOn );
	}
	while( !timeFunction.empty() )
	{
		SDispatchObjectFunction top = timeFunction.top();
		if( top.deliveryTime > simulationTime )
			break;
		timeFunction.pop();
		m_abortInterval = false;
		
		JS_CallFunction( g_ScriptingHost.getContext(), top.operateOn, top.function, 0, NULL, &rval ); 
		
		if( top.isRecurrent && !m_abortInterval )
			pushInterval( top.delay, top.delay, top.function, top.operateOn );
	}
	while( !frameFunction.empty() )
	{
		SDispatchObjectFunction top = frameFunction.top();
		if( top.deliveryTime > frameCount )
			break;
		frameFunction.pop();
		
		JS_CallFunction( g_ScriptingHost.getContext(), top.operateOn, top.function, 0, NULL, &rval ); 
	}

	std::list<CJSProgressTimer*>::iterator it;
	for( it = progressTimers.begin(); it != progressTimers.end(); it++ )
	{
		(*it)->m_Current += (*it)->m_Increment * simElapsed;
		if( (*it)->m_Current >= (*it)->m_Max )
		{
			(*it)->m_Current = (*it)->m_Max;
			if( (*it)->m_Callback )
				JS_CallFunction( g_ScriptingHost.GetContext(), (*it)->m_OperateOn, (*it)->m_Callback, 0, NULL, &rval ); 
			it = progressTimers.erase( it );
		}
	}
}

CJSProgressTimer::CJSProgressTimer( double Max, double Increment, JSFunction* Callback, JSObject* OperateOn )
{
	m_Max = Max; m_Increment = Increment; m_Callback = Callback; m_OperateOn = OperateOn; m_Current = 0.0;
}

void CJSProgressTimer::ScriptingInit()
{
	AddClassProperty( L"max", &CJSProgressTimer::m_Max );
	AddClassProperty( L"current", &CJSProgressTimer::m_Current );
	AddClassProperty( L"increment", &CJSProgressTimer::m_Increment );

	CJSObject<CJSProgressTimer>::ScriptingInit( "ProgressTimer", Construct, 2 );
}

JSBool CJSProgressTimer::Construct( JSContext* cx, JSObject* obj, unsigned int argc, jsval* argv, jsval* rval )
{
	assert( argc >= 2 );
	double max = ToPrimitive<double>( argv[0] );
	double increment = ToPrimitive<double>( argv[1] );
	JSFunction* callback_fn = NULL;
	JSObject* scope_obj = NULL;
	if( argc >= 3 )
	{
		callback_fn = JS_ValueToFunction( cx, argv[2] );
		if( ( argc >= 4 ) && JSVAL_IS_OBJECT( argv[3] ) )
		{
			scope_obj = JSVAL_TO_OBJECT( argv[3] );
		}
		else
		{
			// Attempt to determine object to operate on automatically.

			// SpiderMonkey docs say the 'this' parameter of the calling
			// function is in argv[-2]. Do I believe them? One way to find out.
			scope_obj = JSVAL_TO_OBJECT( argv[-2] );
		}
	}
	CJSProgressTimer* timer = new CJSProgressTimer( max, increment, callback_fn, scope_obj );
	timer->m_EngineOwned = false;

	g_Scheduler.pushProgressTimer( timer );

	*rval = OBJECT_TO_JSVAL( timer->GetScript() );
	return( JS_TRUE );
}