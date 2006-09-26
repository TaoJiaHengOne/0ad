#include "stdafx.h"

#include "ObjectSettings.h"

#include "GameInterface/Messages.h"
#include "ScenarioEditor/Tools/Common/Tools.h"

Observable<ObjectSettings> g_ObjectSettings;

ObjectSettings::ObjectSettings()
: m_PlayerID(0)
{
	m_Conn = g_SelectedObjects.RegisterObserver(0, &ObjectSettings::OnSelectionChange, this);
}

ObjectSettings::~ObjectSettings()
{
	m_Conn.disconnect();
}

int ObjectSettings::GetPlayerID() const
{
	return m_PlayerID;
}

void ObjectSettings::SetPlayerID(int playerID)
{
	m_PlayerID = playerID;
	PostToGame();
}

const std::set<wxString>& ObjectSettings::GetActorSelections() const
{
	return m_ActorSelections;
}

void ObjectSettings::SetActorSelections(const std::set<wxString>& selections)
{
	m_ActorSelections = selections;
	PostToGame();
}

const std::vector<ObjectSettings::Group> ObjectSettings::GetActorVariation() const
{
	std::vector<Group> variation;
	
	for (std::vector<wxArrayString>::const_iterator grp = m_VariantGroups.begin();
		grp != m_VariantGroups.end();
		++grp)
	{
		Group group;
		group.variants = *grp;

		// Variant choice method, as used by the game: Choose the first variant
		// which matches any of the selections

		size_t chosen = 0; // default to first
		for (size_t i = 0; i < grp->GetCount(); ++i)
		{
			if (m_ActorSelections.find(grp->Item(i)) != m_ActorSelections.end())
			{
				chosen = i;
				break;
			}
		}
		group.chosen = grp->Item(chosen);

		variation.push_back(group);
	}

	return variation;
}

AtlasMessage::sObjectSettings ObjectSettings::GetSettings() const
{
	AtlasMessage::sObjectSettings settings;

	settings.player = m_PlayerID;

	// Copy selections from set into vector
	std::vector<std::wstring> selections;
	for (std::set<wxString>::const_iterator it = m_ActorSelections.begin();
		it != m_ActorSelections.end();
		++it)
	{
		selections.push_back(it->c_str());
	}
	settings.selections = selections;

	return settings;
}

void ObjectSettings::OnSelectionChange(const std::vector<AtlasMessage::ObjectID>& selection)
{
	// TODO: what would be the sensible action if nothing's selected?
	// and if multiple objects are selected?

	if (selection.empty())
		return;
		
	AtlasMessage::qGetObjectSettings qry (selection[0]);
	qry.Post();

	m_PlayerID = qry.settings->player;

	m_ActorSelections.clear();
	m_VariantGroups.clear();

	std::vector<std::vector<std::wstring> > variation = *qry.settings->variantgroups;
	for (std::vector<std::vector<std::wstring> >::iterator grp = variation.begin();
		grp != variation.end();
		++grp)
	{
		wxArrayString variants;

		for (std::vector<std::wstring>::iterator it = grp->begin();
			it != grp->end();
			++it)
		{
			variants.Add(it->c_str());
		}

		m_VariantGroups.push_back(variants);
	}

	std::vector<std::wstring> selections = *qry.settings->selections;
	for (std::vector<std::wstring>::iterator sel = selections.begin();
		sel != selections.end();
		++sel)
	{
		m_ActorSelections.insert(sel->c_str());
	}

	static_cast<Observable<ObjectSettings>*>(this)->NotifyObservers();
}

void ObjectSettings::PostToGame()
{
	if (g_SelectedObjects.empty())
		return;

	POST_COMMAND(SetObjectSettings, (g_SelectedObjects[0], GetSettings()));
}