//========================================================
//Andrew Decker aka pyrolink
//ajdecker1022@msn.com
//Description: Manages loading trigger specs (format in Atlas) and updating trigger objects
//=======================================================

#ifndef TRIGGERMANAGER_INCLUDED
#define TRIGGERMANAGER_INCLUDED

#include "ps/Singleton.h"
#include "scripting/ScriptableObject.h"
#include "simulation/ScriptObject.h"

class CXeromyces;
class XMBElement;

//Trigger storage - used be Atlas to keep track of different triggers

struct MapTriggerCondition
{
	MapTriggerCondition() : linkLogic(0), not(false) { }
	CStrW name, functionName, displayName;
	std::list<CStrW> parameters;
	int linkLogic;	//0 = NONE, 1 = AND, 2 = OR
	bool not;
};

struct MapTriggerEffect
{
	CStrW name, functionName, displayName;
	std::list<CStrW> parameters;
};

struct MapTriggerLogicBlock
{
	MapTriggerLogicBlock(size_t i, bool _not=false) : index(i), not(_not) { }
	size_t index;
	bool not;

	bool operator< (const MapTriggerLogicBlock& block) const { return (index < block.index); }
	bool operator== (const MapTriggerLogicBlock& block) const { return (index == block.index); }
};

struct MapTrigger
{
	MapTrigger() : active(false), maxRunCount(0), timeValue(0) { }
	bool active;
	int maxRunCount;
	float timeValue;
	CStrW name, groupName;
	
	std::set<MapTriggerLogicBlock> logicBlocks;	//Indices of where "(" 's go before
	std::set<size_t> logicBlockEnds;  //Indices of where ")" 's come after
	std::list<MapTriggerCondition> conditions;
	std::list<MapTriggerEffect> effects;

	void AddLogicBlock(bool not) { logicBlocks.insert( MapTriggerLogicBlock(conditions.size(), not) ); }
	void AddLogicBlockEnd() { logicBlockEnds.insert( effects.size() ); }
};

struct MapTriggerGroup
{
	MapTriggerGroup() { }
	MapTriggerGroup(CStrW _name, CStrW _parentName) : name(_name), parentName(_parentName) {}

	std::list<MapTrigger> triggers;
	std::list<CStrW> childGroups;	//Indices of children
	CStrW name, parentName;
	
	bool operator== (const CStrW& _name) const { return (name == _name); }
	bool operator== (const MapTriggerGroup& group) const { return (name == group.name); }
};


struct copyIfRootChild
{
	copyIfRootChild(std::list<MapTriggerGroup>& groupList) : m_groupList(groupList) {}
	void operator() ( const MapTriggerGroup& group )
	{ 
		if ( group.parentName == L"Triggers" )
			m_groupList.push_back(group);
	}
private:
	void operator= (const copyIfRootChild& UNUSED(group)) const {}
	std::list<MapTriggerGroup>& m_groupList;
};

//Triggers used by engine

class CTrigger : public CJSObject<CTrigger>
{
	int m_runCount;
	CStrW m_conditionFuncString, m_effectFuncString, m_name, m_groupName;
	CScriptObject m_effectFunction;
	CScriptObject m_conditionFunction;

public:
	CTrigger();
	CTrigger(const CStrW& name, bool active, float delay, int maxRuns, const CStrW& condFunc, 
																	const CStrW& effectFunc);
	CTrigger(const CStrW& name, bool active, float delay, int maxRuns,
					CScriptObject& condFunc, CScriptObject& effectFunc);

	CTrigger& operator= (const CTrigger& trigger);
	~CTrigger();

	void SetFunctionBody(const CStrW& body);
	const CStrW& GetConditionString() { return m_conditionFuncString; }
	const CStrW& GetEffectString() { return m_effectFuncString; }
	const CStrW& GetName() { return m_name; }
	const CStrW& GetGroupName() { return m_groupName; }
	
	bool ShouldFire();
	//Returns false if trigger exceeds run count
	bool Fire();

	jsval Activate(JSContext* cx, uint argc, jsval* argv);
	jsval Deactivate(JSContext* cx, uint argc, jsval* argv);

	static JSBool Construct( JSContext* cx, JSObject* obj, uint argc, jsval* argv, jsval* rval );
	static void ScriptingInit();

	JSContext* m_cx;
	
	bool m_active;
	//Changeable by Atlas or needed by manager, no additional affects required
	int m_maxRunCount;
	float m_timeLeft, m_timeDelay;
	
};


//*****Holds information specifying how conditions and effects should look like in atlas******

struct TriggerParameter
{
	TriggerParameter() {}
	TriggerParameter(int _row, int _column) : row(_row), column(_column) {}
	void SetWindowData(const CStrW& _windowType, CStrW& windowPosition, CStrW& windowSize);
	//Sort parameters in the order they will be added to a sizer in atlas
	bool operator< ( const TriggerParameter& rhs ) const;
	
	CStrW name, windowType, inputType;
	int row, column, xPos, yPos, xSize, ySize, parameterOrder;
};

class CTriggerSpec
{
public:
	CTriggerSpec() {}
	~CTriggerSpec() {}
	
	void AddParameter(const TriggerParameter& param) { parameters.insert(param); }
	const std::set<TriggerParameter>& GetParameters() const { return parameters; }
	
	CStrW displayName, functionName;

private:
	
	//Sorted by parameter order to make things easy for Atlas
	std::set<TriggerParameter> parameters;
};
typedef CTriggerSpec CTriggerEffect;
typedef CTriggerSpec CTriggerCondition;


/******************************Trigger manager******************************/

class CTriggerManager : public Singleton<CTriggerManager>
{
public:
	typedef std::map<CStrW, CTrigger*>::iterator TriggerIter ;

	CTriggerManager();
	~CTriggerManager();
	
	//Returns false on detection of error
	bool LoadXML( const CStr& filename );
	void Update(float delta);

	//Add simulation trigger (probably only called from JS)
	void AddTrigger(CTrigger* trigger);
	void AddTrigger(MapTriggerGroup& group, const MapTrigger& trigger);
	void AddGroup(const MapTriggerGroup& group);
	void SetAllGroups(const std::list<MapTriggerGroup>& groups);
	void DestroyEngineTriggers();

	const std::list<CTriggerCondition>& GetAllConditions() const { return m_ConditionSpecs; }
	const std::list<CTriggerEffect>& GetAllEffects() const { return m_EffectSpecs; }
	const std::list<MapTriggerGroup>& GetAllTriggerGroups() const { return m_GroupList; }
	std::vector<std::wstring> GetTriggerChoices(const std::wstring& name);

	std::map<CStrW, CTrigger*> m_TriggerMap;	//Simulation triggers - used in engine
	
private:
	bool loadTriggerSpec( XMBElement condition, CXeromyces& XeroFile, bool isCondition );
	
	//Contains choices for trigger choice box parameters, with key = spec.funcName+paramName
	std::map<std::wstring, std::vector<std::wstring> > m_TriggerChoices;
	
	//Holds information which descibes trigger layout in atlas
	std::list<MapTriggerGroup> m_GroupList;		
	std::list<CTriggerCondition> m_ConditionSpecs;
	std::list<CTriggerEffect> m_EffectSpecs;

};

#define g_TriggerManager CTriggerManager::GetSingleton()

#endif	//ifndef TRIGGERMANAGER_INCLUDED
