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

#ifndef INCLUDED_ICOMPONENT
#define INCLUDED_ICOMPONENT

#include "Components.h"
#include "Message.h"
#include "Entity.h"
#include "SimContext.h"
#include "scriptinterface/ScriptForward.h"

class CParamNode;
class CMessage;
class ISerializer;
class IDeserializer;

class IComponent
{
public:
	// Component allocation types
	using AllocFunc = IComponent* (*)(const ScriptInterface& scriptInterface, JS::HandleValue ctor);
	using DeallocFunc = void (*)(IComponent*);
	using ClassInitFunc = void (*)(CComponentManager& componentManager);

	virtual ~IComponent();

	static std::string GetSchema();

	static void RegisterComponentType(CComponentManager& mgr, EInterfaceId iid, EComponentTypeId cid, AllocFunc alloc, DeallocFunc dealloc, const char* name, const std::string& schema);
	static void RegisterComponentTypeScriptWrapper(CComponentManager& mgr, EInterfaceId iid,
		EComponentTypeId cid, AllocFunc alloc, DeallocFunc dealloc, const char* name,
		const std::string& schema, ClassInitFunc classInit);

	virtual void Init(const CParamNode& paramNode) = 0;
	virtual void Deinit() = 0;

	virtual void HandleMessage(const CMessage& msg, bool global);

	CEntityHandle GetEntityHandle() const { return m_EntityHandle; }
	void SetEntityHandle(CEntityHandle ent) { m_EntityHandle = ent; }

	entity_id_t GetEntityId() const { return m_EntityHandle.GetId(); }

	CEntityHandle GetSystemEntity() const { return m_SimContext->GetSystemEntity(); }

	const CSimContext& GetSimContext() const { return *m_SimContext; }
	void SetSimContext(const CSimContext& context) { m_SimContext = &context; }

	static u8 GetSerializationVersion() { return 0; }
	virtual void Serialize(ISerializer& serialize) = 0;
	virtual void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) = 0;

	/**
	 * @Returns JS::NullHandleValue if a scripted wrapper of this IComponent is not supported, the wrapper otherwise.
	 */
	virtual JS::HandleValue GetJSInstance() const = 0;
	virtual int GetComponentTypeId() const = 0;

private:
	/**
	 * @Returns whether a scripted wrapper of this IComponent is not supported.
	 * Derrived classes should return true if they implement such a wrapper.
	 */
	virtual bool NewJSObject(const ScriptInterface& scriptInterface, JS::MutableHandleObject out) const = 0;

	CEntityHandle m_EntityHandle;
	const CSimContext* m_SimContext;
};

#endif // INCLUDED_ICOMPONENT
