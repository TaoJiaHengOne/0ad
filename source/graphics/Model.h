/* Copyright (C) 2025 Wildfire Games.
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

/*
 * Mesh object with texture and skinning information
 */

#ifndef INCLUDED_MODEL
#define INCLUDED_MODEL

#include "graphics/Material.h"
#include "graphics/MeshManager.h"
#include "graphics/ModelAbstract.h"

#include <vector>

struct SPropPoint;
class CObjectEntry;
class CSkeletonAnim;
class CSkeletonAnimDef;
class CSimulation2;

// Holds world information for a particular instance of a model in the game.
class CModel : public CModelAbstract
{
	NONCOPYABLE(CModel);

public:
	struct Prop
	{
		float m_MinHeight{0.0f};
		float m_MaxHeight{0.0f};

		/**
		 * Location of the prop point within its parent model, relative to either a bone in the parent model or to the
		 * parent model's origin. See the documentation for @ref SPropPoint for more details.
		 * @see SPropPoint
		 */
		const SPropPoint* m_Point{nullptr};

		/**
		 * Pointer to the model associated with this prop. Note that the transform matrix held by this model is the full object-to-world
		 * space transform, taking into account all parent model positioning (see @ref CModel::ValidatePosition for positioning logic).
		 * @see CModel::ValidatePosition
		 */
		std::unique_ptr<CModelAbstract> m_Model;
		CObjectEntry* m_ObjectEntry{nullptr};

		bool m_Hidden{false}; ///< Should this prop be temporarily removed from rendering?
		bool m_Selectable{true}; /// < should this prop count in the selection size?
	};

public:
	CModel(const CSimulation2& simulation, const CMaterial& material, const CModelDefPtr& modeldef);
	~CModel() override;

	/// Dynamic cast
	CModel* ToCModel() override
	{
		return this;
	}

	// update this model's state; 'time' is the absolute time since the start of the animation, in MS
	void UpdateTo(float time);

	// get the model's geometry data
	const CModelDefPtr& GetModelDef() { return m_pModelDef; }

	// set the model's player ID, recursively through props
	void SetPlayerID(player_id_t id) override;
	// set the models mod color
	void SetShadingColor(const CColor& color) override;
	// get the model's material
	const CMaterial& GetMaterial() { return m_Material; }

	// set the given animation as the current animation on this model
	bool SetAnimation(CSkeletonAnim* anim, bool once = false);

	// get the currently playing animation, if any
	CSkeletonAnim* GetAnimation() const { return m_Anim; }

	// set the animation state to be the same as from another; both models should
	// be compatible types (same type of skeleton)
	void CopyAnimationFrom(CModel* source);

	// set object flags
	void SetFlags(int flags) { m_Flags=flags; }
	// get object flags
	int GetFlags() const { return m_Flags; }

	// add object flags, recursively through props
	void AddFlagsRec(int flags);
	// remove shadow casting and receiving, recursively through props
	// TODO: replace with more generic shader define + flags setting
	void RemoveShadowsCast();
	void RemoveShadowsReceive();


	void SetTerrainDirty(ssize_t i0, ssize_t j0, ssize_t i1, ssize_t j1) override
	{
		for (size_t i = 0; i < m_Props.size(); ++i)
			m_Props[i].m_Model->SetTerrainDirty(i0, j0, i1, j1);
	}

	void SetEntityVariable(const std::string& name, float value) override
	{
		for (size_t i = 0; i < m_Props.size(); ++i)
			m_Props[i].m_Model->SetEntityVariable(name, value);
	}

	// --- WORLD/OBJECT SPACE BOUNDS -----------------------------------------------------------------

	/// Overridden to calculate both the world-space and object-space bounds of this model, and stores the result in
	/// m_Bounds and m_ObjectBounds, respectively.
	void CalcBounds() override;

	/// Returns the object-space bounds for this model, excluding its children.
	const CBoundingBoxAligned& GetObjectBounds()
	{
		RecalculateBoundsIfNecessary();				// recalculates both object-space and world-space bounds if necessary
		return m_ObjectBounds;
	}

	const CBoundingBoxAligned GetWorldBoundsRec() override;		// reimplemented here

	/// Auxiliary method; calculates object space bounds of this model, based solely on vertex positions, and stores
	/// the result in m_ObjectBounds. Called by CalcBounds (instead of CalcAnimatedObjectBounds) if it has been determined
	/// that the object-space bounds are static.
	void CalcStaticObjectBounds();

