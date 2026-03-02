#include "scriptextensions.h"

#include <vprof.h>
#include "v8.h"
#include "ehandle.h"
#include "sourcehook.h"
#include "utlvector.h"
#include "entity/cpointscript.h"
#include "sigutils.h"
#include "scriptExtensions/userMessagesScriptExt.h"
#include "gameconfig.h"
#include "scriptcommon.h"

extern LoggingChannelID_t g_logChanScript;
// HACK!
extern CSScriptExtensionsSystem* g_scriptExtensions;
static void Hook_RegisterInstanceTemplateA(
	CCSBaseScript* script,
	const char* name,
	v8::Local<v8::FunctionTemplate> funcTemplate
) {
	g_scriptExtensions->Hook_RegisterFunctionTemplate(script, name, funcTemplate);
}

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
	m_pHookRegisterInstanceTemplate = safetyhook::create_inline(reinterpret_cast<void*>(m_pfnRegisterInstanceTemplate), reinterpret_cast<void*>(Hook_RegisterInstanceTemplateA));
	g_logChanScript = LoggingSystem_FindChannel("cs_script");
	return true;
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
	CSScriptEntityHandle* entPointer = static_cast<CSScriptEntityHandle*>(obj->GetAlignedPointerFromInternalField(1));
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

void CSScriptExtensionsSystem::Hook_RegisterFunctionTemplate(
	CCSBaseScript* script, 
	const char* name, 
	v8::Local<v8::FunctionTemplate> funcTemplate
)
{
	OnScriptInstanceRegisterFunctionTemplate(script, funcTemplate->PrototypeTemplate(), name);
	m_pHookRegisterInstanceTemplate.call(script, name, funcTemplate);
}

//void CSScriptExtensionsSystem::AddCallback(uint64_t scriptIdx, const char* callbackName, v8::Global<v8::Function>&& func, v8::Global<v8::Context>&& ctx)
//{
//    if (callbackName == nullptr)
//        return;
//
//    // Get (or create) the callback map for this script index.
//    auto& callbackMap = m_scriptCallbacks[scriptIdx];
//    callbackMap[std::string(callbackName)] = { std::move(func), std::move(ctx) };
//}
//
//void CSScriptExtensionsSystem::InvokeCallbacks(const char* cbName)
//{
//	auto script = CSScriptExtensionsSystem::GetCurrentCsScriptInstance();
//	int idx = 0;
//	for(CEntityInstance* scriptEnt : GetScripts())
//	{
//		auto script = GetScriptFromEntity(scriptEnt);
//		if (!m_scriptCallbacks.contains(script->globalScriptIndex))
//			continue;
//
//		const auto& callbackMap = m_scriptCallbacks.at(script->globalScriptIndex);
//		
//		if(!callbackMap.contains(cbName))
//			continue;
//
//		g_scriptExtensions.SwitchScriptContext(script);
//		auto isolate = v8::Isolate::GetCurrent();
//		v8::HandleScope handleScope(isolate);
//
//		const auto& cbInfo = callbackMap.at(cbName);
//		auto context = cbInfo.context.Get(isolate);
//		context->Enter();
//
//		v8::TryCatch tryCatch(isolate);
//		v8::Local<v8::Value> args[] = { v8::Number::New(isolate, idx) };
//		auto func = cbInfo.callback.Get(isolate);
//		func->Call(context, context->Global(), 1, args);
//
//		if(tryCatch.HasCaught())
//		{
//			v8::String::Utf8Value errorStr(isolate, tryCatch.Exception());
//			Log_Warning(g_logChanScript, "InvokeCallbacks: Callback %d threw an exception: %s\n", idx, *errorStr);
//		}
//		
//		context->Exit();
//		idx++;
//	}
//}

