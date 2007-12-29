/**
* =========================================================================
* File        : SoundGroup.cpp
* Project     : 0 A.D.
* Description : Loads up a group of sound files with shared properties, 
*				and provides a simple interface for playing them.        
* =========================================================================
*/

// license: GPL; see sound/license.txt

#include "precompiled.h"
#include "SoundGroup.h"

#include <algorithm>

#include "ps/XML/Xeromyces.h"
#include "ps/CLogger.h"
#include "lib/rand.h"

#define LOG_CATEGORY "audio"


CSoundGroup::CSoundGroup()
{
	m_index = 0;
	m_Flags = 0;
	m_Intensity = 0;
	m_CurTime = 0.0f;
	

}

CSoundGroup::CSoundGroup(const char *XMLfile)
{
	m_index = 0;
	m_Flags = 0;
	m_Intensity = 0;
	m_CurTime = 0.0f;
	LoadSoundGroup(XMLfile);
}

CSoundGroup::~CSoundGroup()
{
	// clean up all the handles from this group.
	ReleaseGroup();

}

void CSoundGroup::PlayNext()
{
	if(m_Intensity >= m_IntensityThreshold)
	{
		if(!is_playing(m_hReplacement))
		{
			// load up replacement file
			m_hReplacement = snd_open(m_filepath + m_intensity_file);
			snd_set_gain(m_hReplacement, m_Gain);	
			snd_set_pitch(m_hReplacement, m_Pitch);
			snd_set_cone(m_hReplacement, m_ConeInnerAngle, m_ConeOuterAngle, m_ConeOuterGain);

			//check for randomization of pitch and gain
			if(TestFlag(eRandPitch))
				snd_set_pitch(m_hReplacement, (float)((rand(m_PitchLower * 100.0f, m_PitchUpper * 100.0f) / 100.0f))); 
			if(TestFlag(eRandGain))
				snd_set_gain(m_hReplacement, (float)((rand(m_GainLower * 100.0f, m_GainUpper * 100.0f) / 100.0f)));	
		
			snd_play(m_hReplacement, m_Priority);
			
		}
	}
	else
	{
		// try loading on the fly only when we need the sound to see if that fixes release problems...
		if(TestFlag(eRandOrder))
			m_index = (size_t)rand(0, (uint)filenames.size());
		Handle temp;
		temp = snd_open(m_filepath + filenames[m_index]);
		snd_set_gain(temp, m_Gain);	
		snd_set_pitch(temp, m_Pitch);
		snd_set_cone(temp, m_ConeInnerAngle, m_ConeOuterAngle, m_ConeOuterGain);
		

		//check for randomization of pitch and gain
		if(TestFlag(eRandPitch))
			snd_set_pitch(temp, (float)((rand(m_PitchLower * 100.0f, m_PitchUpper * 100.0f) / 100.0f))); 
			//snd_set_pitch(snd_group[m_index], (float)((rand(m_PitchLower * 100.0f, m_PitchUpper * 100.0f) / 100.0f))); 
		
		if(TestFlag(eRandGain))
			snd_set_gain(temp, (float)((rand(m_GainLower * 100.0f, m_GainUpper * 100.0f) / 100.0f)));	
			//snd_set_gain(snd_group[m_index], (float)((rand(m_GainLower * 100.0f, m_GainUpper * 100.0f) / 100.0f)));	
	
		snd_play(temp, m_Priority);
		//snd_play(snd_group[m_index], m_Priority);
		
	
		
		
	}
	
	playtimes[m_index] = 0.0f;
	m_index++;
	m_Intensity++;
	if(m_Intensity > m_IntensityThreshold)
		m_Intensity = m_IntensityThreshold;
	

	if(m_index >= filenames.size())
		Reload();
}

void CSoundGroup::Reload()
{
	m_index = 0; // reset our  index
	// get rid of the used handles
	snd_group.clear();
	// clear out the old timers
	playtimes.clear();
	//Reload the sounds
	/*for(size_t i = 0; i < filenames.size(); i++)
	{
		string szTemp = m_filepath + filenames[i];
		Handle temp = snd_open(m_filepath + filenames[i]);
		snd_set_gain(temp, m_Gain);	
		snd_set_pitch(temp, m_Pitch);
		snd_set_cone(temp, m_ConeInnerAngle, m_ConeOuterAngle, m_ConeOuterGain);
		snd_group.push_back(temp);

	}*/
	while(playtimes.size() < filenames.size())
		playtimes.push_back(-1.0f);
	//if(TestFlag(eRandOrder))
		//random_shuffle(snd_group.begin(), snd_group.end());
}

