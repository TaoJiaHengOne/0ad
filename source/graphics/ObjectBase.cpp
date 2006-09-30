#include "precompiled.h"

#include <algorithm>
#include <queue>

#include "ObjectBase.h"

#include "ObjectManager.h"
#include "ps/XML/Xeromyces.h"
#include "ps/CLogger.h"
#include "lib/timer.h"

#define LOG_CATEGORY "graphics"

CObjectBase::CObjectBase()
{
	m_Properties.m_CastShadows = true;
	m_Properties.m_AutoFlatten = false;
	m_Properties.m_FloatOnWater = false;
}

bool CObjectBase::Load(const char* filename)
{
	m_VariantGroups.clear();

	CStr filePath ("art/actors/");
	filePath += filename;

	CXeromyces XeroFile;
	if (XeroFile.Load(filePath) != PSRETURN_OK)
		return false;

	m_Name = filename;

	// Use the filename for the model's name
	m_ShortName = CStr(filename).AfterLast("/").BeforeLast(".xml");

	// Define all the elements used in the XML file
	#define EL(x) int el_##x = XeroFile.getElementID(#x)
	#define AT(x) int at_##x = XeroFile.getAttributeID(#x)
	EL(actor);
	EL(castshadow);
	EL(float);
	EL(material);
	EL(group);
	EL(variant);
	EL(animations);
	EL(animation);
	EL(props);
	EL(prop);
	EL(mesh);
	EL(texture);
	EL(colour);
	AT(file);
	AT(name);
	AT(speed);
	AT(event);
	AT(load);
	AT(attachpoint);
	AT(actor);
	AT(frequency);
	#undef AT
	#undef EL

	XMBElement root = XeroFile.getRoot();

	if (root.getNodeName() != el_actor)
	{
		LOG(ERROR, LOG_CATEGORY, "Invalid actor format (unrecognised root element '%s')", XeroFile.getElementString(root.getNodeName()).c_str());
		return false;
	}


	// Set up the vector<vector<T>> m_Variants to contain the right number
	// of elements, to avoid wasteful copying/reallocation later.
	{
		// Count the variants in each group
		std::vector<int> variantGroupSizes;
		XERO_ITER_EL(root, child)
		{
			if (child.getNodeName() == el_group)
			{
				variantGroupSizes.push_back(child.getChildNodes().Count);
			}
		}

		m_VariantGroups.resize(variantGroupSizes.size());
		// Set each vector to match the number of variants
		for (size_t i = 0; i < variantGroupSizes.size(); ++i)
			m_VariantGroups[i].resize(variantGroupSizes[i]);
	}


	// (This XML-reading code is rather worryingly verbose...)

	std::vector<std::vector<Variant> >::iterator currentGroup = m_VariantGroups.begin();

	XERO_ITER_EL(root, child)
	{
		int child_name = child.getNodeName();

		if (child_name == el_group)
		{
			std::vector<Variant>::iterator currentVariant = currentGroup->begin();
			XERO_ITER_EL(child, variant)
			{
				debug_assert(variant.getNodeName() == el_variant);
				XERO_ITER_ATTR(variant, attr)
				{
					if (attr.Name == at_name)
						currentVariant->m_VariantName = CStr(attr.Value).LowerCase();

					else if (attr.Name == at_frequency)
						currentVariant->m_Frequency = CStr(attr.Value).ToInt();
				}


				XERO_ITER_EL(variant, option)
				{
					int option_name = option.getNodeName();

					if (option_name == el_mesh)
						currentVariant->m_ModelFilename = "art/meshes/" + CStr(option.getText());

					else if (option_name == el_texture)
						currentVariant->m_TextureFilename = "art/textures/skins/" + CStr(option.getText());

					else if (option_name == el_colour)
						currentVariant->m_Color = option.getText();

					else if (option_name == el_animations)
					{
						XERO_ITER_EL(option, anim_element)
						{
							debug_assert(anim_element.getNodeName() == el_animation);

							Anim anim;
							XERO_ITER_ATTR(anim_element, ae)
							{
								if (ae.Name == at_name)
								{
									anim.m_AnimName = ae.Value;
								}
								else if (ae.Name == at_file)
								{
									anim.m_FileName = "art/animation/" + CStr(ae.Value);
								}
								else if (ae.Name == at_speed)
								{
									anim.m_Speed = CStr(ae.Value).ToInt() / 100.f;
									if (anim.m_Speed <= 0.0) anim.m_Speed = 1.0f;
								}
								else if (ae.Name == at_event)
								{
									anim.m_ActionPos = CStr(ae.Value).ToDouble();
									if (anim.m_ActionPos < 0.0) anim.m_ActionPos = 0.0;
									else if (anim.m_ActionPos > 100.0) anim.m_ActionPos = 1.0;
									else if (anim.m_ActionPos > 1.0) anim.m_ActionPos /= 100.0;
								}
								else if (ae.Name == at_load)
								{
									anim.m_ActionPos2 = CStr(ae.Value).ToDouble();
									if (anim.m_ActionPos2 < 0.0) anim.m_ActionPos2 = 0.0;
									else if (anim.m_ActionPos2 > 100.0) anim.m_ActionPos2 = 1.0;
									else if (anim.m_ActionPos2 > 1.0) anim.m_ActionPos2 /= 100.0;
								}
								else
									; // unrecognised element
							}
							currentVariant->m_Anims.push_back(anim);
						}

					}
					else if (option_name == el_props)
					{
						XERO_ITER_EL(option, prop_element)
						{
							debug_assert(prop_element.getNodeName() == el_prop);

							Prop prop;
							XERO_ITER_ATTR(prop_element, pe)
							{
								if (pe.Name == at_attachpoint)
									prop.m_PropPointName = pe.Value;
								else if (pe.Name == at_actor)
									prop.m_ModelName = pe.Value;
								else
									; // unrecognised element
							}
							currentVariant->m_Props.push_back(prop);
						}
					}
					else
						; // unrecognised element
				}

				++currentVariant;
			}

			if (currentGroup->size() == 0)
			{
				LOG(ERROR, LOG_CATEGORY, "Actor group has zero variants ('%s')", filename);
			}

			++currentGroup;
		}
		else if (child_name == el_castshadow)
		{
			m_Properties.m_CastShadows = true; // TODO: this is the default, so it's a bit useless
		}
		else if (child_name == el_float)
		{
			m_Properties.m_FloatOnWater = true;
		}
		else if (child_name == el_material)
		{
			m_Material = "art/materials/" + CStr(child.getText());
		}
		else
			; // unrecognised element

		// TODO: castshadow, etc
	}

	return true;
}