	/// Auxiliary method; calculate object-space bounds encompassing all vertex positions for given animation, and stores
	/// the result in m_ObjectBounds. Called by CalcBounds (instead of CalcStaticBounds) if it has been determined that the
	/// object-space bounds need to take animations into account.
	void CalcAnimatedObjectBounds(CSkeletonAnimDef* anim,CBoundingBoxAligned& result);

	// --- SELECTION BOX/BOUNDS ----------------------------------------------------------------------

	/// Reimplemented here since proper models should participate in selection boxes.
	const CBoundingBoxAligned GetObjectSelectionBoundsRec() override;

	/**
	 * Set transform of this object.
	 *
	 * @note In order to ensure that all child props are updated properly,
	 * you must call ValidatePosition().
	 */
	void SetTransform(const CMatrix3D& transform) override;

	/**
	 * Return whether this is a skinned/skeletal model. If it is, Get*BoneMatrices()
	 * will return valid non-NULL arrays.
	 */
	bool IsSkinned() { return (m_BoneMatrices != NULL); }

	// return the models bone matrices; 16-byte aligned for SSE reads
	const CMatrix3D* GetAnimatedBoneMatrices()
	{
		ENSURE(m_PositionValid);
		return m_BoneMatrices;
	}

	/**
	 * Add a prop to the model on the given point.
	 */
	void AddProp(const SPropPoint* point, std::unique_ptr<CModelAbstract> model, CObjectEntry* objectentry, float minHeight = 0.f, float maxHeight = 0.f, bool selectable = true);

	/**
	 * Add a prop to the model on the given point, and treat it as the ammo prop.
	 * The prop will be hidden by default.
	 */
	void AddAmmoProp(const SPropPoint* point, std::unique_ptr<CModelAbstract> model, CObjectEntry* objectentry);

	/**
	 * Show the ammo prop (if any), and hide any other props on that prop point.
	 */
	void ShowAmmoProp();

	/**
	 * Hide the ammo prop (if any), and show any other props on that prop point.
	 */
	void HideAmmoProp();

	/**
	 * Find the first prop used for ammo, by this model or its own props.
	 */
	CModelAbstract* FindFirstAmmoProp();

	// return prop list
	std::vector<Prop>& GetProps() { return m_Props; }
	const std::vector<Prop>& GetProps() const { return m_Props; }

	// return a clone of this model
	std::unique_ptr<CModelAbstract> Clone() const override;

	/**
	 * Ensure that both the transformation and the bone
	 * matrices are correct for this model and all its props.
	 */
	void ValidatePosition() override;

	/**
	 * Mark this model's position and bone matrices,
	 * and all props' positions as invalid.
	 */
	void InvalidatePosition() override;

private:
	// Needed for terrain aligned props
	const CSimulation2& m_Simulation;

	// object flags
	int m_Flags{0};
	// model's material
	CMaterial m_Material;
	// pointer to the model's raw 3d data
	const CModelDefPtr m_pModelDef;
	// object space bounds of model - accounts for bounds of all possible animations
	// that can play on a model. Not always up-to-date - currently CalcBounds()
	// updates it when necessary.
	CBoundingBoxAligned m_ObjectBounds;
	// animation currently playing on this model, if any
	CSkeletonAnim* m_Anim = nullptr;
	// time (in MS) into the current animation
	float m_AnimTime{0.0f};

	/**
	 * Current state of all bones on this model; null if associated modeldef isn't skeletal.
	 * Props may attach to these bones by means of the SPropPoint::m_BoneIndex field; in this case their
	 * transformation matrix held is relative to the bone transformation (see @ref SPropPoint and
	 * @ref CModel::ValidatePosition).
	 *
	 * @see SPropPoint
	 */
	CMatrix3D* m_BoneMatrices{nullptr};
	// list of current props on model
	std::vector<Prop> m_Props;

	/**
	 * The prop point to which the ammo prop is attached, or NULL if none
	 */
	const SPropPoint* m_AmmoPropPoint{nullptr};

	/**
	 * If m_AmmoPropPoint is not NULL, then the index in m_Props of the ammo prop
	 */
	size_t m_AmmoLoadedProp{0};
};

#endif // INCLUDED_MODEL
