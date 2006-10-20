#include "precompiled.h"
#include "Formation.h"
#include "ps/CLogger.h"
#include "ps/CStr.h"
#include "maths/MathUtil.h"

#define LOG_CATEGORY "Formation"

CFormation::CFormation()
{
	m_numSlots = 0;
}
bool CFormation::loadXML(const CStr& filename)
{
	CXeromyces XeroFile;

	if (XeroFile.Load(filename) != PSRETURN_OK)
		return false;

	#define EL(x) int el_##x = XeroFile.getElementID(#x)
	#define AT(x) int at_##x = XeroFile.getAttributeID(#x)
	EL(formation);
	EL(fl);
	EL(rk);
	EL(blank);

	AT(tag);
	AT(bonus);
	AT(bonusbase);
	AT(bonustype);
	AT(bonusval);
	AT(penalty);
	AT(penaltybase);
	AT(penaltytype);
	AT(penaltyval);
	AT(anglepenalty);
	AT(anglepenaltydivs);
	AT(anglepenaltytype);
	AT(anglepenaltyval);

	AT(required);
	AT(next);
	AT(prior);
	AT(movement);
	AT(filespacing);
	AT(rankspacing);
	AT(category);
	AT(order);
	#undef AT
	#undef EL

	XMBElement Root = XeroFile.getRoot();
	if( Root.getNodeName() != el_formation )
	{
		LOG( ERROR, LOG_CATEGORY, "CFormation::LoadXML: XML root was not \"Formation\" in file %s. Load failed.", filename.c_str() );
		return( false );
	}

	//Load in single attributes
	XMBAttributeList Attributes = Root.getAttributes();
	for ( int i=0; i<Attributes.Count; ++i )
	{
		XMBAttribute Attr = Attributes.item(i);
		if ( Attr.Name == at_tag )
			m_tag = CStr(Attr.Value);
		else if ( Attr.Name == at_bonus )
			m_bonus = CStr(Attr.Value);
		else if ( Attr.Name == at_bonusbase )
			m_bonusBase = CStr(Attr.Value);
		else if ( Attr.Name == at_bonustype )
			m_bonusType = CStr(Attr.Value);
		else if ( Attr.Name == at_bonusval )
			m_bonusVal = CStr(Attr.Value).ToFloat();
		else if ( Attr.Name == at_penalty )
			m_penalty = CStr(Attr.Value);
		else if ( Attr.Name == at_penaltybase )
			m_penaltyBase = CStr(Attr.Value);
		else if ( Attr.Name == at_penaltytype )
			m_penaltyType = CStr(Attr.Value);
		else if ( Attr.Name == at_penaltyval )
			m_penaltyVal = CStr(Attr.Value).ToFloat();

		else if ( Attr.Name == at_anglepenalty )
			m_anglePenalty = CStr(Attr.Value);
		else if ( Attr.Name == at_anglepenaltydivs )
			m_anglePenaltyDivs = CStr(Attr.Value).ToInt();
		else if ( Attr.Name == at_anglepenaltytype )
			m_anglePenaltyType = CStr(Attr.Value);
		else if ( Attr.Name == at_anglepenaltyval )
			m_anglePenaltyVal = CStr(Attr.Value).ToFloat();

		else if ( Attr.Name == at_required)
			m_required = CStr(Attr.Value).ToInt();
		else if ( Attr.Name == at_next )
			m_next = CStr(Attr.Value);
		else if ( Attr.Name == at_prior )
			m_prior = CStr(Attr.Value);
		else if ( Attr.Name == at_movement )
			m_movement = CStr(Attr.Value);
		else if ( Attr.Name == at_rankspacing )
			m_rankSpacing = CStr(Attr.Value).ToFloat();
		else if ( Attr.Name == at_filespacing )
			m_fileSpacing = CStr(Attr.Value).ToFloat();
		else
		{
			const char* invAttr = XeroFile.getAttributeString(Attr.Name).c_str();
			LOG( ERROR, LOG_CATEGORY, "CFormation::LoadXML: Invalid attribute %s defined in formation file %s. Load failed.", invAttr, filename.c_str() );
			return( false );
		}
	}

    XMBElementList RootChildren = Root.getChildNodes();
	int file=0;
	int rank=0;
	int maxrank=0;

	//Read in files and ranks
    for (int i = 0; i < RootChildren.Count; ++i)
    {
        XMBElement RootChild = RootChildren.item(i);
        int ChildName = RootChild.getNodeName();

        if ( ChildName == el_fl )
		{
			 rank = 0;
			XMBAttributeList FileAttribList = RootChild.getAttributes();
			//Load default category
			CStr FileCatValue = FileAttribList.getNamedItem(at_category);

			//Specific slots in this file (row)
			XMBElementList RankNodes = RootChild.getChildNodes();
			for ( int r=0; r<RankNodes.Count; ++r )
			{
				XMBElement Rank = RankNodes.item(r);
				if ( Rank.getNodeName() == el_blank )
				{
					++rank;
					continue;
				}
				else if ( Rank.getNodeName() != el_rk )
					return false;
					//error

				XMBAttributeList RankAttribList = Rank.getAttributes();
				int order = CStr( RankAttribList.getNamedItem(at_order) ).ToInt();
				CStr category = CStr( RankAttribList.getNamedItem(at_category) );

				if( order <= 0 )
				{
					LOG( ERROR, LOG_CATEGORY, "CFormation::LoadXML: Invalid (negative number or 0) order defined in formation file %s. The game will try to continue anyway.", filename.c_str() );
					continue;
				}
				--order;	//We need this to be in line with arrays, so start at 0
				//if ( category.length() )
					//AssignCategory(order, category);
				//else
					AssignCategory(order, FileCatValue);

				m_slots[order].fileOff = file * m_fileSpacing;
				m_slots[order].rankOff = rank * m_rankSpacing;
				++m_numSlots;
				++rank;
			}
			if ( rank > maxrank )
				maxrank = rank;
			++file;
		}	//if el_fl

		else if ( ChildName == el_blank )
			++file;
	}

	float centerx = maxrank * m_rankSpacing / 2.0f;
	float centery = file * m_fileSpacing / 2.0f;

	//Here we check to make sure no order was skipped over.  If so, failure, because we rely
	//on a linearly accessible slots in entityformation.cpp.
	for ( int i=0; i<m_numSlots; ++i )
	{
		if ( m_slots.find(i) == m_slots.end() )
		{
			LOG( ERROR, LOG_CATEGORY, "CFormation::LoadXML: Missing orders in %s. Load failed.", filename.c_str() );
			return false;
		}
		else
		{
			m_slots[i].rankOff = m_slots[i].rankOff - centerx;
			m_slots[i].fileOff = m_slots[i].fileOff - centery;
		}

	}
	return true;
}

void CFormation::AssignCategory(int order, CStr category)
{
	category.Remove( CStr(",") );
	category = category + " ";	//So the final word will be pushed as well
	CStr temp;

	//Push categories until last space
	while ( ( temp = category.BeforeFirst(" ") ) != "" )
	{
		m_slots[order].category.push_back(temp);
		//Don't use remove because certain categories could be substrings of others
		size_t off = category.find(temp);
		category.erase( off, temp.length() );
	}
}
