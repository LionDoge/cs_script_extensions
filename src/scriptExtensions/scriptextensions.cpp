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
#include "scriptextensions.h"
#include <vprof.h>
#include "entity/cpointscript.h"
#include "sigutils.h"
#include "gameconfig.h"
#include "scriptcommon.h"
#include "plugin.h"

SH_DECL_HOOK0_void(CCSScript_EntityScript, InitializeFunctionTemplates, SH_NOATTRIB, false);

bool ScriptExtensions::ResolveSigs(CGameConfig* gameConfig)
{
	RESOLVE_SIG(gameConfig, "CSScript_RegisterFunctionTemplate", m_pfnRegisterInstanceTemplate)
	RESOLVE_SIG(gameConfig, "CSScript_SwitchContext", m_pfnSwitchScriptContext)
	RESOLVE_SIG(gameConfig, "CSScript_GetCurrentScriptContext", m_pfnGetCurrentCSScript)
	RESOLVE_SIG(gameConfig, "CSScript_AssignEntityToObject", m_pfnAssignEntityToObject)
	RESOLVE_SIG(gameConfig, "CSScript_CreateEntityObjectFromTemplate", m_pfnCreateEntintyObjectFromTemplate)
	RESOLVE_SIG(gameConfig, "CSScript_RunScript", m_pfnRunScript)

	return true;
}

ScriptExtensions* ScriptExtensions::GetInstance()
{
	if (m_instance == nullptr)
		m_instance = new ScriptExtensions();

	return m_instance;
}

bool ScriptExtensions::Initialize(CGameConfig* gameConfig)
{
	if (!ResolveSigs(gameConfig))
	{
		MsgCrit("Failed to resolve one or more required signatures!\n");
		return false;
	}

	auto vtable = modules::server->FindVirtualTable("CCSScript_EntityScript", true);
	if (!vtable)
	{
		MsgCrit("Failed to find CCSScript_EntityScript vtable!\n");
		return false;
	}

	if (m_registerTemplatesHook == -1)
		m_registerTemplatesHook = SH_ADD_DVPHOOK(CCSScript_EntityScript, InitializeFunctionTemplates, vtable, SH_MEMBER(this, &ScriptExtensions::OnScriptInstanceRegisterTemplates), true);
	g_logChanScript = LoggingSystem_FindChannel("cs_script");
	return true;
}


ScriptExtensions::~ScriptExtensions()
{
	if (m_registerTemplatesHook != -1)
	{
		SH_REMOVE_HOOK_ID(m_registerTemplatesHook);
		m_registerTemplatesHook = -1;
	}
}

void ScriptExtensions::IncludeFunctions(const std::string& templateName,
	std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions)
{
	for (const auto& pair : functions)
	{
		m_registeredFunctions[templateName.c_str()].emplace_back(pair.first, pair.second);
	}
}

void ScriptExtensions::RegisterCustomFunctionTemplate(const std::string& templateName,
	std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions, unsigned int internalFields,
	ScriptTypeMarker* typeMarker, const std::optional<v8::FunctionCallback>& constructor, 
	const std::optional<std::string_view>& inheritFrom)
{
	if(!typeMarker)
	{
		MsgCrit("Failed to register custom function template %s, type marker is null!\n", templateName.c_str());
		return;
	}
	auto vec = std::vector<ScriptFunctionInfo>();
	vec.reserve(functions.size());
	for (const auto& pair : functions)
	{
		vec.emplace_back(pair.first, pair.second);
	}

	m_customFunctionTemplates.insert_or_assign(
		templateName.c_str(),
		ScriptCustomTemplateInfo{
			.functions = std::move(vec),
			.internalFields = internalFields,
			.typeMarker = typeMarker,
			.constructor = constructor,
			.inheritObject = inheritFrom
		}
	);
}

void ScriptExtensions::RegisterCustomFunctionTemplate(void (*callback)(CCSBaseScript*))
{
	m_functionTemplateInitializers.push_back(callback);
}

CEntityHandle ScriptExtensions::GetEntityHandleFromScriptObject(v8::Local<v8::Object> obj)
{
	if (obj->InternalFieldCount() != 3) {
		return {}; // default, invalid handle
	}

	// TODO: we don't know the Entity marker to check for, this will probably explode if we pass something else.
	// Note: The domain marker is a pointer to a value that evaluates to the type that's defined by 'ScriptHandleType'.
	uint32_t* typeMarker = static_cast<uint32_t*>(obj->GetAlignedPointerFromInternalField(0));
	CSScriptHandle* entPointer = static_cast<CSScriptHandle*>(obj->GetAlignedPointerFromInternalField(1));
	if (!entPointer)
	{
		PluginMsg("Failed to get entity pointer from object! (Field 1 of object doesn't exist)");
		return {};
	}
	return entPointer->handle;
}

