#include "precompiled.h"

#include "EntityFormation.h"

#include "Entity.h"
#include "FormationCollection.h"
#include "FormationManager.h"
#include "Simulation.h"
#include "ps/Game.h"
#include "ps/Interact.h"
#include "ps/Network/NetMessage.h"

CEntityFormation::CEntityFormation( CFormation*& base, size_t index )
{
	if (!base)
		return;

	m_self = m_base = base;
	m_numEntities=0;
	m_speed=0.0f;
	m_orientation = 0.0f;

	m_position.x = m_position.y = -1.0f;
	m_duplication=false;
	m_entities.resize(m_base->m_numSlots);

	m_index = (int)index;
}
CEntityFormation::~CEntityFormation()
{
	for ( int i=0; i<m_base->m_numSlots; ++i )
	{
		if ( m_entities[i] )
		{
			m_entities[i]->m_formation = -1;
			m_entities[i]->m_formationSlot = -1;
			m_entities[i] = NULL;
		}
	}
}
void CEntityFormation::SwitchBase( CFormation*& base )
{
	std::vector<CEntity*> copy;
	copy.resize( m_base->m_numSlots );
	for ( int i=0; i < m_base->m_numSlots; ++i )
	{
		if ( !m_entities[i] )
			continue;
		copy.push_back( m_entities[i] );
		RemoveUnit( m_entities[i] );
	}
	m_self = m_base = base;
	m_entities.resize(m_base->m_numSlots);
	ResetAllEntities();

	for ( std::vector<CEntity*>::iterator it=copy.begin(); it != copy.end(); it++ )
		g_FormationManager.AddUnit(*it, m_index);
}
bool CEntityFormation::AddUnit( CEntity*& entity )
{
	debug_assert( entity );
	//Add the unit to the most appropriate slot
	for (int i=0; i<m_base->m_numSlots; ++i)
	{
		if ( IsBetterUnit( i, entity ) )
		{
			if ( m_entities[i] )
			{
				CEntity* prev = m_entities[i];
				RemoveUnit( m_entities[i] );
				m_entities[i] = entity;
				AddUnit( prev );
			}

			m_entities[i] = entity;
			++m_numEntities;
			entity->m_formation = m_index;
			entity->m_formationSlot = i;

			return true;
		}
	}
	return false;
}
void CEntityFormation::RemoveUnit( CEntity*& entity )
{
	if ( !(IsValidOrder(entity->m_formationSlot) && entity) )
		return;

	m_entities[entity->m_formationSlot] = NULL;
	entity->m_formation = -1;
	entity->m_formationSlot = -1;

	--m_numEntities;
	//UpdateFormation();
}
bool CEntityFormation::IsSlotAppropriate( int order, CEntity* entity )
{
	debug_assert( entity );
	if ( !IsValidOrder(order) )
		return false;

	for ( size_t idx=0; idx < m_base->m_slots[order].category.size(); ++idx )
	{
		CStr tmp( m_base->m_slots[order].category[idx] );
		if ( entity->m_classes.IsMember( tmp ) )
			return true;
	}
	return false;
}
bool CEntityFormation::IsBetterUnit( int order, CEntity* entity )
{
	if ( !( IsValidOrder(order) && entity ) )
		return false;
	//Adding to an empty slot, check if we're elligible
	if ( !m_entities[order] )
		return IsSlotAppropriate(order, entity);

	//Go through each category and check both units' classes.  Must be done in order
	//because categories are listed in order of importance
	for ( size_t idx=0; idx < m_base->m_slots[order].category.size(); ++idx )
	{
		CStr cat = m_base->m_slots[order].category[idx];
		bool current=false;
		bool newEnt=false;

		current = m_entities[order]->m_classes.IsMember( cat );
		newEnt = entity->m_classes.IsMember( cat );

		if ( current != newEnt )
			return newEnt;
	}
	return false;
}

void CEntityFormation::UpdateFormation()
{
	//Get the entities in the right order (as in, ordered correctly and in the right order/slot)
	for ( int i=1; i<m_base->m_numSlots; ++i )
	{
		if ( !m_entities[i] )
			continue;

		//Go through slots with higher order
		for ( int j=i-1; j > 0; --j )
		{
			if ( IsBetterUnit( j, m_entities[i] ) )
			{
				CEntity* temp = m_entities[j];
				m_entities[j] = m_entities[i];
				m_entities[i] = temp;

				int tmpSlot = m_entities[i]->m_formationSlot;
				m_entities[i]->m_formationSlot = m_entities[j]->m_formationSlot;
				m_entities[j]->m_formationSlot = tmpSlot;
				--i;
			}
		}
	}
	CEntityList entities = GetEntityList();
	CNetMessage* msg = CNetMessage::CreatePositionMessage( entities, NMT_FormationGoto, m_position );
	g_Game->GetSimulation()->QueueLocalCommand(msg);

}

void CEntityFormation::ResetAllEntities()
{
	for ( int i=0; i<m_base->m_numSlots; ++i )
		m_entities[i] = NULL;
}
void CEntityFormation::ResetAngleDivs()
{
	for ( int i=0; i<m_base->m_anglePenaltyDivs; ++i )
		m_angleDivs[i] = false;
}
void CEntityFormation::SelectAllUnits()
{
	for ( int i=0; i<m_base->m_numSlots; ++i )
	{
		if ( m_entities[i] && !g_Selection.isSelected(m_entities[i]->me) )
			g_Selection.addSelection( m_entities[i]->me );
	}
}

CEntityList CEntityFormation::GetEntityList()
{
	CEntityList ret;
	for ( int i=0; i<m_base->m_numSlots; i++ )
	{
		if ( m_entities[i] )
			ret.push_back( m_entities[i]->me);
	}
	return ret;
}

CVector2D CEntityFormation::GetSlotPosition( int order )
{
	if ( IsValidOrder(order) )
		return CVector2D ( m_base->m_slots[order].rankOff, m_base->m_slots[order].fileOff );
	return CVector2D(-1, -1);
}
void CEntityFormation::BaseToMovement()
{
	CFormation* tmp = m_self;
	CFormation* move = g_EntityFormationCollection.getTemplate( m_base->m_movement );
	if (!move)
		return;
	SwitchBase( move );
	//SwitchBase resets m_self, so reset it so we can return to the correct base later
	m_self = tmp;
}
void CEntityFormation::ResetIndex( size_t index )
{
	m_index = (int)index;

	for ( size_t i=0; i< m_entities.size(); ++i )
	{
		if ( m_entities[i] )
			m_entities[i]->m_formation = m_index;
	}
}
/*
inline void CEntityFormation::SetDuplication( JSContext* UNUSED(cx), uintN UNUSED(argc), jsval* argv )
{
	m_duplication=ToPrimitive<bool>( argv[0] );
}
inline bool CEntityFormation::IsDuplication( JSContext* UNUSED(cx), uintN UNUSED(argc), jsval* UNUSED(argv))
{
	return m_duplication;
}*/
