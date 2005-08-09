#include "precompiled.h"

#include "JSInterface_Camera.h"
#include "scripting/JSInterface_Vector3D.h"
#include "Camera.h"
#include "Vector3D.h"
#include "Matrix3D.h"
#include "Terrain.h"
#include "Game.h"


JSClass JSI_Camera::JSI_class = {
	"Camera", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	JSI_Camera::getProperty, JSI_Camera::setProperty,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JSI_Camera::finalize,
	NULL, NULL, NULL, NULL
};

JSPropertySpec JSI_Camera::JSI_props[] =
{
	{ "position", JSI_Camera::vector_position, JSPROP_ENUMERATE },
	{ "orientation", JSI_Camera::vector_orientation, JSPROP_ENUMERATE },
	{ "up", JSI_Camera::vector_up, JSPROP_ENUMERATE },
	{ 0 },
};

JSFunctionSpec JSI_Camera::JSI_methods[] = 
{
    { "lookAt", JSI_Camera::lookAt, 2, 0, 0 },
	{ "getFocus", JSI_Camera::getFocus, 0, 0, 0 },
	{ 0 }
};

void JSI_Camera::init()
{
	g_ScriptingHost.DefineCustomObjectType( &JSI_class, JSI_Camera::construct, 0, JSI_props, JSI_methods, NULL, NULL );
}

JSI_Camera::Camera_Info::Camera_Info()
{
	Initialise();
}

JSI_Camera::Camera_Info::Camera_Info( const CVector3D& Position )
{
	CMatrix3D Orient;
	Orient.SetXRotation( DEGTORAD( 30 ) );
	Orient.RotateY( DEGTORAD( -45 ) );
	Orient.Translate( Position );

	Initialise( (const CMatrix3D&)Orient );
}

JSI_Camera::Camera_Info::Camera_Info( const CVector3D& Position, const CVector3D& Orientation )
{
	Initialise();
	m_Data->LookAlong( Position, Orientation, CVector3D( 0.0f, 1.0f, 0.0f ) );
}

JSI_Camera::Camera_Info::Camera_Info( const CVector3D& Position, const CVector3D& Orientation, const CVector3D& Up )
{
	Initialise();
	m_Data->LookAlong( Position, Orientation, Up );
}

JSI_Camera::Camera_Info::Camera_Info( const CMatrix3D& Orientation )
{
	Initialise( Orientation );
}

JSI_Camera::Camera_Info::Camera_Info( CCamera* Reference )
{
	m_Data = Reference;
	m_EngineOwned = true;
}

JSI_Camera::Camera_Info::~Camera_Info()
{
	if( !m_EngineOwned )
		delete( m_Data );
}

void JSI_Camera::Camera_Info::Initialise()
{
	CMatrix3D Orient;
	Orient.SetXRotation( DEGTORAD( 30 ) );
	Orient.RotateY( DEGTORAD( -45 ) );
	Orient.Translate( 100, 150, -100 );

	Initialise( (const CMatrix3D&)Orient );
}

void JSI_Camera::Camera_Info::Initialise( const CMatrix3D& Orientation )
{
	m_Data = new CCamera();
	m_EngineOwned = false;

	m_Data->LookAlong( Orientation.GetTranslation(), Orientation.GetIn(), Orientation.GetUp() );
}

void JSI_Camera::Camera_Info::Freshen()
{
	m_sv_Position = m_Data->m_Orientation.GetTranslation();
	m_sv_Orientation = m_Data->m_Orientation.GetIn();
	m_sv_Up = m_Data->m_Orientation.GetUp();
}

void JSI_Camera::Camera_Info::Update()
{
	m_Data->LookAlong( m_sv_Position, m_sv_Orientation, m_sv_Up );
}

void JSI_Camera::Camera_Info::FreshenTarget()
{
	m_sv_Target = m_Data->GetFocus();
}