std::vector<u8> CObjectBase::CalculateVariationKey(const std::vector<std::set<CStr> >& selections)
{
	// (TODO: see CObjectManager::FindObjectVariation for an opportunity to
	// call this function a bit less frequently)

	// Calculate a complete list of choices, one per group, based on the
	// supposedly-complete selections (i.e. not making random choices at this
	// stage).
	// In each group, if one of the variants has a name matching a string in the
	// first 'selections', set use that one.
	// Otherwise, try with the next (lower priority) selections set, and repeat.
	// Otherwise, choose the first variant (arbitrarily).

	std::vector<u8> choices;

	std::map<CStr, CStr> chosenProps;

	for (std::vector<std::vector<CObjectBase::Variant> >::iterator grp = m_VariantGroups.begin();
		grp != m_VariantGroups.end();
		++grp)
	{
		// Ignore groups with nothing inside. (A warning will have been
		// emitted by the loading code.)
		if (grp->size() == 0)
			continue;

		int match = -1; // -1 => none found yet

		// If there's only a single variant, choose that one
		if (grp->size() == 1)
		{
			match = 0;
		}
		else
		{
			// Determine the first variant that matches the provided strings,
			// starting with the highest priority selections set:

			for (std::vector<std::set<CStr> >::const_iterator selset = selections.begin(); selset < selections.end(); ++selset)
			{
				debug_assert(grp->size() < 256); // else they won't fit in 'choices'

				for (size_t i = 0; i < grp->size(); ++i)
				{
					if (selset->count((*grp)[i].m_VariantName))
					{
						match = (u8)i;
						break;
					}
				}

				// Stop after finding the first match
				if (match != -1)
					break;
			}

			// If no match, just choose the first
			if (match == -1)
				match = 0;
		}

		choices.push_back(match);

		// Remember which props were chosen. (Later-defined props override
		// earlier props at the same prop point.)
		CObjectBase::Variant& var ((*grp)[match]);
		for (std::vector<CObjectBase::Prop>::iterator it = var.m_Props.begin(); it != var.m_Props.end(); ++it)
		{
			if (it->m_ModelName.Length())
				chosenProps[it->m_PropPointName] = it->m_ModelName;
			else
				chosenProps.erase(it->m_PropPointName);
		}
	}

	// Load each prop, and add their CalculateVariationKey to our key:
	for (std::map<CStr, CStr>::iterator it = chosenProps.begin(); it != chosenProps.end(); ++it)
	{
		CObjectBase* prop = g_ObjMan.FindObjectBase(it->second);
		if (prop)
		{
			std::vector<u8> propChoices = prop->CalculateVariationKey(selections);
			choices.insert(choices.end(), propChoices.begin(), propChoices.end());
		}
	}

	return choices;
}

