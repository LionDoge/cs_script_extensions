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
#include "ehandle.h"
#include "entity/cpointscript.h"
#include "sigutils.h"
#include "gameconfig.h"
#include "scriptcommon.h"
#include "plugin.h"
#include "ehandle.h"

extern LoggingChannelID_t g_logChanScript;

SH_DECL_HOOK0_void(CCSScript_EntityScript, InitializeFunctionTemplates, SH_NOATTRIB, false);

bool CSScriptExtensionsSystem::ResolveSigs(CGameConfig* gameConfig)
{
	RESOLVE_SIG(gameConfig, "CSScript_RegisterFunctionTemplate", m_pfnRegisterInstanceTemplate)
	RESOLVE_SIG(gameConfig, "CSScript_SwitchContext", m_pfnSwitchScriptContext)
	RESOLVE_SIG(gameConfig, "CSScript_GetCurrentScriptContext", m_pfnGetCurrentCSScript)
	RESOLVE_SIG(gameConfig, "CSScript_AssignEntityToObject", m_pfnAssignEntityToObject)
	RESOLVE_SIG(gameConfig, "CSScript_CreateEntityObjectFromTemplate", m_pfnCreateEntintyObjectFromTemplate)
	RESOLVE_SIG(gameConfig, "CSScript_RunScript", m_pfnRunScript)

	return true;
}

CSScriptExtensionsSystem* CSScriptExtensionsSystem::GetInstance()
{
	if (m_instance == nullptr)
		m_instance = new CSScriptExtensionsSystem();

	return m_instance;
}

bool CSScriptExtensionsSystem::Initialize(CGameConfig* gameConfig)
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
		m_registerTemplatesHook = SH_ADD_DVPHOOK(CCSScript_EntityScript, InitializeFunctionTemplates, vtable, SH_MEMBER(this, &CSScriptExtensionsSystem::OnScriptInstanceRegisterTemplates), true);
	g_logChanScript = LoggingSystem_FindChannel("cs_script");
	return true;
}


CSScriptExtensionsSystem::~CSScriptExtensionsSystem()
{
	if (m_registerTemplatesHook != -1)
	{
		SH_REMOVE_HOOK_ID(m_registerTemplatesHook);
		m_registerTemplatesHook = -1;
	}
}

void CSScriptExtensionsSystem::IncludeFunctions(const std::string& templateName,
	std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions)
{
	auto vec = std::vector<ScriptFunctionInfo>();
	vec.reserve(functions.size());
	for (const auto& pair : functions)
	{
		vec.emplace_back(pair.first, pair.second);
	}
	m_registeredFunctions[templateName] = vec;
}

void CSScriptExtensionsSystem::RegisterCustomFunctionTemplate(const std::string& templateName,
	std::initializer_list<std::pair<std::string, v8::FunctionCallback>> functions, unsigned int internalFields,
	const std::optional<v8::FunctionCallback>& constructor, const std::optional<std::string_view>& inheritFrom)
{	
	auto vec = std::vector<ScriptFunctionInfo>();
	vec.reserve(functions.size());
	for (const auto& pair : functions)
	{
		vec.emplace_back(pair.first, pair.second);
	}

	ScriptCustomTemplateInfo info {
		.functions = std::move(vec),
		.internalFields = internalFields,
		.constructor = constructor,
		.inheritObject = inheritFrom
	};
	m_customFunctionTemplates[templateName] = std::move(info);
}

void CSScriptExtensionsSystem::RegisterCustomFunctionTemplate(void (*callback)(CCSBaseScript*))
{
	m_functionTemplateInitializers.push_back(callback);
}

CEntityInstance* CSScriptExtensionsSystem::GetEntityInstanceFromScriptObject(v8::Local<v8::Object> obj)
{
	if (obj->InternalFieldCount() != 3) {
		return nullptr;
	}

	// TODO: we don't know the Entity marker to check for, this will probably explode if we pass something else.
	// Note: The domain marker is a pointer to a value that evaluates to the type that's defined by 'ScriptHandleType'.
	uint32_t* typeMarker = static_cast<uint32_t*>(obj->GetAlignedPointerFromInternalField(0));
	CSScriptHandle* entPointer = static_cast<CSScriptHandle*>(obj->GetAlignedPointerFromInternalField(1));
	if (!entPointer)
	{
		Msg("[cs_script_extensions] Failed to get entity pointer from object! (Field 1 of object doesn't exist)");
		return nullptr;
	}
	return entPointer->handle.Get();
}

CCSBaseScript* CSScriptExtensionsSystem::GetCurrentCsScriptInstance()
{
	return m_pfnGetCurrentCSScript();
}

v8::Local<v8::Object> CSScriptExtensionsSystem::CreateEntityObjectAuto(CEntityInstance* ent)
{
	return m_pfnAssignEntityToObject(ent);
}

v8::Local<v8::Object> CSScriptExtensionsSystem::CreateEntityObjectFromTemplate(const CGlobalSymbol& templateName, CEntityInstance* entity)
{
	return m_pfnCreateEntintyObjectFromTemplate(templateName, entity);
}

