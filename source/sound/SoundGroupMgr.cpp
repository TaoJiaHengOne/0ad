/**
* =========================================================================
* File        : SoundGroupMgr.h
* Project     : 0 A.D.
* Description : Manages and updates SoundGroups        
* =========================================================================
*/

/*
 * Copyright (c) 2005-2006 Gavin Fowler
 *
 * Redistribution and/or modification are also permitted under the
 * terms of the GNU General Public License as published by the
 * Free Software Foundation (version 2 or later, at your option).
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

// Example usage: 

// size_t index;
// CSoundGroupMgr *sgm = CSoundGroupMgr::GetInstance();
// index = sgm->AddGroup("SoundGroup.xml");

// sgm->UpdateSoundGroups(TimeSinceLastFrame); // call in Frame()

// sgm->PlayNext(index);  // wash-rinse-repeat


// sgm->RemoveGroup(index);  // Remove the group if you like

// sgm->DeleteInstance();  // Delete instance in shutdown


#include "precompiled.h"
#include "SoundGroupMgr.h"


CSoundGroupMgr *CSoundGroupMgr::m_pInstance = 0;

CSoundGroupMgr::CSoundGroupMgr()
{

}

CSoundGroupMgr *CSoundGroupMgr::GetInstance()
{
	if(!m_pInstance)
		m_pInstance = new CSoundGroupMgr();

	return m_pInstance;

}

void CSoundGroupMgr::DeleteInstance()
{
	if(m_pInstance)
	{
		
		vector<CSoundGroup *>::iterator vIter = m_pInstance->m_Groups.begin();
		while(vIter != m_pInstance->m_Groups.end())
			vIter = m_pInstance->RemoveGroup(vIter);		
		
		delete m_pInstance;
	}		
	m_pInstance = 0;

}

///////////////////////////////////////////
// AddGroup()
// in:  const char *XMLFile - the filename of the SoundGroup.xml to open
// out: size_t index into m_Groups
// Loads the given XML file and returns an index for later use
///////////////////////////////////////////
size_t CSoundGroupMgr::AddGroup(const char *XMLFile)
{
	CSoundGroup* newGroup = new CSoundGroup(XMLFile);
	m_Groups.push_back(newGroup);

	return m_Groups.size() - 1;
}

///////////////////////////////////////////
// RemoveGroup()
// in: size_t index into m_Groups
// out: vector<CSoundGroup *>::iterator - one past the index removed (sometimes useful)
// Removes and Releases a given soundgroup
///////////////////////////////////////////
vector<CSoundGroup *>::iterator CSoundGroupMgr::RemoveGroup(size_t index)
{

	vector<CSoundGroup *>::iterator vIter = m_Groups.begin();
	if(index >= m_Groups.size())
		return vIter;

	
	
	CSoundGroup *temp = (*vIter);
	(*vIter)->ReleaseGroup();
	vIter = m_Groups.erase(vIter+index);
	
	delete temp;

	return vIter;

}

///////////////////////////////////////////
// RemoveGroup()
// in: vector<CSoundGroup *>::iterator - item to remove
// out: vector<CSoundGroup *>::iterator - one past the index removed (sometimes useful)
// Removes and Releases a given soundgroup
///////////////////////////////////////////
vector<CSoundGroup *>::iterator CSoundGroupMgr::RemoveGroup(vector<CSoundGroup *>::iterator iter)
{

	(*iter)->ReleaseGroup();
	
	CSoundGroup *temp = (*iter);
	
	iter = m_Groups.erase(iter);
	
	delete temp;

	return iter;

}

///////////////////////////////////////////
// UpdateSoundGroups()
// updates all soundgroups, call in Frame()
///////////////////////////////////////////
void CSoundGroupMgr::UpdateSoundGroups(float TimeSinceLastFrame)
{
	vector<CSoundGroup *>::iterator vIter = m_Groups.begin();	
	while(vIter != m_Groups.end())
	{
		(*vIter)->Update(TimeSinceLastFrame);
		vIter++;
	}
}

///////////////////////////////////////////
// PlayNext()
// in:  size_t index - index into m_Groups
// Plays the next queued sound in an indexed group 
///////////////////////////////////////////
void CSoundGroupMgr::PlayNext(size_t index)
{
	m_Groups[index]->PlayNext();
}