void CSoundGroup::ReleaseGroup()
{

	for(size_t i = m_index; i<snd_group.size(); i++)
	{
		//if(!is_playing(snd_group[i]))	
			snd_free(snd_group[i]);

	}
	snd_group.clear();
	playtimes.clear();
	//if(is_playing(m_hReplacement))
	//	snd_free(m_hReplacement);
	m_index = 0;

}

void CSoundGroup::Update(float TimeSinceLastFrame)
{
	
	for(size_t i = 0; i < playtimes.size(); i++)
	{
		if(playtimes[i] >= 0.0f)
			playtimes[i] += TimeSinceLastFrame;

		if(playtimes[i] >= m_Decay)
		{
			playtimes[i] = -1.0f;
			m_Intensity--;
		}	
	}
}

bool CSoundGroup::LoadSoundGroup(const char *XMLfile)
{

	CXeromyces XeroFile;
	if (XeroFile.Load(XMLfile) != PSRETURN_OK)
		return false;

	// adjust the path name for resources if necessary
	//m_Name = XMLfile + directorypath;

	//Define elements used in XML file
	#define EL(x) int el_##x = XeroFile.GetElementID(#x)
	#define AT(x) int at_##x = XeroFile.GetAttributeID(#x)
	EL(soundgroup);
	EL(gain);
	EL(looping);
	EL(pitch);
	EL(priority);
	EL(randorder);
	EL(randgain);
	EL(randpitch);
	EL(conegain);
	EL(coneinner);
	EL(coneouter);
	EL(sound);
	EL(gainupper);
	EL(gainlower);
	EL(pitchupper);
	EL(pitchlower);
	EL(path);
	EL(threshold);
	EL(decay);
	EL(replacement);
	#undef AT
	#undef EL

	XMBElement root = XeroFile.GetRoot();

	if (root.GetNodeName() != el_soundgroup)
	{
		LOG(CLogger::Error, LOG_CATEGORY, "Invalid SoundGroup format (unrecognised root element '%s')", XeroFile.GetElementString(root.GetNodeName()).c_str());
		return false;
	}
	
	XERO_ITER_EL(root, child)
	{
	
		int child_name = child.GetNodeName();			
		
		if(child_name == el_gain)
		{
			this->m_Gain = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_looping)
		{			
			if(CStr(child.GetText()).ToInt() == 1)
				SetFlag(eLoop);
		}

		if(child_name == el_pitch)
		{
			this->m_Pitch = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_priority)
		{
			this->m_Priority = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_randorder)
		{
			if(CStr(child.GetText()).ToInt() == 1)
				SetFlag(eRandOrder);
		}

		if(child_name == el_randgain)
		{
			if(CStr(child.GetText()).ToInt() == 1)
				SetFlag(eRandGain);
		}

		if(child_name == el_gainupper)
		{
			this->m_GainUpper = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_gainlower)
		{
			this->m_GainLower = CStr(child.GetText()).ToFloat();
		}



		if(child_name == el_randpitch)
		{
			if(CStr(child.GetText()).ToInt() == 1)
				SetFlag(eRandPitch);
		}


		if(child_name == el_pitchupper)
		{
			this->m_PitchUpper = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_pitchlower)
		{
			this->m_PitchLower = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_conegain)
		{
			this->m_ConeOuterGain = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_coneinner)
		{
			this->m_ConeInnerAngle = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_coneouter)
		{
			this->m_ConeOuterAngle = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_sound)
		{
			CStr szTemp(child.GetText());
			this->filenames.push_back(szTemp);	
			
		}
		if(child_name == el_path)
		{
			m_filepath = child.GetText();
		
		}
		if(child_name == el_threshold)
		{
			//m_intensity_threshold = CStr(child.GetText()).ToFloat();
			m_IntensityThreshold = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_decay)
		{
			//m_intensity_threshold = CStr(child.GetText()).ToFloat();
			m_Decay = CStr(child.GetText()).ToFloat();
		}

		if(child_name == el_replacement)
		{
			m_intensity_file = child.GetText();
		
		}
	}


	Reload();
	return true;

}