const CObjectBase::Variation CObjectBase::BuildVariation(const std::vector<u8>& variationKey)
{
	Variation variation;

	// variationKey should correspond with m_Variants, giving the id of the
	// chosen variant from each group. (Except variationKey has some bits stuck
	// on the end for props, but we don't care about those in here.)

	std::vector<std::vector<CObjectBase::Variant> >::iterator grp = m_VariantGroups.begin();
	std::vector<u8>::const_iterator match = variationKey.begin();
	for ( ;
		grp != m_VariantGroups.end() && match != variationKey.end();
		++grp, ++match)
	{
		// Ignore groups with nothing inside. (A warning will have been
		// emitted by the loading code.)
		if (grp->size() == 0)
			continue;

		size_t id = *match;
		if (id >= grp->size())
		{
			// This should be impossible
			debug_warn("BuildVariation: invalid variant id");
			continue;
		}

		// Get the matched variant
		CObjectBase::Variant& var ((*grp)[id]);

		// Apply its data:

		if (var.m_TextureFilename.Length())
			variation.texture = var.m_TextureFilename;

		if (var.m_ModelFilename.Length())
			variation.model = var.m_ModelFilename;

		if (var.m_Color.Length())
			variation.color = var.m_Color;

		for (std::vector<CObjectBase::Prop>::iterator it = var.m_Props.begin(); it != var.m_Props.end(); ++it)
		{
			if (it->m_ModelName.Length())
				variation.props[it->m_PropPointName] = *it;
			else
				variation.props.erase(it->m_PropPointName);
		}

		// If one variant defines one animation called e.g. "attack", and this
		// variant defines two different animations with the same name, the one
		// original should be erased, and replaced by the two new ones.
		//
		// So, erase all existing animations which are overridden by this variant:
		for (std::vector<CObjectBase::Anim>::iterator it = var.m_Anims.begin(); it != var.m_Anims.end(); ++it)
			variation.anims.erase(it->m_AnimName);
		// and then insert the new ones:
		for (std::vector<CObjectBase::Anim>::iterator it = var.m_Anims.begin(); it != var.m_Anims.end(); ++it)
			variation.anims.insert(make_pair(it->m_AnimName, *it));
	}

	return variation;
}

