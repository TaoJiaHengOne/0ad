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

#ifndef INCLUDED_SCRIPTCOMPONENT
#define INCLUDED_SCRIPTCOMPONENT

// These headers are included because they are required in component implementation,
// so including them here transitively makes sense.
#include "scriptinterface/FunctionWrapper.h"
#include "simulation2/system/CmpPtr.h"
#include "simulation2/system/Components.h"
#include "simulation2/system/IComponent.h"
#include "simulation2/system/ParamNode.h"
#include "simulation2/system/SimContext.h"
#include "simulation2/serialization/ISerializer.h"
#include "simulation2/serialization/IDeserializer.h"

#include "ps/CLogger.h"

class CComponentTypeScript
{
	NONCOPYABLE(CComponentTypeScript);
public:
	CComponentTypeScript(const ScriptInterface& scriptInterface, JS::HandleValue instance);

	JS::HandleValue GetInstance() const { return JS::HandleValue::fromMarkedLocation(m_Instance.address()); }
	JS::MutableHandleValue GetMutInstance() { return JS::MutableHandleValue::fromMarkedLocation(const_cast<JS::Value*>(m_Instance.address())); }
	static void Trace(JSTracer* trc, void* data);

	void Init(CComponentManager& cmpMgr, const CParamNode& paramNode, entity_id_t ent);
	void Deinit();
	bool HasMessageHandler(const CMessage& msg, const bool global);
	void HandleMessage(const CMessage& msg, bool global);

	void Serialize(ISerializer& serialize);
	void Deserialize(CComponentManager& cmpMgr, const CParamNode& paramNode, IDeserializer& deserialize, entity_id_t ent);

	template<typename R, typename... Ts>
	R Call(const char* funcname, const Ts&... params) const
	{
		R ret;
		ScriptRequest rq(m_ScriptInterface);
		if (ScriptFunction::Call(rq, GetInstance(), funcname, ret, params...))
			return ret;
		LOGERROR("Error calling component script function %s", funcname);
		return R();
	}

	// CallRef is mainly used for returning script values with correct stack rooting.
	template<typename R, typename... Ts>
	void CallRef(const char* funcname, R ret, const Ts&... params) const
	{
		ScriptRequest rq(m_ScriptInterface);
		if (!ScriptFunction::Call(rq, GetInstance(), funcname, ret, params...))
			LOGERROR("Error calling component script function %s", funcname);
	}

	template<typename... Ts>
	void CallVoid(const char* funcname, const Ts&... params) const
	{
		ScriptRequest rq(m_ScriptInterface);
		if (!ScriptFunction::CallVoid(rq, GetInstance(), funcname, params...))
			LOGERROR("Error calling component script function %s", funcname);
	}

private:
	const ScriptInterface& m_ScriptInterface;
	JS::Heap<JS::Value> m_Instance;
};

#define REGISTER_COMPONENT_SCRIPT_WRAPPER(cname) \
	void RegisterComponentType_##cname(CComponentManager& mgr) \
	{ \
		IComponent::RegisterComponentTypeScriptWrapper(mgr, CCmp##cname::GetInterfaceId(), \
			CID_##cname, CCmp##cname::Allocate, CCmp##cname::Deallocate, #cname, \
			CCmp##cname::GetSchema(), CCmp##cname::ClassInit); \
	}


#define DEFAULT_SCRIPT_WRAPPER_BASIC(cname) \
	static IComponent* Allocate(const ScriptInterface& scriptInterface, JS::HandleValue instance) \
	{ \
		return new CCmp##cname(scriptInterface, instance); \
	} \
	static void Deallocate(IComponent* cmp) \
	{ \
		delete static_cast<CCmp##cname*> (cmp); \
	} \
	CCmp##cname(const ScriptInterface& scriptInterface, JS::HandleValue instance) : m_Script(scriptInterface, instance) { } \
	static std::string GetSchema() \
	{ \
		return "<a:component type='script-wrapper'/><empty/>"; \
	} \
	void Init(const CParamNode& paramNode) override \
	{ \
		m_Script.Init(GetSimContext().GetComponentManager(), paramNode, GetEntityId()); \
	} \
	void Deinit() override \
	{ \
		m_Script.Deinit(); \
	} \
	JS::HandleValue GetJSInstance() const override \
	{ \
		return m_Script.GetInstance(); \
	} \
	int GetComponentTypeId() const override \
	{ \
		return CID_##cname; \
	} \
	private: \
		CComponentTypeScript m_Script; \
	public:


#define DEFAULT_SCRIPT_WRAPPER(cname) \
	static void ClassInit(CComponentManager&) { } \
	void HandleMessage(const CMessage& msg, bool global) override \
	{ \
		m_Script.HandleMessage(msg, global); \
	} \
	void Serialize(ISerializer& serialize) override \
	{ \
		m_Script.Serialize(serialize); \
	} \
	void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize) override \
	{ \
		m_Script.Deserialize(GetSimContext().GetComponentManager(), paramNode, deserialize, GetEntityId()); \
	} \
	DEFAULT_SCRIPT_WRAPPER_BASIC(cname)

#endif // INCLUDED_SCRIPTCOMPONENT
