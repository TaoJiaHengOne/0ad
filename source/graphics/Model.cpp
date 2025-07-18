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

#include "precompiled.h"

#include "Model.h"

#include "graphics/Decal.h"
#include "graphics/MeshManager.h"
#include "graphics/ModelDef.h"
#include "graphics/ObjectEntry.h"
#include "graphics/SkeletonAnim.h"
#include "graphics/SkeletonAnimDef.h"
#include "maths/BoundingBoxAligned.h"
#include "maths/Quaternion.h"
#include "lib/sysdep/rtl.h"
#include "ps/CLogger.h"
#include "ps/CStrInternStatic.h"
#include "ps/Profile.h"
#include "renderer/RenderingOptions.h"
#include "simulation2/components/ICmpTerrain.h"
#include "simulation2/components/ICmpWaterManager.h"
#include "simulation2/Simulation2.h"


CModel::CModel(const CSimulation2& simulation, const CMaterial& material, const CModelDefPtr& modeldef)
	: m_Simulation{simulation}, m_Material{material}, m_pModelDef{modeldef}
{
	const size_t numberOfBones = modeldef->GetNumBones();
	if (numberOfBones != 0)
	{
		const size_t numberOfBlends = modeldef->GetNumBlends();

		// allocate matrices for bone transformations
		// (one extra matrix is used for the special case of bind-shape relative weighting)
		m_BoneMatrices = (CMatrix3D*)rtl_AllocateAligned(sizeof(CMatrix3D) * (numberOfBones + 1 + numberOfBlends), 16);
		for (size_t i = 0; i < numberOfBones + 1 + numberOfBlends; ++i)
		{
			m_BoneMatrices[i].SetIdentity();
		}
	}

	m_PositionValid = true;
}