//void CSScriptExtensionsSystem::TestInvokeAllCallbacks()
//{
//	auto isolate = v8::Isolate::GetCurrent();
//	for (CEntityInstance* scriptEnt : GetScripts())
//	{
//		v8::HandleScope handleScope(isolate);
//		auto script = GetScriptFromEntity(scriptEnt);
//		auto context = script->context.Get(isolate);
//		context->Enter();
//		SwitchScriptContext(script);
//		FOR_EACH_HASHTABLE(script->callbackMap, i)
//		{
//			auto key = script->callbackMap.Key(i);
//			auto val = script->callbackMap.Element(i);
//			auto func = val->Get(isolate);
//
//			v8::TryCatch tryCatch(isolate);
//			v8::Local<v8::Value> args[3];
//			args[0] = v8::String::NewFromUtf8(isolate, "hello").ToLocalChecked();
//			args[1] = v8::Number::New(isolate, 42);
//			args[2] = v8::Boolean::New(isolate, true);
//			func->Call(context, context->Global(), 3, args);
//			if (tryCatch.HasCaught())
//			{
//				v8::Local<v8::Value> exception = tryCatch.Exception();
//				v8::String::Utf8Value exception_str(isolate, exception);
//				Log_Warning(g_logChanScript, "Exception: %s\n", *exception_str);
//			}
//		}
//		context->Exit();
//	}
//}

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
	m_pHookRegisterInstanceTemplate.call(script, name, functionTemplate);
}

void CSScriptExtensionsSystem::RunScriptString(CCSBaseScript* script, const char* path, const char* scriptData)
{
	m_pfnRunScript(script, path, scriptData);
}

void CSScriptExtensionsSystem::OnScriptInstanceRegisterFunctionTemplate(CCSBaseScript* script, v8::Local<v8::ObjectTemplate> prototypeTemplate, const char* name)
{
	std::string instanceNameStr(name);
	if (!m_registeredFunctions.contains(instanceNameStr))
		return;
	auto& funcs = m_registeredFunctions[instanceNameStr];
	for (const auto& funcInfo : funcs)
	{
		RegisterNewFunction(prototypeTemplate, funcInfo);
		Log_Msg(g_logChanScript, "Registered script extension function %s.%s\n", instanceNameStr.c_str(), funcInfo.name.c_str());
	}

	for (const auto& initializer : m_functionTemplateInitializers)
	{
		if (initializer)
			initializer(script);
	}

	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	for (const auto& [templateName, templateInfo] : m_customFunctionTemplates)
	{
		// kinda ugly way before we do hooking onto post-script initialization method separately.
		if (script->IsFunctionTemplateRegistered(templateName.c_str()))
			continue;

		v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
			isolate,
			templateInfo.constructor.has_value() ? *templateInfo.constructor : V8FakeObjectConstructorCallback
		);
		tpl->SetClassName(v8::String::NewFromUtf8(isolate, templateName.c_str()).ToLocalChecked());
		tpl->InstanceTemplate()->SetInternalFieldCount(static_cast<int>(templateInfo.internalFields));

		for (const auto& funcInfo : templateInfo.functions)
		{
			RegisterNewFunction(tpl->PrototypeTemplate(), funcInfo);
		}
		// inheritance won't really work at this stage, but this whole thing will be moved out later, as per the comment above.
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
	if (dynamic_cast<CCSPointScriptEntity*>(ent) == nullptr)
		return nullptr;

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
	}

	return reinterpret_cast<CCSScript_EntityScript*>((unsigned char*)ent + _csScriptOffset);
#else
	// TODO: linux impl
	return reinterpret_cast<CCSScript_EntityScript*>((unsigned char*)ent+0x4C0);
#endif
}

CSScriptHeader* CSScriptExtensionsSystem::GetScriptHeaderFromEntity(CEntityInstance* ent)
{
	if (!ent)
		return nullptr;
	if (dynamic_cast<CCSPointScriptEntity*>(ent) == nullptr)
		return nullptr;

	return reinterpret_cast<CSScriptHeader*>((unsigned char*)ent + 0x4A8);
}