CCSBaseScript* ScriptExtensions::GetCurrentCsScriptInstance()
{
	return m_pfnGetCurrentCSScript();
}

v8::Local<v8::Object> ScriptExtensions::CreateEntityObjectAuto(CEntityInstance* ent)
{
	return m_pfnAssignEntityToObject(ent);
}

v8::Local<v8::Object> ScriptExtensions::CreateEntityObjectFromTemplate(const CGlobalSymbol& templateName, CEntityInstance* entity)
{
	return m_pfnCreateEntintyObjectFromTemplate(templateName, entity);
}

std::vector<v8::Local<v8::Value>> ScriptExtensions::InvokeCallbacks(const char* callbackName, int argc, v8::Local<v8::Value> argv[])
{
	VPROF("CSScriptExtensionsSystem::InvokeCallbacks");

	auto isolate = v8::Isolate::GetCurrent();
	auto scripts = GetScripts();

	std::vector<v8::Local<v8::Value>> results;
	results.reserve(scripts.size());
	for (CPointScript* scriptEnt : scripts)
	{
		v8::HandleScope handleScope(isolate);
		if (const auto script = scriptEnt->GetScript())
			results.push_back(script->InvokeCallback(callbackName, argc, argv));
	}
	return results;
}

void ScriptExtensions::ScriptRegisterFunctionTemplate(CCSBaseScript* script, const char* name, const v8::Local<v8::FunctionTemplate>& functionTemplate)
{
	m_pfnRegisterInstanceTemplate(script, name, functionTemplate);
}

void ScriptExtensions::RunScriptString(CCSBaseScript* script, const char* path, const char* scriptData)
{
	m_pfnRunScript(script, path, scriptData);
}

void ScriptExtensions::OnScriptInstanceRegisterTemplates()
{
	CCSScript_EntityScript* script = META_IFACEPTR(CCSScript_EntityScript);
	auto isolate = v8::Isolate::GetCurrent();
	auto domainTemplate = script->GetFunctionTemplate("Domain");

	if (domainTemplate && !domainTemplate->IsEmpty())
	{
		auto prototypeTemplate = domainTemplate->Get(isolate)->PrototypeTemplate();
		auto name = v8::String::NewFromUtf8(isolate, "scriptExtensionsVersion").ToLocalChecked();
		prototypeTemplate->Set(name, v8::String::NewFromUtf8(isolate, g_ThisPlugin.GetVersion()).ToLocalChecked());
	}

	for (const auto& [name, funcInfos] : m_registeredFunctions)
	{
		// get the already registered template
		auto tem = script->GetFunctionTemplate(name);
		if (tem && !tem->IsEmpty())
		{
			auto prototypeTemplate = tem->Get(isolate)->PrototypeTemplate();
			for (const auto& funcInfo : funcInfos)
			{
				RegisterNewFunction(prototypeTemplate, funcInfo);
				Log_Debug(g_logChanScript, "Registered script extension function %s.%s\n", name.Get(), funcInfo.name.c_str());
			}
		}
	}

	for (const auto& initializer : m_functionTemplateInitializers)
	{
		if (initializer)
			initializer(script);
	}

	v8::HandleScope handleScope(isolate);

	for (const auto& [templateName, templateInfo] : m_customFunctionTemplates)
	{
		v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
			isolate,
			templateInfo.constructor.value_or(V8FakeObjectConstructorCallback)
		);
		tpl->SetClassName(v8::String::NewFromUtf8(isolate, templateName.Get()).ToLocalChecked());
		// +1 internal field for our type marker
		tpl->InstanceTemplate()->SetInternalFieldCount(static_cast<int>(templateInfo.internalFields) + 1);

		for (const auto& funcInfo : templateInfo.functions)
		{
			RegisterNewFunction(tpl->PrototypeTemplate(), funcInfo);
			Log_Debug(g_logChanScript, "Registered script custom template function %s.%s\n", templateName.Get(), funcInfo.name.c_str());
		}

		if (templateInfo.inheritObject.has_value())
		{
			auto inheritTpl = script->GetFunctionTemplate(templateInfo.inheritObject.value().data());
			if (inheritTpl && !inheritTpl->IsEmpty())
			{
				tpl->Inherit(inheritTpl->Get(isolate));
			}
		}
		script->AddFunctionTemplate(templateName.Get(), tpl);
	}

	RETURN_META(MRES_IGNORED);
}