CModel::~CModel()
{
	rtl_FreeAligned(m_BoneMatrices);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CalcBound: calculate the world space bounds of this model
void CModel::CalcBounds()
{
	// Need to calculate the object bounds first, if that hasn't already been done
	if (! (m_Anim && m_Anim->m_AnimDef))
	{
		if (m_ObjectBounds.IsEmpty())
			CalcStaticObjectBounds();
	}
	else
	{
		if (m_Anim->m_ObjectBounds.IsEmpty())
			CalcAnimatedObjectBounds(m_Anim->m_AnimDef, m_Anim->m_ObjectBounds);
		ENSURE(! m_Anim->m_ObjectBounds.IsEmpty()); // (if this happens, it'll be recalculating the bounds every time)
		m_ObjectBounds = m_Anim->m_ObjectBounds;
	}

	// Ensure the transform is set correctly before we use it
	ValidatePosition();

	// Now transform the object-space bounds to world-space bounds
	m_ObjectBounds.Transform(GetTransform(), m_WorldBounds);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CalcObjectBounds: calculate object space bounds of this model, based solely on vertex positions
void CModel::CalcStaticObjectBounds()
{
	PROFILE2("CalcStaticObjectBounds");
	m_pModelDef->GetMaxBounds(nullptr, !(m_Flags & ModelFlag::NO_LOOP_ANIMATION), m_ObjectBounds);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CalcAnimatedObjectBound: calculate bounds encompassing all vertex positions for given animation
void CModel::CalcAnimatedObjectBounds(CSkeletonAnimDef* anim, CBoundingBoxAligned& result)
{
	PROFILE2("CalcAnimatedObjectBounds");
	m_pModelDef->GetMaxBounds(anim, !(m_Flags & ModelFlag::NO_LOOP_ANIMATION), result);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CBoundingBoxAligned CModel::GetWorldBoundsRec()
{
	CBoundingBoxAligned bounds = GetWorldBounds();
	for (size_t i = 0; i < m_Props.size(); ++i)
		bounds += m_Props[i].m_Model->GetWorldBoundsRec();
	return bounds;
}

const CBoundingBoxAligned CModel::GetObjectSelectionBoundsRec()
{
	CBoundingBoxAligned objBounds = GetObjectBounds();		// updates the (children-not-included) object-space bounds if necessary

	// now extend these bounds to include the props' selection bounds (if any)
	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		const Prop& prop = m_Props[i];
		if (prop.m_Hidden || !prop.m_Selectable)
			continue; // prop is hidden from rendering, so it also shouldn't be used for selection

		CBoundingBoxAligned propSelectionBounds = prop.m_Model->GetObjectSelectionBoundsRec();
		if (propSelectionBounds.IsEmpty())
			continue;	// submodel does not wish to participate in selection box, exclude it

		// We have the prop's bounds in its own object-space; now we need to transform them so they can be properly added
		// to the bounds in our object-space. For that, we need the transform of the prop attachment point.
		//
		// We have the prop point information; however, it's not trivial to compute its exact location in our object-space
		// since it may or may not be attached to a bone (see SPropPoint), which in turn may or may not be in the middle of
		// an animation. The bone matrices might be of interest, but they're really only meant to be used for the animation
		// system and are quite opaque to use from the outside (see @ref ValidatePosition).
		//
		// However, a nice side effect of ValidatePosition is that it also computes the absolute world-space transform of
		// our props and sets it on their respective models. In particular, @ref ValidatePosition will compute the prop's
		// world-space transform as either
		//
		// T' = T x	B x O
		// or
		// T' = T x O
		//
		// where T' is the prop's world-space transform, T is our world-space transform, O is the prop's local
		// offset/rotation matrix, and B is an optional transformation matrix of the bone the prop is attached to
		// (taking into account animation and everything).
		//
		// From this, it is clear that either O or B x O is the object-space transformation matrix of the prop. So,
		// all we need to do is apply our own inverse world-transform T^(-1) to T' to get our desired result. Luckily,
		// this is precomputed upon setting the transform matrix (see @ref SetTransform), so it is free to fetch.

		CMatrix3D propObjectTransform = prop.m_Model->GetTransform(); // T'
		propObjectTransform.Concatenate(GetInvTransform()); // T^(-1) x T'

		// Transform the prop's bounds into our object coordinate space
		CBoundingBoxAligned transformedPropSelectionBounds;
		propSelectionBounds.Transform(propObjectTransform, transformedPropSelectionBounds);

		objBounds += transformedPropSelectionBounds;
	}

	return objBounds;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update: update this model to the given time, in msec
void CModel::UpdateTo(float time)
{
	// update animation time, but don't calculate bone matrices - do that (lazily) when
	// something requests them; that saves some calculation work for offscreen models,
	// and also assures the world space, inverted bone matrices (required for normal
	// skinning) are up to date with respect to m_Transform
	m_AnimTime = time;

	// mark vertices as dirty
	SetDirty(RENDERDATA_UPDATE_VERTICES);

	// mark matrices as dirty
	InvalidatePosition();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// InvalidatePosition
void CModel::InvalidatePosition()
{
	m_PositionValid = false;

	for (size_t i = 0; i < m_Props.size(); ++i)
		m_Props[i].m_Model->InvalidatePosition();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ValidatePosition: ensure that current transform and bone matrices are both uptodate
void CModel::ValidatePosition()
{
	if (m_PositionValid)
	{
		ENSURE(!m_Parent || m_Parent->m_PositionValid);
		return;
	}

	if (m_Parent && !m_Parent->m_PositionValid)
	{
		// Make sure we don't base our calculations on
		// a parent animation state that is out of date.
		m_Parent->ValidatePosition();

		// Parent will recursively call our validation.
		ENSURE(m_PositionValid);
		return;
	}

	if (m_Anim && m_BoneMatrices)
	{
//		PROFILE( "generating bone matrices" );

		ENSURE(m_pModelDef->GetNumBones() == m_Anim->m_AnimDef->GetNumKeys());

		m_Anim->m_AnimDef->BuildBoneMatrices(m_AnimTime, m_BoneMatrices, !(m_Flags & ModelFlag::NO_LOOP_ANIMATION));
	}
	else if (m_BoneMatrices)
	{
		// Bones but no animation - probably a buggy actor forgot to set up the animation,
		// so just render it in its bind pose

		for (size_t i = 0; i < m_pModelDef->GetNumBones(); i++)
		{
			m_BoneMatrices[i].SetIdentity();
			m_BoneMatrices[i].Rotate(m_pModelDef->GetBones()[i].m_Rotation);
			m_BoneMatrices[i].Translate(m_pModelDef->GetBones()[i].m_Translation);
		}
	}

	// For CPU skinning, we precompute as much as possible so that the only
	// per-vertex work is a single matrix*vec multiplication.
	// For GPU skinning, we try to minimise CPU work by doing most computation
	// in the compute shader instead.
	const bool isGPUSkinningEnabled{g_RenderingOptions.GetGPUSkinning()};
	const bool worldSpaceBoneMatrices{!isGPUSkinningEnabled};
	const bool computeBlendMatrices{!isGPUSkinningEnabled};

	if (m_BoneMatrices && worldSpaceBoneMatrices)
	{
		// add world-space transformation to m_BoneMatrices
		const CMatrix3D transform = GetTransform();
		for (size_t i = 0; i < m_pModelDef->GetNumBones(); i++)
			m_BoneMatrices[i].Concatenate(transform);
	}

	// our own position is now valid; now we can safely update our props' positions without fearing
	// that doing so will cause a revalidation of this model (see recursion above).
	m_PositionValid = true;

	CMatrix3D translate;
	CVector3D objTranslation = m_Transform.GetTranslation();
	float objectHeight = 0.0f;

	CmpPtr<ICmpTerrain> cmpTerrain(m_Simulation, SYSTEM_ENTITY);
	if (cmpTerrain)
		objectHeight = cmpTerrain->GetExactGroundLevel(objTranslation.X, objTranslation.Z);

	// Object height is incorrect for floating objects. We use water height instead.
	if (m_Flags & ModelFlag::FLOAT_ON_WATER)
	{
		CmpPtr<ICmpWaterManager> cmpWaterManager(m_Simulation, SYSTEM_ENTITY);
		if (cmpWaterManager)
		{
			const float waterHeight = cmpWaterManager->GetExactWaterLevel(objTranslation.X, objTranslation.Z);
			if (waterHeight >= objectHeight)
				objectHeight = waterHeight;
		}
	}

	// re-position and validate all props
	for (const Prop& prop : m_Props)
	{
		CMatrix3D proptransform = prop.m_Point->m_Transform;
		if (prop.m_Point->m_BoneIndex != 0xff)
		{
			CMatrix3D boneMatrix = m_BoneMatrices[prop.m_Point->m_BoneIndex];
			if (!worldSpaceBoneMatrices)
				boneMatrix.Concatenate(GetTransform());
			proptransform.Concatenate(boneMatrix);
		}
		else
		{
			// not relative to any bone; just apply world-space transformation (i.e. relative to object-space origin)
			proptransform.Concatenate(m_Transform);
		}

		// Adjust prop height to terrain level when needed
		if (cmpTerrain && (prop.m_MaxHeight != 0.f || prop.m_MinHeight != 0.f))
		{
			const CVector3D& propTranslation = proptransform.GetTranslation();
			const float propTerrain = cmpTerrain->GetExactGroundLevel(propTranslation.X, propTranslation.Z);
			const float translateHeight = std::min(prop.m_MaxHeight, std::max(prop.m_MinHeight, propTerrain - objectHeight));
			translate.SetTranslation(0.f, translateHeight, 0.f);
			proptransform.Concatenate(translate);
		}

		prop.m_Model->SetTransform(proptransform);
		prop.m_Model->ValidatePosition();
	}

	if (m_BoneMatrices)
	{
		for (size_t i = 0; i < m_pModelDef->GetNumBones(); i++)
		{
			m_BoneMatrices[i] = m_BoneMatrices[i] * m_pModelDef->GetInverseBindBoneMatrices()[i];
		}

		// Note: there is a special case of joint influence, in which the vertex
		//	is influenced by the bind-shape transform instead of a particular bone,
		//	which we indicate with the blending bone ID set to the total number
		//	of bones. But since we're skinning in world space, we use the model's
		//	world space transform and store that matrix in this special index.
		//	(see https://gitea.wildfiregames.com/0ad/0ad/issues/1012)
		m_BoneMatrices[m_pModelDef->GetNumBones()] = m_Transform;

		if (computeBlendMatrices)
			m_pModelDef->BlendBoneMatrices(m_BoneMatrices);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SetAnimation: set the given animation as the current animation on this model;
// return false on error, else true
bool CModel::SetAnimation(CSkeletonAnim* anim, bool once)
{
	m_Anim = nullptr; // in case something fails

	if (anim)
	{
		m_Flags &= ~ModelFlag::NO_LOOP_ANIMATION;

		if (once)
			m_Flags |= ModelFlag::NO_LOOP_ANIMATION;

		// Not rigged or animation is not valid.
		if (!m_BoneMatrices || !anim->m_AnimDef)
			return false;

		if (anim->m_AnimDef->GetNumKeys() != m_pModelDef->GetNumBones())
		{
			LOGERROR("Mismatch between model's skeleton and animation's skeleton (%s.dae has %lu model bones while the animation %s has %lu animation keys.)",
				m_pModelDef->GetName().string8().c_str() ,
				static_cast<unsigned long>(m_pModelDef->GetNumBones()),
				anim->m_Name.c_str(),
				static_cast<unsigned long>(anim->m_AnimDef->GetNumKeys()));
			return false;
		}

		// Reset the cached bounds when the animation is changed.
		m_ObjectBounds.SetEmpty();
		InvalidateBounds();

		// Start anim from beginning.
		m_AnimTime = 0;
	}

	m_Anim = anim;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CopyAnimation
void CModel::CopyAnimationFrom(CModel* source)
{
	m_Anim = source->m_Anim;
	m_AnimTime = source->m_AnimTime;

	m_ObjectBounds.SetEmpty();
	InvalidateBounds();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AddProp: add a prop to the model on the given point
void CModel::AddProp(const SPropPoint* point, std::unique_ptr<CModelAbstract> model, CObjectEntry* objectentry, float minHeight, float maxHeight, bool selectable)
{
	// position model according to prop point position

	// this next call will invalidate the bounds of "model", which will in turn also invalidate the selection box
	model->SetTransform(point->m_Transform);
	model->m_Parent = this;

	Prop prop;
	prop.m_Point = point;
	prop.m_Model = std::move(model);
	prop.m_ObjectEntry = objectentry;
	prop.m_MinHeight = minHeight;
	prop.m_MaxHeight = maxHeight;
	prop.m_Selectable = selectable;
	m_Props.push_back(std::move(prop));
}

void CModel::AddAmmoProp(const SPropPoint* point, std::unique_ptr<CModelAbstract> model, CObjectEntry* objectentry)
{
	AddProp(point, std::move(model), objectentry);
	m_AmmoPropPoint = point;
	m_AmmoLoadedProp = m_Props.size() - 1;
	m_Props[m_AmmoLoadedProp].m_Hidden = true;

	// we only need to invalidate the selection box here if it is based on props and their visibilities
	if (!m_CustomSelectionShape)
		m_SelectionBoxValid = false;
}

void CModel::ShowAmmoProp()
{
	if (m_AmmoPropPoint == NULL)
		return;

	// Show the ammo prop, hide all others on the same prop point
	for (size_t i = 0; i < m_Props.size(); ++i)
		if (m_Props[i].m_Point == m_AmmoPropPoint)
			m_Props[i].m_Hidden = (i != m_AmmoLoadedProp);

	//  we only need to invalidate the selection box here if it is based on props and their visibilities
	if (!m_CustomSelectionShape)
		m_SelectionBoxValid = false;
}

void CModel::HideAmmoProp()
{
	if (m_AmmoPropPoint == NULL)
		return;

	// Hide the ammo prop, show all others on the same prop point
	for (size_t i = 0; i < m_Props.size(); ++i)
		if (m_Props[i].m_Point == m_AmmoPropPoint)
			m_Props[i].m_Hidden = (i == m_AmmoLoadedProp);

	//  we only need to invalidate here if the selection box is based on props and their visibilities
	if (!m_CustomSelectionShape)
		m_SelectionBoxValid = false;
}

CModelAbstract* CModel::FindFirstAmmoProp()
{
	if (m_AmmoPropPoint)
		return m_Props[m_AmmoLoadedProp].m_Model.get();

	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		CModel* propModel = m_Props[i].m_Model->ToCModel();
		if (propModel)
		{
			CModelAbstract* model = propModel->FindFirstAmmoProp();
			if (model)
				return model;
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clone: return a clone of this model
std::unique_ptr<CModelAbstract> CModel::Clone() const
{
	std::unique_ptr<CModel> clone = std::make_unique<CModel>(m_Simulation, m_Material, m_pModelDef);
	clone->m_ObjectBounds = m_ObjectBounds;
	clone->SetAnimation(m_Anim);
	clone->SetFlags(m_Flags);

	for (size_t i = 0; i < m_Props.size(); i++)
	{
		// eek!  TODO, RC - need to investigate shallow clone here
		if (m_AmmoPropPoint && i == m_AmmoLoadedProp)
			clone->AddAmmoProp(m_Props[i].m_Point, m_Props[i].m_Model->Clone(), m_Props[i].m_ObjectEntry);
		else
			clone->AddProp(m_Props[i].m_Point, m_Props[i].m_Model->Clone(), m_Props[i].m_ObjectEntry, m_Props[i].m_MinHeight, m_Props[i].m_MaxHeight, m_Props[i].m_Selectable);
	}

	return clone;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SetTransform: set the transform on this object, and reorientate props accordingly
void CModel::SetTransform(const CMatrix3D& transform)
{
	// call base class to set transform on this object
	CRenderableObject::SetTransform(transform);
	InvalidatePosition();
}

//////////////////////////////////////////////////////////////////////////

void CModel::AddFlagsRec(int flags)
{
	m_Flags |= flags;

	if (flags & ModelFlag::IGNORE_LOS)
		m_Material.AddShaderDefine(str_IGNORE_LOS, str_1);

	for (size_t i = 0; i < m_Props.size(); ++i)
		if (m_Props[i].m_Model->ToCModel())
			m_Props[i].m_Model->ToCModel()->AddFlagsRec(flags);
}

void CModel::RemoveShadowsCast()
{
	m_Flags &= ~ModelFlag::CAST_SHADOWS;
	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		if (m_Props[i].m_Model->ToCModel())
			m_Props[i].m_Model->ToCModel()->RemoveShadowsCast();
	}
}

void CModel::RemoveShadowsReceive()
{
	m_Material.AddShaderDefine(str_DISABLE_RECEIVE_SHADOWS, str_1);
	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		if (m_Props[i].m_Model->ToCModel())
			m_Props[i].m_Model->ToCModel()->RemoveShadowsReceive();
		else if (m_Props[i].m_Model->ToCModelDecal())
			m_Props[i].m_Model->ToCModelDecal()->RemoveShadowsReceive();
	}
}

void CModel::SetPlayerID(player_id_t id)
{
	CModelAbstract::SetPlayerID(id);

	for (std::vector<Prop>::iterator it = m_Props.begin(); it != m_Props.end(); ++it)
		it->m_Model->SetPlayerID(id);
}

void CModel::SetShadingColor(const CColor& color)
{
	CModelAbstract::SetShadingColor(color);

	for (std::vector<Prop>::iterator it = m_Props.begin(); it != m_Props.end(); ++it)
		it->m_Model->SetShadingColor(color);
}
