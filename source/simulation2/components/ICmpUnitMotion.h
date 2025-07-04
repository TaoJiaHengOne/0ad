/* Copyright (C) 2022 Wildfire Games.
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

#ifndef INCLUDED_ICMPUNITMOTION
#define INCLUDED_ICMPUNITMOTION

#include "simulation2/system/Interface.h"

#include "simulation2/components/ICmpPathfinder.h" // for pass_class_t
#include "simulation2/components/ICmpPosition.h" // for entity_pos_t

/**
 * Motion interface for entities with complex movement capabilities.
 * (Simpler motion is handled by ICmpMotion instead.)
 *
 * It should eventually support different movement speeds, moving to areas
 * instead of points, moving as part of a group, moving as part of a formation,
 * etc.
 */
class ICmpUnitMotion : public IComponent
{
public:

	/**
	 * Attempt to walk into range of a to a given point, or as close as possible.
	 * The range is measured from the center of the unit.
	 * If cannot move anywhere at all, or if there is some other error, then returns false.
	 * Otherwise, returns true.
	 * If maxRange is negative, then the maximum range is treated as infinity.
	 */
	virtual bool MoveToPointRange(entity_pos_t x, entity_pos_t z, entity_pos_t minRange, entity_pos_t maxRange) = 0;

	/**
	 * Attempt to walk into range of a given target entity, or as close as possible.
	 * The range is measured between approximately the edges of the unit and the target, so that
	 * maxRange=0 is not unreachably close to the target.
	 * If the unit cannot move anywhere at all, or if there is some other error, then returns false.
	 * Otherwise, returns true.
	 * If maxRange is negative, then the maximum range is treated as infinity.
	 */
	virtual bool MoveToTargetRange(entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange) = 0;

	/**
	 * Join a formation, and move towards a given offset relative to the formation controller entity.
	 * The unit will remain 'in formation' fromthe perspective of UnitMotion
	 * until SetMemberOfFormation(INVALID_ENTITY) is passed.
	 */
	virtual void MoveToFormationOffset(entity_id_t controller, entity_pos_t x, entity_pos_t z) = 0;

	/**
	 * Set/unset the unit as a formation member.
	 * @param controller - if INVALID_ENTITY, the unit is no longer a formation member. Otherwise it is and this is the controller.
	 */
	virtual void SetMemberOfFormation(entity_id_t controller) = 0;

	/**
	 * Check if the target is reachable.
	 * Don't take this as absolute gospel since there are things that the pathfinder may not detect, such as
	 * entity obstructions in the way, but in general it should return satisfactory results.
	 * The interface is similar to MoveToTargetRange but the move is not attempted.
	 * @return true if the target is assumed reachable, false otherwise.
	 */
	virtual bool IsTargetRangeReachable(entity_id_t target, entity_pos_t minRange, entity_pos_t maxRange) = 0;

	/**
	 * Returns true if we are possibly at our destination.
	 * Since the concept of being at destination is dependent on why the move was requested,
	 * UnitMotion can only ever hint about this, hence the conditional tone.
	 */
	virtual bool PossiblyAtDestination() const = 0;

	/**
	 * Turn to look towards the given point.
	 */
	virtual void FaceTowardsPoint(entity_pos_t x, entity_pos_t z) = 0;

	/**
	 * Stop moving immediately.
	 */
	virtual void StopMoving() = 0;

	/**
	 * Get the speed at the end of the current turn.
	 */
	virtual fixed GetCurrentSpeed() const = 0;

	/**
	 * @returns true if the unit has a destination.
	 */
	virtual bool IsMoveRequested() const = 0;

	/**
	 * Get the unit template walk speed after modifications.
	 */
	virtual fixed GetWalkSpeed() const = 0;

	/**
	 * Get the unit template running (i.e. max) speed after modifications.
	 */
	virtual fixed GetRunMultiplier() const = 0;

	/**
	 * Returns the ratio of GetSpeed() / GetWalkSpeed().
	 */
	virtual fixed GetSpeedMultiplier() const = 0;

	/**
	 * Set the current movement speed.
	 * @param speed A multiplier of GetWalkSpeed().
	 */
	virtual void SetSpeedMultiplier(fixed multiplier) = 0;

	/**
	 * Get the speed at which the unit intends to move.
	 * (regardless of whether the unit is moving or not right now).
	 */
	virtual fixed GetSpeed() const = 0;

	/**
	 * @return the estimated position of the unit in @param dt seconds,
	 * following current paths. This is allowed to 'look into the future'.
	 */
	virtual CFixedVector2D EstimateFuturePosition(const fixed dt) const = 0;

	/**
	 * Get the current acceleration.
	 */
	virtual fixed GetAcceleration() const = 0;

	/**
	 * Set the current acceleration.
	 * @param acceleration The acceleration.
	 */
	virtual void SetAcceleration(fixed acceleration) = 0;

	/**
	 * Set whether the unit will turn to face the target point after finishing moving.
	 */
	virtual void SetFacePointAfterMove(bool facePointAfterMove) = 0;

	virtual bool GetFacePointAfterMove() const = 0;

	/**
	 * Get the unit's passability class.
	 */
	virtual pass_class_t GetPassabilityClass() const = 0;

	/**
	 * Get the passability class name (as defined in pathfinder.xml)
	 */
	virtual std::string GetPassabilityClassName() const = 0;

	/**
	 * Sets the passability class name (as defined in pathfinder.xml)
	 */
	virtual void SetPassabilityClassName(const std::string& passClassName) = 0;

	/**
	 * Get the unit clearance (used by the Obstruction component)
	 */
	virtual entity_pos_t GetUnitClearance() const = 0;

	/**
	 * Toggle the rendering of debug info.
	 */
	virtual void SetDebugOverlay(bool enabled) = 0;

	DECLARE_INTERFACE_TYPE(UnitMotion)
};

#endif // INCLUDED_ICMPUNITMOTION
