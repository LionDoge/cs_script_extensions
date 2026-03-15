/**
 * =============================================================================
 * cs_script_extensions
 * Copyright (C) 2026 liondoge
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <unordered_map>
#include "v8-function-callback.h"
#include "v8-template.h"
#include "module.h"
#include "csscript.h"
#include "ehandle.h"
#include "entity/cpointscript.h"

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

class ScriptExtensions {
public:
	ScriptExtensions() = default;
	~ScriptExtensions();

	ScriptExtensions(ScriptExtensions& other) = delete;
	void operator=(const ScriptExtensions&) = delete;
	static ScriptExtensions* GetInstance();

	bool Initialize(CGameConfig* gameConfig);

	// Add functions to an existing function template.
	// Use this if you want to extend existing templates like 'Entity' or 'Domain'
	void IncludeFunctions(
		const std::string& templateName,
		std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions
	);

	// Register a custom object that can be used inside scripts, and have its own methods.
	// Remember to not use the same name as the native function, unless you really want to override them!
	// For adding more functions to an existing template, check IncludeFunctions instead.
	void RegisterCustomFunctionTemplate(
		const std::string& templateName,
		std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions, unsigned int internalFields,
		const std::optional<v8::FunctionCallback>& constructor = std::nullopt,
		const std::optional<std::string_view>& inheritFrom = std::nullopt
	);

	// More advanced API where the custom template registration can be fully controlled by the user.
	void RegisterCustomFunctionTemplate(void (*callback)(CCSBaseScript*));

	static CEntityHandle GetEntityHandleFromScriptObject(v8::Local<v8::Object> obj);
	// This is safe to call inside script callback functions, should always return the current running script.
	static CCSBaseScript* GetCurrentCsScriptInstance();
	// Native functions check for this! V8 context is not enough! Use when invoking callbacks from your own code.
	void SwitchScriptContext(CCSBaseScript* script);

	// Creates a V8 object representing the entity instance from a specific template (can also be our types).
	// Using a signature is required since it also stores a global marker inside the internal field that gets checked by native functions.
	static v8::Local<v8::Object> CreateEntityObjectFromTemplate(const CGlobalSymbol& templateName, CEntityInstance* entity);

	// Creates a V8 object representing the entity instance.
	// Internally this calls an inner function that runs checks for a few hardcoded types.
	// NOTE: DO NOT USE FOR YOUR OWN CUSTOM REGISTERED ENTITY TYPES!
	static v8::Local<v8::Object> CreateEntityObjectAuto(CEntityInstance* ent);

	// Get all script entities in the map
	static std::vector<CPointScript*> GetScripts();

	// Gets the script component from a point_script entity.
	static CCSScript_EntityScript* GetScriptFromEntity(CEntityInstance* ent);
	CSScriptHeader* GetScriptHeaderFromEntity(CEntityInstance* ent);

	// Invoke named callback on all scripts, this doesn't let you retrieve return values yet, maybe later...
	// Depending on the needs you may want to manually iterate script entities and call InvokeCallback on each of them instead.
	void InvokeCallbacks(const char* callbackName, int argc, v8::Local<v8::Value> argv[]);
	
	// Register a function template with the script, just calls the game function.
	// This usually is done only during script initialization, but technically can be done at any time.
	void ScriptRegisterFunctionTemplate(CCSBaseScript* script, const char* name, const v8::Local<v8::FunctionTemplate>& functionTemplate);
	// Overwrite the data of this script with custom text contents, and runs it.
	// This behavior could change to run in the existing context, but it requires additional setup with the V8 inspector if we want REPL.
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
	// Global SourceHook for CSScript
	int m_registerTemplatesHook = -1;

protected:
	inline static ScriptExtensions* m_instance;
};