JSBool JSI_Camera::getCamera( JSContext* cx, JSObject* UNUSED(obj), jsval UNUSED(id), jsval* vp )
{
	JSObject* camera = JS_NewObject( cx, &JSI_Camera::JSI_class, NULL, NULL );
	JS_SetPrivate( cx, camera, new Camera_Info( g_Game->GetView()->GetCamera() ) );
	*vp = OBJECT_TO_JSVAL( camera );
	return( JS_TRUE );
}

JSBool JSI_Camera::setCamera( JSContext* cx, JSObject* UNUSED(obj), jsval UNUSED(id), jsval* vp )
{
	JSObject* camera = JSVAL_TO_OBJECT( *vp );
	Camera_Info* cameraInfo;
	if( !JSVAL_IS_OBJECT( *vp ) || !( cameraInfo = (Camera_Info*)JS_GetInstancePrivate( cx, camera, &JSI_Camera::JSI_class, NULL ) ) )
	{
		JS_ReportError( cx, "[Camera] Invalid object" );
	}
	else
	{
		g_Game->GetView()->GetCamera()->m_Orientation = cameraInfo->m_Data->m_Orientation;
	}
	return( JS_TRUE );
}

JSBool JSI_Camera::getProperty( JSContext* cx, JSObject* obj, jsval id, jsval* vp )
{
	if( !JSVAL_IS_INT( id ) )
		return( JS_TRUE );

	Camera_Info* cameraInfo = (Camera_Info*)JS_GetPrivate( cx, obj );
	if( !cameraInfo )
	{
		JS_ReportError( cx, "[Camera] Invalid Reference" );
		return( JS_TRUE );
	}
	
	CVector3D* d;

	switch( ToPrimitive<int>( id ) )
	{
	case vector_position: d = &cameraInfo->m_sv_Position; break;
	case vector_orientation: d = &cameraInfo->m_sv_Orientation; break;
	case vector_up: d = &cameraInfo->m_sv_Up; break;
	default: return( JS_TRUE );
	}

	JSObject* vector3d = JS_NewObject( g_ScriptingHost.getContext(), &JSI_Vector3D::JSI_class, NULL, NULL );
	JS_SetPrivate( g_ScriptingHost.getContext(), vector3d, new JSI_Vector3D::Vector3D_Info( d, cameraInfo, ( void( IPropertyOwner::* )() )&Camera_Info::Update, ( void( IPropertyOwner::* )() )&Camera_Info::Freshen ) );
	*vp = OBJECT_TO_JSVAL( vector3d );
	return( JS_TRUE );
}

JSBool JSI_Camera::setProperty( JSContext* cx, JSObject* obj, jsval id, jsval* vp )
{
	if( !JSVAL_IS_INT( id ) )
		return( JS_TRUE );

	Camera_Info* cameraInfo = (Camera_Info*)JS_GetPrivate( cx, obj );
	if( !cameraInfo )
	{
		JS_ReportError( cx, "[Camera] Invalid reference" );
		return( JS_TRUE );
	}
	
	JSObject* vector3d = JSVAL_TO_OBJECT( *vp );
	JSI_Vector3D::Vector3D_Info* v = NULL;

	if( JSVAL_IS_OBJECT( *vp ) && ( v = (JSI_Vector3D::Vector3D_Info*)JS_GetInstancePrivate( g_ScriptingHost.getContext(), vector3d, &JSI_Vector3D::JSI_class, NULL ) ) )
	{
		cameraInfo->Freshen();

		switch( ToPrimitive<int>( id ) )
		{
		case vector_position: cameraInfo->m_sv_Position = *( v->vector ); break;
		case vector_orientation: cameraInfo->m_sv_Orientation = *( v->vector ); break;
		case vector_up: cameraInfo->m_sv_Up = *( v->vector ); break;
		}

		cameraInfo->Update();
	}

	return( JS_TRUE );
}

