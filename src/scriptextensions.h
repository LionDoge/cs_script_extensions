#pragma once
#include <unordered_map>
#include "v8-function-callback.h"
#include "v8-template.h"
#include "module.h"
#include "csscript.h"
#include "safetyhook.hpp"

struct ScriptFunctionInfo {
	std::string name; // Name of the function as seen from script.
	v8::FunctionCallback callback;
};

struct ScriptCustomTemplateInfo {
	std::vector<ScriptFunctionInfo> functions;
	unsigned int internalFields;
	std::optional<v8::FunctionCallback> constructor;
	std::optional<std::string_view> inheritObject;
};

class CSScriptExtensionsSystem {
public:
	CSScriptExtensionsSystem() = default;
	~CSScriptExtensionsSystem();

	CSScriptExtensionsSystem(CSScriptExtensionsSystem& other) = delete;
	void operator=(const CSScriptExtensionsSystem&) = delete;
	static CSScriptExtensionsSystem* GetInstance();

	bool Initialize(CGameConfig* gameConfig);

	// Add functions to an existing function template.
	void IncludeFunctions(
		const std::string& templateName,
		std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions
	);

	// Register a custom object that can be used inside scripts and have its own methods
	// The passed callback is expected to create a function template and register it inside the script.
	// This will be run each time a script tries to register a function template. TODO hook onto script creation only instead.
	void RegisterCustomFunctionTemplate(
		const std::string& templateName,
		std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions, unsigned int internalFields,
		const std::optional<v8::FunctionCallback>& constructor = std::nullopt,
		const std::optional<std::string_view>& inheritFrom = std::nullopt
	);

	// More advanced API where the registration can be fully controlled by the user.
	void RegisterCustomFunctionTemplate(void (*callback)(CCSBaseScript*));

	static CEntityInstance* GetEntityInstanceFromScriptObject(v8::Local<v8::Object> obj);
	static CCSBaseScript* GetCurrentCsScriptInstance();
	// Native functions check for this. V8 context is not enough! Use when calling from the "outside" (i.e. callbacks).
	void SwitchScriptContext(CCSBaseScript* script);

	// Creates a V8 object representing the entity instance from a specific template (can also be our types).
	// Using a signature is required since it also stores a global marker inside the internal field that gets checked by native functions.
	static v8::Local<v8::Object> CreateEntityObjectFromTemplate(const CGlobalSymbol& templateName, CEntityInstance* entity);

	// NOTE: DO NOT USE FOR CUSTOM ENTITY TYPES!
	// Creates a V8 object representing the entity instance.
	// Internally this calls an inner function that runs checks for a few types of entities being passed.
	static v8::Local<v8::Object> CreateEntityObjectAuto(CEntityInstance* ent);

	// Get all script entities in the map
	static std::vector<CEntityInstance*> GetScripts();

	static CCSScript_EntityScript* GetScriptFromEntity(CEntityInstance* ent);
	CSScriptHeader* GetScriptHeaderFromEntity(CEntityInstance* ent);

	void Hook_RegisterFunctionTemplate(CCSBaseScript* script, const char* name, v8::Local<v8::FunctionTemplate> funcTemplate);
	// Invoke named callback on all scripts, this doesn't let you retrieve return values yet, maybe later...
	void InvokeCallbacks(const char* callbackName, int argc, v8::Local<v8::Value> argv[]);
	
	// just calls the game function
	void ScriptRegisterFunctionTemplate(CCSBaseScript* script, const char* name, const v8::Local<v8::FunctionTemplate>& functionTemplate);
	void RunScriptString(CCSBaseScript* script, const char* path, const char* scriptData);

protected:
	// Should be called by a hook when a script is being created.
	void OnScriptInstanceRegisterTemplates();
	void RegisterNewFunction(v8::Local<v8::ObjectTemplate> prototypeTemplate, const ScriptFunctionInfo& funcInfo);

private:
	bool ResolveSigs(CGameConfig* gameConfig);

	std::unordered_map<std::string, std::vector<ScriptFunctionInfo>> m_registeredFunctions;
	// Used for manual registration of function templates, useful for advanced users that want full control of the process.
	std::vector<void (*)(CCSBaseScript*)> m_functionTemplateInitializers;
	std::unordered_map<std::string, ScriptCustomTemplateInfo> m_customFunctionTemplates;
	

	// function pointers
	typedef void* (FASTCALL* funcRegisterV8InstanceTemplate_t)(CCSBaseScript* script, const char* name, v8::Local<v8::FunctionTemplate> funcTemplate);
	typedef CEntityInstance* (FASTCALL* funcGetEntityInstanceFromCSScriptArg_t)(void* ths);
	typedef CCSBaseScript* (FASTCALL* funcGetCurrentCssScript_t)();
	typedef	v8::Local<v8::Object>(FASTCALL* funcScriptAssignEntAuto_t)(CEntityInstance* ent);
	typedef	v8::Local<v8::Object>(FASTCALL* funcCreateEntityObjectFromTemplate_t)(const CGlobalSymbol& name, CEntityInstance* ent);
	typedef void(FASTCALL* funcSwitchScriptContext_t)(CCSBaseScript* script);
	typedef void(FASTCALL* funcRunScriptString)(CCSBaseScript* script, const char* path, const char* scriptData);

	inline static funcRegisterV8InstanceTemplate_t m_pfnRegisterInstanceTemplate;
	inline static funcGetEntityInstanceFromCSScriptArg_t m_pfnGetEntityInstanceFromCSScriptArg;
	inline static funcGetCurrentCssScript_t m_pfnGetCurrentCSScript;
	inline static funcScriptAssignEntAuto_t m_pfnAssignEntityToObject;
	inline static funcCreateEntityObjectFromTemplate_t m_pfnCreateEntintyObjectFromTemplate;
	inline static funcSwitchScriptContext_t m_pfnSwitchScriptContext;
	inline static funcRunScriptString m_pfnRunScript;

	// hooks
	// SafetyHookInline m_pHookRegisterInstanceTemplate{};
	// Global SourceHook for CSScript
	int m_registerTemplatesHook = -1;

protected:
	inline static CSScriptExtensionsSystem* m_instance;
};