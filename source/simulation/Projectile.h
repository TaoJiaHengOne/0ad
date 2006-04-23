// Projectile.h
//
// Simple class that represents a single projectile in the simulation.
//
// Mark Thompson (mot20@cam.ac.uk / mark@wildfiregames.com)

#ifndef PROJECTILE_INCLUDED
#define PROJECTILE_INCLUDED

#include "Vector3D.h"
#include "Vector2D.h"
#include "Singleton.h"
#include "scripting/ScriptableObject.h"
#include "scripting/DOMEvent.h"
#include "ScriptObject.h"
#include "ps/Game.h"
#include "ps/World.h"

class CProjectileManager;
class CModel;
class CEntity;

class CProjectile : public CJSObject<CProjectile>, public IEventTarget
{
	friend class CProjectileManager;
	friend class CJSObject<CProjectile>;

	CModel* m_Actor;

	CVector3D m_Position;
	CVector2D m_Axis;

	CVector3D m_Position_Previous;
	CVector3D m_Position_Graphics;

	// Horizontal and vertical velocities
	float m_Speed_H;
	float m_Speed_V;

	CEntity* m_Originator;

	CScriptObject m_ImpactEventHandler;
	CScriptObject m_MissEventHandler;

	CProjectile( const CModel* Actor, const CVector3D& Position, const CVector3D& Target, float Speed, CEntity* Originator, const CScriptObject& ImpactScript, const CScriptObject& MissScript );
	~CProjectile();

	// Updates gameplay information for the specified timestep. Returns 'false' if the projectile should be removed from the world.
	bool Update( size_t timestep_millis );
	// Updates graphical information for a point timestep_millis after the previous simulation frame (and before the current one)
	void Interpolate( size_t timestep_millis );

	// Scripty things.
public:	
	static void ScriptingInit();
	static JSBool Construct( JSContext* cx, JSObject* obj, uint argc, jsval* argv, jsval* rval );
	JSObject* GetScriptExecContext( IEventTarget* UNUSED(target) ) { return( GetScript() ); }

	inline CModel* GetModel() const { return( m_Actor ); }
};

class CEventProjectileImpact : public CScriptEvent
{
	CEntity* m_Originator;
	CEntity* m_Impact;
	CVector3D m_Position;
public:
	CEventProjectileImpact( CEntity* Originator, CEntity* Impact, const CVector3D& Position );
};

class CEventProjectileMiss : public CScriptEvent
{
	CEntity* m_Originator;
	CVector3D m_Position;
public:
	CEventProjectileMiss( CEntity* Originator, const CVector3D& Position );
};

// TODO: Maybe roll this (or at least the graphics bit) into the particle system?
// Initially this had g_UnitMan managing the models and g_EntityManager holding the
// projectiles themselves. This way may be less confusing - I'm not sure.

class CProjectileManager
{
	friend class CProjectile;
public:
	CProjectileManager();
	~CProjectileManager();

	void UpdateAll( size_t timestep );
	void InterpolateAll( double frametime );

	inline const std::vector<CProjectile*>& GetProjectiles() { return m_Projectiles; }

	CProjectile* AddProjectile( const CModel* Actor, const CVector3D& Position, const CVector3D& Target, float Speed, CEntity* Originator, const CScriptObject& ImpactScript, const CScriptObject& MissScript );
	// Only if you have some reason to prematurely get rid of a projectile.
	// Under normal circumstances, it will delete itself when it hits something.
	void DeleteProjectile( CProjectile* p );
private:
	// Keep this so we can go from relative->absolute offsets in interpolate.
	size_t m_LastTurnLength;

	// Maintain a list of the projectiles in the world
	std::vector<CProjectile*> m_Projectiles;
};

#define g_ProjectileManager (*(g_Game->GetWorld()->GetProjectileManager()))

#endif
