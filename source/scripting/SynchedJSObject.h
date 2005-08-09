/*
	CSynchedJSObject
	
	AUTHOR: Simon Brenner <simon.brenner@home.se>
	
	DESCRIPTION:
		A helper class for CJSObject that enables a callback to be called
		whenever an attribute of the class changes and enables all (synched)
		properties to be set and retrieved as strings for network sync.
		
		All string conversions are performed by specific functions that use a
		strictly (hrm) defined format - or at least a format that is specific
		for the type in question (which is why JSParseString can't be used -
		the JS interface's ToString function is also not usable since it often
		produces a human-readable format that doesn't parse well and might
		change outside the control of the network protocol).
		
		This replaces CAttributeMap for both player and game attributes.
	
	USAGE:
		First you must create your subclass, make it inherit from
		CSynchedJSObject and implement the pure virtual method Update (see
		prototype below)
		
		Then you may use it just like CJSObject (see ScriptableObject.h) - with
		one exception: Any property you want to be synchronized (i.e. have the
		new property functionality including the update callback) is added using
		the AddSynchedProperty method instead: AddSynchedProperty(name, &m_Property)
		
		The extra arguments that exist in the AddProperty method haven't been
		implemented (if you by any chance would need to, just do it ;-)

*/

#ifndef _SynchedJSObject_H
#define _SynchedJSObject_H

#include "CStr.h"
#include "ScriptableObject.h"

template <typename T>
void SetFromNetString(T &data, CStrW string);

template <typename T>
CStrW ToNetString(const T &data);

#define TYPE(type) \
	template <> CStrW ToNetString(const type &data); \
	template <> void SetFromNetString(type &data, CStrW string);

TYPE(uint)
TYPE(CStrW)
	
#undef TYPE

class ISynchedJSProperty: public IJSProperty
{
public:
	virtual void FromString(CStrW value)=0;
	virtual CStrW ToString()=0;
};

// non-templated base class
struct CSynchedJSObjectBase
{
	typedef void (*UpdateFn)(CSynchedJSObjectBase *owner);

	template <typename PropType, bool ReadOnly = false>
	class CSynchedJSProperty: public ISynchedJSProperty
	{
		PropType *m_Data;
		CStrW m_Name;
		CSynchedJSObjectBase *m_Owner;
		UpdateFn m_Update;
		
		virtual void Set(JSContext* cx, IJSObject* UNUSED(owner), jsval value)
		{
			if (!ReadOnly)
			{
				if (ToPrimitive(cx, value, *m_Data))
				{
					m_Owner->Update(m_Name, this);
					if (m_Update)
						m_Update(m_Owner);
				}
			}
		}
		virtual jsval Get(JSContext* UNUSED(cx), IJSObject* UNUSED(owner))
		{
			return ToJSVal(*m_Data);
		}
		
		virtual void ImmediateCopy(IJSObject* UNUSED(CopyFrom), IJSObject* UNUSED(CopyTo), IJSProperty* other)
		{
			*m_Data = *( ((CSynchedJSProperty<PropType, ReadOnly>*)other)->m_Data );
		}
		
		virtual void FromString(CStrW value)
		{
			SetFromNetString(*m_Data, value);
			if (m_Update)
				m_Update(m_Owner);
		}
		
		virtual CStrW ToString()
		{
			return ToNetString(*m_Data);
		}
		
	public:
		inline CSynchedJSProperty(CStrW name, PropType* native, CSynchedJSObjectBase *owner, UpdateFn update=NULL):
			m_Data(native),
			m_Name(name),
			m_Owner(owner),
			m_Update(update)
		{
		}
	};
	
	typedef STL_HASH_MAP<CStrW, ISynchedJSProperty *, CStrW_hash_compare> SynchedPropertyTable;
	typedef SynchedPropertyTable::iterator SynchedPropertyIterator;
	SynchedPropertyTable m_SynchedProperties;

protected:
	// Called every time a property changes.
	// This is where the individual callbacks are dispatched from.
	virtual void Update(CStrW name, ISynchedJSProperty *prop)=0;

public:
	ISynchedJSProperty *GetSynchedProperty(CStrW name);

	typedef void (IterateCB)(CStrW name, ISynchedJSProperty *prop, void *userdata);
	void IterateSynchedProperties(IterateCB *cb, void *userdata);
};

template <typename Class>
class CSynchedJSObject: public CJSObject<Class>, public CSynchedJSObjectBase
{
protected:
	// Add a property to the object; if desired, a callback is called every time it changes.
	// Replaces CJSObject's AddProperty.
	template <typename T> void AddSynchedProperty(CStrW name, T *native, UpdateFn update=NULL)
	{
		ISynchedJSProperty *prop=new CSynchedJSProperty<T>(name, native, this, update);
		m_NonsharedProperties[name]=prop;
		m_SynchedProperties[name]=prop;
	}
};

#endif