void CSScriptExtensionsSystem::InvokeCallbacks(const char* callbackName, int argc, v8::Local<v8::Value> argv[])
{
	VPROF("CSScriptExtensionsSystem::InvokeCallbacks");

	auto isolate = v8::Isolate::GetCurrent();
	for (CEntityInstance* scriptEnt : GetScripts())
	{
		v8::HandleScope handleScope(isolate);
		if (const auto script = GetScriptFromEntity(scriptEnt))
			script->InvokeCallback(callbackName, argc, argv);
	}
}

void CSScriptExtensionsSystem::ScriptRegisterFunctionTemplate(CCSBaseScript* script, const char* name, const v8::Local<v8::FunctionTemplate>& functionTemplate)
{
	m_pfnRegisterInstanceTemplate(script, name, functionTemplate);
}

void CSScriptExtensionsSystem::RunScriptString(CCSBaseScript* script, const char* path, const char* scriptData)
{
	m_pfnRunScript(script, path, scriptData);
}

void CSScriptExtensionsSystem::OnScriptInstanceRegisterTemplates()
{
	CCSScript_EntityScript* script = META_IFACEPTR(CCSScript_EntityScript);
	auto isolate = v8::Isolate::GetCurrent();

	for (const auto& [name, funcInfos] : m_registeredFunctions)
	{
		// get the already registered template
		auto tem = script->GetFunctionTemplate(name.c_str());
		if (tem && !tem->IsEmpty())
		{
			auto prototypeTemplate = tem->Get(isolate)->PrototypeTemplate();
			for (const auto& funcInfo : funcInfos)
			{
				RegisterNewFunction(prototypeTemplate, funcInfo);
				Log_Msg(g_logChanScript, "Registered script extension function %s.%s\n", name.c_str(), funcInfo.name.c_str());
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
			templateInfo.constructor.has_value() ? *templateInfo.constructor : V8FakeObjectConstructorCallback
		);
		tpl->SetClassName(v8::String::NewFromUtf8(isolate, templateName.c_str()).ToLocalChecked());
		tpl->InstanceTemplate()->SetInternalFieldCount(static_cast<int>(templateInfo.internalFields));

		for (const auto& funcInfo : templateInfo.functions)
		{
			RegisterNewFunction(tpl->PrototypeTemplate(), funcInfo);
			Log_Msg(g_logChanScript, "Registered script custom template function %s.%s\n", templateName.c_str(), funcInfo.name.c_str());
		}

		if (templateInfo.inheritObject.has_value())
		{
			auto inheritTpl = script->GetFunctionTemplate(templateInfo.inheritObject.value().data());
			if (inheritTpl && !inheritTpl->IsEmpty())
			{
				tpl->Inherit(inheritTpl->Get(isolate));
			}
		}
		script->AddFunctionTemplate(templateName.c_str(), tpl);
	}

	RETURN_META(MRES_IGNORED);
}

void CSScriptExtensionsSystem::RegisterNewFunction(v8::Local<v8::ObjectTemplate> prototypeTemplate, const ScriptFunctionInfo& funcInfo)
{
	auto isolate = v8::Isolate::GetCurrent();
	auto name = v8::String::NewFromUtf8(isolate, funcInfo.name.c_str()).ToLocalChecked();
	auto funcTemplate = v8::FunctionTemplate::New(isolate, funcInfo.callback);
	prototypeTemplate->Set(name, funcTemplate);
}

void CSScriptExtensionsSystem::SwitchScriptContext(CCSBaseScript* script)
{
	m_pfnSwitchScriptContext(script);
}

std::vector<CEntityInstance*> CSScriptExtensionsSystem::GetScripts() {
	std::vector<CEntityInstance*> vecInstances;
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
}
class CCSPointScriptEntity {};
CCSScript_EntityScript* CSScriptExtensionsSystem::GetScriptFromEntity(CEntityInstance* ent)
{
	if (!ent)
		return nullptr;
	/*if (dynamic_cast<CCSPointScriptEntity*>(ent) == nullptr)
		return nullptr;*/

	// scans a memory range relative to entity, trying to find rtti of csscript, and caches the result.
#ifdef _WIN32
	if (_csScriptOffset == -1)
	{
		for (int i = 0; i < 0x800; i += sizeof(void*))
		{
			if (!modules::server->IsAddressInRange(*(void**)((unsigned char*)ent + i)))
				continue;

			DummyBase* obj = (DummyBase*)((unsigned char*)ent + i);
			if (V_stristr(typeid(*obj).name(), "CCSScript") != nullptr)
			{
				_csScriptOffset = i;
				break;
			}
		}
		if (_csScriptOffset == -1)
		{
			MsgCrit("!!!!! Failed to find CCSScript_EntityScript offset!\n");
			return nullptr;
		}
	}

	return reinterpret_cast<CCSScript_EntityScript*>((unsigned char*)ent + _csScriptOffset);
#else
	// TODO: linux impl
	return reinterpret_cast<CCSScript_EntityScript*>((unsigned char*)ent+0x798);
#endif
}

CSScriptHeader* CSScriptExtensionsSystem::GetScriptHeaderFromEntity(CEntityInstance* ent)
{
	if (!ent)
		return nullptr;
	if (dynamic_cast<CCSPointScriptEntity*>(ent) == nullptr)
		return nullptr;

	return reinterpret_cast<CSScriptHeader*>((unsigned char*)GetScriptFromEntity(ent) - 0x18);
}
