#ifndef INCLUDED_MISCSTATE
#define INCLUDED_MISCSTATE

#include "General/Observable.h"

namespace AtlasMessage
{
	typedef int ObjectID;
}

extern wxString g_SelectedTexture;

extern Observable<std::vector<AtlasMessage::ObjectID> > g_SelectedObjects;

#endif // INCLUDED_MISCSTATE
