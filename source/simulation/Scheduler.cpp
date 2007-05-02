#include "precompiled.h"
#include "Scheduler.h"
#include "Entity.h"

size_t simulationTime;
size_t frameCount;

/*
void CScheduler::PushTime( size_t delay, const HEntity& destination, const CMessage* message )
{
	timeMessage.push( SDispatchObjectMessage( destination, simulationTime + delay, message ) );
}

void CScheduler::PushFrame( size_t delay, const HEntity& destination, const CMessage* message )
{
	frameMessage.push( SDispatchObjectMessage( destination, frameCount + delay, message ) );
}
*/

CScheduler::CScheduler()
{
	m_nextTaskId = 1;
}

int CScheduler::PushTime( size_t delay, const CStrW& fragment, JSObject* operateOn )
{
	timeScript.push( SDispatchObjectScript( m_nextTaskId, fragment, simulationTime + delay, operateOn ) );
	return m_nextTaskId++;
}

int CScheduler::PushFrame( size_t delay, const CStrW& fragment, JSObject* operateOn )
{
	frameScript.push( SDispatchObjectScript( m_nextTaskId, fragment, frameCount + delay, operateOn ) );
	return m_nextTaskId++;
}

int CScheduler::PushInterval( size_t first, size_t interval, const CStrW& fragment, JSObject* operateOn, int id )
{
	if( !id )
		id = m_nextTaskId++;
	timeScript.push( SDispatchObjectScript( id, fragment, simulationTime + first, operateOn, interval ) );
	return id++;
}

int CScheduler::PushTime( size_t delay, JSFunction* script, JSObject* operateOn )
{
	timeFunction.push( SDispatchObjectFunction( m_nextTaskId, script, simulationTime + delay, operateOn ) );
	return m_nextTaskId++;
}

int CScheduler::PushFrame( size_t delay, JSFunction* script, JSObject* operateOn )
{
	frameFunction.push( SDispatchObjectFunction( m_nextTaskId, script, frameCount + delay, operateOn ) );
	return m_nextTaskId++;
}

int CScheduler::PushInterval( size_t first, size_t interval, JSFunction* function, JSObject* operateOn, int id )
{
	if( !id )
		id = m_nextTaskId++;
	timeFunction.push( SDispatchObjectFunction( id, function, simulationTime + first, operateOn, interval ) );
	return id++;
}

void CScheduler::PushProgressTimer( CJSProgressTimer* progressTimer )
{
	progressTimers.push_back( progressTimer );
}

void CScheduler::CancelTask( int id )
{
	tasksToCancel.insert( id );
}

void CScheduler::Update(size_t simElapsed)
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

		if( tasksToCancel.find( top.id ) != tasksToCancel.end() )
		{
			tasksToCancel.erase( top.id );
			continue;
		}

		g_ScriptingHost.ExecuteScript( top.script, CStrW( L"timer" ), top.operateOn );
		if( top.isRecurrent && !m_abortInterval )
			PushInterval( top.delay, top.delay, top.script, top.operateOn, top.id );
	}
	while( !frameScript.empty() )
	{
		SDispatchObjectScript top = frameScript.top();
		if( top.deliveryTime > frameCount )
			break;
		frameScript.pop();

		if( tasksToCancel.find( top.id ) != tasksToCancel.end() )
		{
			tasksToCancel.erase( top.id );
			continue;
		}

		g_ScriptingHost.ExecuteScript( top.script, CStrW( L"timer" ), top.operateOn );
	}
	while( !timeFunction.empty() )
	{
		SDispatchObjectFunction top = timeFunction.top();
		if( top.deliveryTime > simulationTime )
			break;
		timeFunction.pop();
		m_abortInterval = false;
		
		if( tasksToCancel.find( top.id ) != tasksToCancel.end() )
		{
			tasksToCancel.erase( top.id );
			continue;
		}
		
		JS_CallFunction( g_ScriptingHost.getContext(), top.operateOn, top.function, 0, NULL, &rval ); 
		
		if( top.isRecurrent && !m_abortInterval )
			PushInterval( top.delay, top.delay, top.function, top.operateOn, top.id );
	}
	while( !frameFunction.empty() )
	{
		SDispatchObjectFunction top = frameFunction.top();
		if( top.deliveryTime > frameCount )
			break;
		frameFunction.pop();
		
		if( tasksToCancel.find( top.id ) != tasksToCancel.end() )
		{
			tasksToCancel.erase( top.id );
			continue;
		}

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
	AddProperty( L"max", &CJSProgressTimer::m_Max );
	AddProperty( L"current", &CJSProgressTimer::m_Current );
	AddProperty( L"increment", &CJSProgressTimer::m_Increment );

	CJSObject<CJSProgressTimer>::ScriptingInit( "ProgressTimer", Construct, 2 );
}

JSBool CJSProgressTimer::Construct( JSContext* cx, JSObject* UNUSED(obj), uint argc, jsval* argv, jsval* rval )
{
	debug_assert( argc >= 2 );
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

	g_Scheduler.PushProgressTimer( timer );

	*rval = OBJECT_TO_JSVAL( timer->GetScript() );
	return( JS_TRUE );
}