#define GETVECTOR( jv ) ( ( JSVAL_IS_OBJECT( jv ) && ( v = (JSI_Vector3D::Vector3D_Info*)JS_GetInstancePrivate( g_ScriptingHost.getContext(), JSVAL_TO_OBJECT( jv ), &JSI_Vector3D::JSI_class, NULL ) ) ) ? *(v->vector) : CVector3D() )


JSBool JSI_Camera::lookAt( JSContext* cx, JSObject* obj, uint argc, jsval* argv, jsval* rval )
{
	JSI_Vector3D::Vector3D_Info* v = NULL;
	Camera_Info* cameraInfo = (Camera_Info*)JS_GetPrivate( cx, obj );

	if( 2 <= argc && argc <= 3 )
	{
		cameraInfo->m_sv_Position = GETVECTOR( argv[0] );
		cameraInfo->m_sv_Orientation = ( GETVECTOR( argv[1] ) - GETVECTOR( argv[0] ) );
		cameraInfo->m_sv_Orientation.Normalize();
	}
	else
	{
		JS_ReportError( cx, "[Camera] lookAt: incorrect argument count" );
		*rval = JSVAL_FALSE;
		return( JS_TRUE );
	}
	
	if( argc == 2 )
	{
		cameraInfo->m_sv_Up = CVector3D( 0.0f, 1.0f, 0.0f );
	}
	else if( argc == 3 )
	{
		cameraInfo->m_sv_Up = GETVECTOR( argv[2] );
	}

	cameraInfo->Update();

	*rval = JSVAL_TRUE;
	return( JS_TRUE );
}

JSBool JSI_Camera::getFocus( JSContext* cx, JSObject* obj,
	uintN UNUSED(argc), jsval* UNUSED(argv), jsval* rval )
{
	// Largely copied from the equivalent method in CCamera

	Camera_Info* cameraInfo = (Camera_Info*)JS_GetPrivate( cx, obj );

	JSObject* vector3d = JS_NewObject( g_ScriptingHost.getContext(), &JSI_Vector3D::JSI_class, NULL, NULL );
	JS_SetPrivate( g_ScriptingHost.getContext(), vector3d, new JSI_Vector3D::Vector3D_Info( &( cameraInfo->m_sv_Target ), cameraInfo, NULL, ( void( IPropertyOwner::* )() )&Camera_Info::FreshenTarget ) );
	*rval = OBJECT_TO_JSVAL( vector3d );
	return( JS_TRUE );
}

JSBool JSI_Camera::construct( JSContext* cx, JSObject* UNUSED(obj),
	uintN argc, jsval* argv, jsval* rval )
{
	JSI_Vector3D::Vector3D_Info* v = NULL;

	JSObject* camera = JS_NewObject( cx, &JSI_Camera::JSI_class, NULL, NULL );

	if( argc == 0 )
	{
		JS_SetPrivate( cx, camera, new Camera_Info() );
	}
	else if( argc == 1 )
	{
		JS_SetPrivate( cx, camera, new Camera_Info( GETVECTOR( argv[0] ) ) );
	}
	else if( argc == 2 )
	{
		JS_SetPrivate( cx, camera, new Camera_Info( GETVECTOR( argv[0] ), GETVECTOR( argv[1] ) ) );
	}
	else if( argc == 3 )
	{
		JS_SetPrivate( cx, camera, new Camera_Info( GETVECTOR( argv[0] ), GETVECTOR( argv[1] ), GETVECTOR( argv[2] ) ) );
	}
	else
	{
		JS_ReportError( cx, "[Camera] Too many arguments to constructor" );
		*rval = JSVAL_NULL;
		return( JS_TRUE );
	}

#undef GET_VECTOR

	*rval = OBJECT_TO_JSVAL( camera );
	return( JS_TRUE );
}

void JSI_Camera::finalize( JSContext* cx, JSObject* obj )
{
	delete( (Camera_Info*)JS_GetPrivate( cx, obj ) );
}

