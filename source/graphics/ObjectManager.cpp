#include "precompiled.h"

#include "ObjectManager.h"

#include "graphics/ObjectBase.h"
#include "graphics/ObjectEntry.h"
#include "ps/CLogger.h"
#include "ps/Profile.h"
#include "ps/VFSUtil.h"

#define LOG_CATEGORY "graphics"

template<typename T, typename S>
static void delete_pair_2nd(std::pair<T,S> v)
{
	delete v.second;
}

template<typename T>
struct second_equals
{
	T x;
	second_equals(const T& x) : x(x) {}
	template<typename S> bool operator()(const S& v) { return v.second == x; }
};

bool operator< (const CObjectManager::ObjectKey& a, const CObjectManager::ObjectKey& b)
{
	if (a.ActorName < b.ActorName)
		return true;
	else if (a.ActorName > b.ActorName)
		return false;
	else
		return a.ActorVariation < b.ActorVariation;
}

CObjectManager::CObjectManager(CMeshManager& meshManager)
: m_MeshManager(meshManager)
{
}

CObjectManager::~CObjectManager()
{
	UnloadObjects();
}


CObjectBase* CObjectManager::FindObjectBase(const char* objectname)
{
	debug_assert(strcmp(objectname, "") != 0);

	// See if the base type has been loaded yet:

	std::map<CStr, CObjectBase*>::iterator it = m_ObjectBases.find(objectname);
	if (it != m_ObjectBases.end())
		return it->second;

	// Not already loaded, so try to load it:

	CObjectBase* obj = new CObjectBase(*this);

	if (obj->Load(objectname))
	{
		m_ObjectBases[objectname] = obj;
		return obj;
	}
	else
		delete obj;

	LOG(ERROR, LOG_CATEGORY, "CObjectManager::FindObjectBase(): Cannot find object '%s'", objectname);

	return 0;
}

CObjectEntry* CObjectManager::FindObject(const char* objname)
{
	std::vector<std::set<CStr> > selections; // TODO - should this really be empty?
	return FindObjectVariation(objname, selections);
}

CObjectEntry* CObjectManager::FindObjectVariation(const char* objname, const std::vector<std::set<CStr> >& selections)
{
	CObjectBase* base = FindObjectBase(objname);

	if (! base)
		return NULL;

	return FindObjectVariation(base, selections);
}

CObjectEntry* CObjectManager::FindObjectVariation(CObjectBase* base, const std::vector<std::set<CStr> >& selections)
{
	PROFILE( "object variation loading" );

	// Look to see whether this particular variation has already been loaded

	std::vector<u8> choices = base->CalculateVariationKey(selections);
	ObjectKey key (base->m_Name, choices);

	std::map<ObjectKey, CObjectEntry*>::iterator it = m_Objects.find(key);
	if (it != m_Objects.end())
		return it->second;

	// If it hasn't been loaded, load it now

	CObjectEntry* obj = new CObjectEntry(base); // TODO: type ?

	// TODO (for some efficiency): use the pre-calculated choices for this object,
	// which has already worked out what to do for props, instead of passing the
	// selections into BuildVariation and having it recalculate the props' choices.

	if (! obj->BuildVariation(selections, choices, *this))
	{
		DeleteObject(obj);
		return NULL;
	}

	m_Objects[key] = obj;

	return obj;
}


void CObjectManager::DeleteObject(CObjectEntry* entry)
{
	std::map<ObjectKey, CObjectEntry*>::iterator it;
	while (m_Objects.end() != (it = find_if(m_Objects.begin(), m_Objects.end(), second_equals<CObjectEntry*>(entry))))
		m_Objects.erase(it);

	delete entry;
}


void CObjectManager::UnloadObjects()
{
	std::for_each(
		m_Objects.begin(),
		m_Objects.end(),
		delete_pair_2nd<ObjectKey, CObjectEntry*>
	);
	m_Objects.clear();

	std::for_each(
		m_ObjectBases.begin(),
		m_ObjectBases.end(),
		delete_pair_2nd<CStr, CObjectBase*>
	);
	m_ObjectBases.clear();
}



static void GetObjectName_ThunkCb(const char* path, const DirEnt* UNUSED(ent), void* context)
{
	std::vector<CStr>* names = (std::vector<CStr>*)context;
	CStr name (path);
	names->push_back(name.AfterFirst("actors/"));
}

void CObjectManager::GetAllObjectNames(std::vector<CStr>& names)
{
	vfs_dir_enum("art/actors/", VFS_DIR_RECURSIVE, "*.xml",
		GetObjectName_ThunkCb, &names);
}

void CObjectManager::GetPropObjectNames(std::vector<CStr>& names)
{
	vfs_dir_enum("art/actors/props/", VFS_DIR_RECURSIVE, "*.xml",
		GetObjectName_ThunkCb, &names);
}