void ScriptExtensions::RegisterNewFunction(v8::Local<v8::ObjectTemplate> prototypeTemplate, const ScriptFunctionInfo& funcInfo)
{
	auto isolate = v8::Isolate::GetCurrent();
	auto name = v8::String::NewFromUtf8(isolate, funcInfo.name.c_str()).ToLocalChecked();
	auto funcTemplate = v8::FunctionTemplate::New(isolate, funcInfo.callback);
	prototypeTemplate->Set(name, funcTemplate);
}

void ScriptExtensions::SwitchScriptContext(CCSBaseScript* script)
{
	m_pfnSwitchScriptContext(script);
}

std::vector<CPointScript*> ScriptExtensions::GetScripts() {
	std::vector<CPointScript*> vecInstances;
	vecInstances.reserve(1);
	EntityInstanceByClassIter_t iter(nullptr, "point_script");

	for (CPointScript* currentEnt = static_cast<CPointScript*>(iter.First());
		currentEnt != nullptr;
		currentEnt = static_cast<CPointScript*>(iter.Next()))
	{
		vecInstances.emplace_back(currentEnt);
	}
	return vecInstances;
}

namespace {
	struct DummyBase { virtual ~DummyBase() = default; };
	static size_t _csScriptOffset = -1;

	bool IsInSectionRange(Section* section, void* addr)
	{
		return (addr >= section->m_pBase && addr < (void*)((uintptr_t)section->m_pBase + section->m_iSize));
	}
}

CCSScript_EntityScript* ScriptExtensions::GetScriptFromEntity(CEntityInstance* ent)
{
	if (!ent)
		return nullptr;

	// scans a memory range relative to entity, trying to find rtti of csscript, and caches the result.
	// all of this is probably not 100% safe, if for example some garbage data happens to fall within the pointer range.
	// there are probably more safeguards for this method.
#ifdef _WIN32
	if (_csScriptOffset == -1)
	{
		auto roDataSection = modules::server->GetSection(".rdata");
		for (int i = 0; i < 0x800; i += sizeof(void*))
		{
			if (!IsInSectionRange(roDataSection, *(void**)((unsigned char*)ent + i)))
				continue;

			DummyBase* obj = nullptr;
			const char* typeName = "";
			__try
			{
				obj = (DummyBase*)((unsigned char*)ent + i);
				typeName = typeid(*obj).name();
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
				EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				// just in case, we might read some invalid memory and cause an access violation, we can just ignore it and continue scanning.
				continue;
			}

			if (V_stristr(typeName, "CCSScript") != nullptr)
			{
				_csScriptOffset = i;
				PluginMsg("Found CCSScript entity offset at: %x\n", _csScriptOffset);
				break;
			}
			
		}
		if (_csScriptOffset == -1)
		{
			MsgCrit("!!!!! Failed to find CCSScript_EntityScript offset!\n");
			return nullptr;
		}
	}
#else
	if (_csScriptOffset == -1)
	{
		auto roDataSection = modules::server->GetSection(".data.rel.ro");
		for (int i = 0; i < 0x800; i += sizeof(void*))
		{
			void* ptr = *(void**)((unsigned char*)ent + i);
			if (!IsInSectionRange(roDataSection, ptr))
				continue;

			DummyBase* obj = (DummyBase*)((unsigned char*)ent + i);
			if (V_stristr(typeid(*obj).name(), "CCSScript") != nullptr)
			{
				_csScriptOffset = static_cast<size_t>(i);
				Msg("Found CCSScript entity offset at: %x\n", _csScriptOffset);
				break;
			}
		}
		if (_csScriptOffset == -1)
		{
			MsgCrit("!!!!! Failed to find CCSScript_EntityScript offset!\n");
			return nullptr;
		}
	}
#endif
	return reinterpret_cast<CCSScript_EntityScript*>((unsigned char*)ent + _csScriptOffset);
}

CCSPointScriptEntity* ScriptExtensions::GetPointScriptComponent(CEntityInstance* ent)
{
	if (!ent)
		return nullptr;

	return reinterpret_cast<CCSPointScriptEntity*>((unsigned char*)GetScriptFromEntity(ent) - 0x18);
}