std::set<CStr> CObjectBase::CalculateRandomVariation(const std::set<CStr>& initialSelections)
{
	std::set<CStr> selections = initialSelections;

	std::map<CStr, CStr> chosenProps;

	// Calculate a complete list of selections, so there is at least one
	// (and in most cases only one) per group.
	// In each group, if one of the variants has a name matching a string in
	// 'selections', use that one.
	// If more than one matches, choose randomly from those matching ones.
	// If none match, choose randomly from all variants.
	//
	// When choosing randomly, make use of each variant's frequency. If all
	// variants have frequency 0, treat them as if they were 1.

	for (std::vector<std::vector<CObjectBase::Variant> >::iterator grp = m_VariantGroups.begin();
		grp != m_VariantGroups.end();
		++grp)
	{
		// Ignore groups with nothing inside. (A warning will have been
		// emitted by the loading code.)
		if (grp->size() == 0)
			continue;

		int match = -1; // -1 => none found yet

		// If there's only a single variant, choose that one
		if (grp->size() == 1)
		{
			match = 0;
		}
		else
		{
			// See if a variant (or several, but we only care about the first)
			// is already matched by the selections we've made

			for (size_t i = 0; i < grp->size(); ++i)
			{
				if (selections.count((*grp)[i].m_VariantName))
				{
					match = (int)i;
					break;
				}
			}

			// If there was one, we don't need to do anything now because there's
			// already something to choose. Otherwise, choose randomly from the others.
			if (match == -1)
			{
				// Sum the frequencies
				int totalFreq = 0;
				for (size_t i = 0; i < grp->size(); ++i)
					totalFreq += (*grp)[i].m_Frequency;

				// Someone might be silly and set all variants to have freq==0, in
				// which case we just pretend they're all 1
				bool allZero = (totalFreq == 0);
				if (allZero) totalFreq = (int)grp->size();

				// Choose a random number in the interval [0..totalFreq).
				// (It shouldn't be necessary to use a network-synchronised RNG,
				// since actors are meant to have purely visual manifestations.)
				int randNum = rand(0, totalFreq);

				// and use that to choose one of the variants
				for (size_t i = 0; i < grp->size(); ++i)
				{
					randNum -= (allZero ? 1 : (*grp)[i].m_Frequency);
					if (randNum < 0)
					{
						selections.insert((*grp)[i].m_VariantName);
						// (If this change to 'selections' interferes with earlier
						// choices, then we'll get some non-fatal inconsistencies
						// that just break the randomness. But that shouldn't
						// happen, much.)
						match = (int)i;
						break;
					}
				}
				debug_assert(randNum < 0);
				// This should always succeed; otherwise it
				// wouldn't have chosen any of the variants.
			}
		}

		// Remember which props were chosen. (Later-defined props override
		// earlier props at the same prop point.)
		CObjectBase::Variant& var ((*grp)[match]);
		for (std::vector<CObjectBase::Prop>::iterator it = var.m_Props.begin(); it != var.m_Props.end(); ++it)
		{
			if (it->m_ModelName.Length())
				chosenProps[it->m_PropPointName] = it->m_ModelName;
			else
				chosenProps.erase(it->m_PropPointName);
		}
	}

	// Load each prop, and add their required selections to ours:
	for (std::map<CStr, CStr>::iterator it = chosenProps.begin(); it != chosenProps.end(); ++it)
	{
		CObjectBase* prop = g_ObjMan.FindObjectBase(it->second);
		if (prop)
		{
			std::set<CStr> propSelections = prop->CalculateRandomVariation(selections);
			// selections = union(propSelections, selections)
			std::set<CStr> newSelections;
			std::set_union(propSelections.begin(), propSelections.end(),
				selections.begin(), selections.end(),
				std::inserter(newSelections, newSelections.begin()));
			selections.swap(newSelections);
		}
	}

	return selections;
}

std::vector<std::vector<CStr> > CObjectBase::GetVariantGroups() const
{
	std::vector<std::vector<CStr> > groups;

	// Queue of objects (main actor plus props (recursively)) to be processed
	std::queue<const CObjectBase*> objectsQueue;
	objectsQueue.push(this);

	// Set of objects already processed, so we don't do them more than once
	std::set<const CObjectBase*> objectsProcessed;

	while (objectsQueue.size())
	{
		const CObjectBase* obj = objectsQueue.front();
		objectsQueue.pop();
		// Ignore repeated objects (likely to be props)
		if (objectsProcessed.find(obj) != objectsProcessed.end())
			continue;

		objectsProcessed.insert(obj);

		// Iterate through the list of groups
		for (size_t i = 0; i < obj->m_VariantGroups.size(); ++i)
		{
			// Copy the group's variant names into a new vector
			std::vector<CStr> group;
			group.reserve(obj->m_VariantGroups[i].size());
			for (size_t j = 0; j < obj->m_VariantGroups[i].size(); ++j)
				group.push_back(obj->m_VariantGroups[i][j].m_VariantName);

			// If this group is identical to one elsewhere, don't bother listing
			// it twice.
			// Linear search is theoretically not very efficient, but hopefully
			// we don't have enough props for that to matter...
			bool dupe = false;
			for (size_t j = 0; j < groups.size(); ++j)
			{
				if (groups[j] == group)
				{
					dupe = true;
					break;
				}
			}
			if (dupe)
				continue;

			// Add non-trivial groups (i.e. not just one entry) to the returned list
			if (obj->m_VariantGroups[i].size() > 1)
				groups.push_back(group);

			// Add all props onto the queue to be considered
			for (size_t j = 0; j < obj->m_VariantGroups[i].size(); ++j)
			{
				const std::vector<Prop>& props = obj->m_VariantGroups[i][j].m_Props;
				for (size_t k = 0; k < props.size(); ++k)
				{
					if (props[k].m_ModelName.Length())
					{
						CObjectBase* prop = g_ObjMan.FindObjectBase(props[k].m_ModelName);
						if (prop)
							objectsQueue.push(prop);
					}
				}
			}
		}
	}

	return groups;
}
