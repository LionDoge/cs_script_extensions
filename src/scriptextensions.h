#pragma once
#include <string_view>
#include <map>
#include "v8-function-callback.h"
#include "v8-template.h"
#include "module.h"
#include "csscript.h"
#include "safetyhook.hpp"
#include "gameconfig.h"

typedef void* (FASTCALL* funcRegisterV8InstanceTemplate_t)(CCSBaseScript* script, const char* name, v8::Local<v8::FunctionTemplate> funcTemplate);
typedef CEntityInstance* (FASTCALL* funcGetEntityInstanceFromCSScriptArg_t)(void* ths);
typedef CCSBaseScript* (FASTCALL* funcGetCurrentCssScript_t)();
typedef	v8::Local<v8::Object> (FASTCALL* funcScriptAssignEntityToObject_t)(CUtlSymbolLarge* name, CEntityInstance* ent);
typedef	v8::Local<v8::Object> (FASTCALL* funcScriptAssignEntAuto_t)(CEntityInstance* ent);
typedef void* (FASTCALL* funcConstructScriptAutoList_t)(bool unk);
typedef void(FASTCALL* funcSwitchScriptContext_t)(CCSScript_EntityScript* script);

// Used to hold data in the function map
class ScriptFunctionInfo {
public:
	std::string name; // Name of the function as seen from script.
	std::string scopeName; // Matters for logging only. The class instance e.g. 'point_script'
	v8::FunctionCallback callback;
};

// Used only for initialization (separate className)
class ScriptFunctionDetails : public ScriptFunctionInfo {
public:
	std::string className; // The domain of the function e.g. 'Domain', 'Entity'

	ScriptFunctionDetails(
		const std::string& _name,
		const std::string& _scopeName,
		std::string _className,
		v8::FunctionCallback _callback
	) : ScriptFunctionInfo{.name = _name, .scopeName = _scopeName, .callback = _callback }, className(
		    std::move(_className)) {}
};

struct ScriptCallbackInfo {
	v8::Global<v8::Function> callback;
	v8::Global<v8::Context> context; // this needed? maybe GetCreationContext on callback could work too.
};

typedef std::unordered_map<std::string, ScriptCallbackInfo> ScriptCallbackMap;

class CSScriptExtensionsSystem {
public:
	CSScriptExtensionsSystem() = default;
	CSScriptExtensionsSystem(const std::initializer_list<ScriptFunctionDetails>& initList);
	bool Initialize(CGameConfig* gameConfig);

	// Registers a function to be callable from cs_script, valid for newly created script instances.
	// Registering an existing name will cause the original function to be overwritten.
	bool AddNewFunction(
		const std::string& instanceName,
		const std::string& funcName,
		const std::string& scopeName, // only matters for logging
		v8::FunctionCallback callback
	);

	// Can be used by V8 callbacks.
	static CEntityInstance* GetEntityInstanceFromScriptObject(v8::Local<v8::Object> obj);
	static CCSBaseScript* GetCurrentCsScriptInstance();

	// [Valve impl] Creates a V8 object representing the entity instance.
	// NOTE: Internally this calls an inner function that does most of the work
	// however this one automatically deduces which object template to use based on the actual entity type.
	// I haven't gotten that inner function working properly yet, but this will do unless there's need for more entity types.
	static v8::Local<v8::Object> CreateEntityObject(CEntityInstance* ent);
	// Get all script entities
	static std::vector<CEntityInstance*> GetScripts();
	static CCSScript_EntityScript* GetScriptFromEntity(CEntityInstance* ent);
	CSScriptHeader* GetScriptHeaderFromEntity(CEntityInstance* ent);
	//static v8::Local<v8::Object> AssignEntityToObject(v8::Local<v8::Object> obj, const char* name, CEntityInstance* ent);
	
	void Hook_RegisterInstanceTemplate(CCSBaseScript* script, const char* name, v8::Local<v8::FunctionTemplate> funcTemplate);
	// use internal hashmap for storing callbacks, (shared by game functions).
	void AddCallbackNative(CCSScript_EntityScript* script, const CUtlString&, v8::Local<v8::Function> func);
	/*void AddCallback(uint64_t scriptIdx, const char* callbackName, v8::Global<v8::Function>&& func, v8::Global<v8::Context>&& ctx);
	void InvokeCallbacks(const char* cbName);*/
	void InvokeNativeCallbacks();
	void InvokeNativeCallback(const char* callbackName, int argc, v8::Local<v8::Value> argv[]);
	void InvokeNativeCallbackForScript(CCSScript_EntityScript* script, const char* callbackName, int argc, v8::Local<v8::Value> argv[]);

	void SwitchScriptContext(CCSScript_EntityScript* script);
	// maps CSPointScript entity handles to their registered callbacks
	// indexed by the global script index from CCSScript_EntityScript

	void AddUserMessageCallback(NetworkMessageId msgId, v8::Global<v8::Function> callback);
protected:
	// Should be called by a hook when a script is being created.
	void OnScriptInstanceRegisterInstance(CCSBaseScript* script, v8::Local<v8::ObjectTemplate> functionTemplate, const char* name);
	void RegisterNewFunction(v8::Local<v8::ObjectTemplate> targetFuncTemplate, const ScriptFunctionInfo& funcInfo);
	//inline static std::unordered_map<uint64_t, ScriptCallbackMap> m_scriptCallbacks{};
	//std::unordered_map<NetworkMessageId, v8::Global<v8::Function>> m_userMessageCallbacks{};
private:
	bool ResolveSigs(CGameConfig* gameConfig);
	std::map<std::string, std::vector<ScriptFunctionInfo>> m_registeredFunctions;
	// functions
	inline static funcRegisterV8InstanceTemplate_t m_pfnRegisterInstanceTemplate;
	inline static funcGetEntityInstanceFromCSScriptArg_t m_pfnGetEntityInstanceFromCSScriptArg;
	inline static funcGetCurrentCssScript_t m_pfnGetCurrentCSScript;
	inline static funcScriptAssignEntAuto_t m_pfnScriptAssignEntAuto;
	//inline static funcConstructScriptAutoList_t m_pfnConstructScriptAutoList;
	inline static funcSwitchScriptContext_t m_pfnSwitchScriptContext;

	// hooks
	SafetyHookInline m_pHookRegisterInstanceTemplate{};

	// saved callbacks. TODO map per script instance
	
};