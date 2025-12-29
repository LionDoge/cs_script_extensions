#include "scriptextensions.h"

#include <vprof.h>

#include "v8.h"
#include "ehandle.h"
#include "sourcehook.h"
#include "utlvector.h"
#include "entity/cpointscript.h"
#include "sigutils.h"
#include "scriptExtensions/userMessagesScriptExt.h"

extern LoggingChannelID_t g_logChanScript;
// HACK!
extern CSScriptExtensionsSystem g_scriptExtensions;
static void Hook_RegisterInstanceTemplateA(
	CCSBaseScript* script,
	const char* name,
	v8::Local<v8::FunctionTemplate> funcTemplate
) {
	g_scriptExtensions.Hook_RegisterInstanceTemplate(script, name, funcTemplate);
}

CSScriptExtensionsSystem::CSScriptExtensionsSystem(const std::initializer_list<ScriptFunctionDetails>& initList)
{
	for (const auto& entry : initList)
	{
		AddNewFunction(entry.className, entry.name, entry.scopeName, entry.callback);
	}
}

bool CSScriptExtensionsSystem::ResolveSigs(CGameConfig* gameConfig)
{
	RESOLVE_SIG(gameConfig, "CSScript_RegisterFunctionTemplate", m_pfnRegisterInstanceTemplate)
	RESOLVE_SIG(gameConfig, "CSScript_SwitchContext", m_pfnSwitchScriptContext)
	RESOLVE_SIG(gameConfig, "CSScript_GetCurrentScriptContext", m_pfnGetCurrentCSScript)
	RESOLVE_SIG(gameConfig, "CSScript_AssignEntityToObject", m_pfnScriptAssignEntAuto)
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

bool CSScriptExtensionsSystem::AddNewFunction(
	const std::string& instanceName,
	const std::string& funcName,
	const std::string& scopeName,
	v8::FunctionCallback callback
)
{
	if (callback == nullptr)
		return false;
	if (instanceName.empty())
		return false;
	
	if (!m_registeredFunctions.contains(instanceName))
	{
		auto vec = std::vector<ScriptFunctionInfo>();
		vec.reserve(8); // just a random guess for prealloc.
		m_registeredFunctions[instanceName] = vec;
	}
	m_registeredFunctions[instanceName].emplace_back(funcName, scopeName, callback);
}

CEntityInstance* CSScriptExtensionsSystem::GetEntityInstanceFromScriptObject(v8::Local<v8::Object> obj)
{
	if (obj->InternalFieldCount() != 3) {
		return nullptr;
	}

	// TODO: we don't know the Entity marker to check for, this will probably explode if we pass something else.
	// Validate type marker in field 0
	// values under returned pointer seem to be:
	// u32 0x1
	uint32_t* typeMarker = (uint32_t*)obj->GetAlignedPointerFromInternalField(0);
	//Msg("cs_script type marker is %d", *typeMarker);

	CSScriptEntityHandle* entPointer = (CSScriptEntityHandle*)obj->GetAlignedPointerFromInternalField(1);
	if (!entPointer)
	{
		Msg("[cs_script_extensions] Failed to get entity pointer from object! (Field 1 of object doesn't exist)");
		return nullptr;
	}
	auto ent = entPointer->handle.Get();
	return ent;
}

CCSBaseScript* CSScriptExtensionsSystem::GetCurrentCsScriptInstance()
{
	return m_pfnGetCurrentCSScript();
}

v8::Local<v8::Object> CSScriptExtensionsSystem::CreateEntityObject(CEntityInstance* ent)
{
	return m_pfnScriptAssignEntAuto(ent);
}

//v8::Local<v8::Object> CSScriptExtensionsSystem::AssignEntityToObject(v8::Local<v8::Object> obj, const char* name, CEntityInstance* ent)
//{
//	return m_pfnScriptAssignEntityToObject(obj, name, ent);
//}

void CSScriptExtensionsSystem::Hook_RegisterInstanceTemplate(
	CCSBaseScript* script, 
	const char* name, 
	v8::Local<v8::FunctionTemplate> funcTemplate
)
{
	OnScriptInstanceRegisterInstance(script, funcTemplate->PrototypeTemplate(), name);
	m_pHookRegisterInstanceTemplate.call(script, name, funcTemplate);
}

void CSScriptExtensionsSystem::AddCallbackNative(CCSScript_EntityScript* script, const CUtlString& callbackName, v8::Local<v8::Function> func)
{
	if (script->callbackMap.HasElement(callbackName))
	{
		Log_Warning(g_logChanScript, "Callback \"%s\" already registered\n", callbackName.String());
		return;
	}
	v8::Global<v8::Function>* newFunc = new v8::Global<v8::Function>(v8::Isolate::GetCurrent(), func);
	script->callbackMap.Insert(callbackName, newFunc);
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

void CSScriptExtensionsSystem::InvokeNativeCallbacks()
{
	auto isolate = v8::Isolate::GetCurrent();
	for (CEntityInstance* scriptEnt : GetScripts())
	{
		v8::HandleScope handleScope(isolate);
		auto script = GetScriptFromEntity(scriptEnt);
		auto context = script->context.Get(isolate);
		context->Enter();
		SwitchScriptContext(script);
		FOR_EACH_HASHTABLE(script->callbackMap, i)
		{
			auto key = script->callbackMap.Key(i);
			auto val = script->callbackMap.Element(i);
			auto func = val->Get(isolate);

			v8::TryCatch tryCatch(isolate);
			v8::Local<v8::Value> args[3];
			args[0] = v8::String::NewFromUtf8(isolate, "hello").ToLocalChecked();
			args[1] = v8::Number::New(isolate, 42);
			args[2] = v8::Boolean::New(isolate, true);
			func->Call(context, context->Global(), 3, args);
			if (tryCatch.HasCaught())
			{
				v8::Local<v8::Value> exception = tryCatch.Exception();
				v8::String::Utf8Value exception_str(isolate, exception);
				Log_Warning(g_logChanScript, "Exception: %s\n", *exception_str);
			}
		}
		context->Exit();
	}
}

void CSScriptExtensionsSystem::InvokeNativeCallback(const char* callbackName, int argc, v8::Local<v8::Value> argv[])
{
	VPROF("CSScriptExtensionsSystem::InvokeNativeCallback");

	auto isolate = v8::Isolate::GetCurrent();
	for (CEntityInstance* scriptEnt : GetScripts())
	{
		v8::HandleScope handleScope(isolate);
		auto script = GetScriptFromEntity(scriptEnt);
		InvokeNativeCallbackForScript(script, callbackName, argc, argv);
	}
}

void CSScriptExtensionsSystem::InvokeNativeCallbackForScript(CCSScript_EntityScript* script, const char* callbackName, int argc, v8::Local<v8::Value> argv[])
{
	VPROF("CSScriptExtensionsSystem::InvokeNativeCallbackForScript");

	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	auto funcHandle = script->callbackMap.Get(callbackName, nullptr);
	if (!funcHandle)
		return;
	auto func = funcHandle->Get(isolate);
	auto context = script->context.Get(isolate);
	context->Enter();
	SwitchScriptContext(script);
	v8::TryCatch tryCatch(isolate);
	func->Call(context, context->Global(), argc, argv);
	if (tryCatch.HasCaught())
	{
		v8::Local<v8::Value> exception = tryCatch.Exception();
		v8::String::Utf8Value exception_str(isolate, exception);
		Log_Warning(g_logChanScript, "Exception during cs_script callback \"%s\"\n", *exception_str);
	}
	context->Exit();
}

void CSScriptExtensionsSystem::OnScriptInstanceRegisterInstance(CCSBaseScript* script, v8::Local<v8::ObjectTemplate> functionTemplate, const char* name)
{
	std::string instanceNameStr(name);
	if (!m_registeredFunctions.contains(instanceNameStr))
		return;
	auto& funcs = m_registeredFunctions[instanceNameStr];
	for (const auto& funcInfo : funcs)
	{
		RegisterNewFunction(functionTemplate, funcInfo);
		Log_Msg(g_logChanScript, "Registered script extension function %s.%s\n", funcInfo.scopeName.c_str(), funcInfo.name.c_str());
	}
	if (const auto entityScript = dynamic_cast<CCSScript_EntityScript*>(script))
	{
		V8CallbacksUserMsg::InitUserMessageInfoTemplate(entityScript);
	}
}

void CSScriptExtensionsSystem::RegisterNewFunction(v8::Local<v8::ObjectTemplate> targetFuncTemplate, const ScriptFunctionInfo& funcInfo)
{
	auto isolate = v8::Isolate::GetCurrent();
	auto name = v8::String::NewFromUtf8(isolate, funcInfo.name.c_str()).ToLocalChecked();
	auto funcTemplate = v8::FunctionTemplate::New(isolate, funcInfo.callback);
	targetFuncTemplate->Set(name, funcTemplate);
}

void CSScriptExtensionsSystem::SwitchScriptContext(CCSScript_EntityScript* script)
{
	m_pfnSwitchScriptContext(script);
}

void CSScriptExtensionsSystem::AddUserMessageCallback(NetworkMessageId msgId, v8::Global<v8::Function> callback)
{
}

std::vector<CEntityInstance*> CSScriptExtensionsSystem::GetScripts() {
	std::vector<CEntityInstance*> vecInstances;
	vecInstances.reserve(1);
	EntityInstanceByClassIter_t iter(NULL, "point_script");

	for (CPointScript* currentEnt = (CPointScript*)iter.First();
		currentEnt != nullptr;
		currentEnt = (CPointScript*)iter.Next())
	{
		vecInstances.emplace_back(currentEnt);
	}
	return vecInstances;
}

class CCSPointScriptEntity {};
CCSScript_EntityScript* CSScriptExtensionsSystem::GetScriptFromEntity(CEntityInstance* ent)
{
	if (!ent)
		return nullptr;
	if (dynamic_cast<CCSPointScriptEntity*>(ent) == nullptr)
		return nullptr;

	return reinterpret_cast<CCSScript_EntityScript*>((unsigned char*)ent+0x508);
}

CSScriptHeader* CSScriptExtensionsSystem::GetScriptHeaderFromEntity(CEntityInstance* ent)
{
	if (!ent)
		return nullptr;
	if (dynamic_cast<CCSPointScriptEntity*>(ent) == nullptr)
		return nullptr;

	return reinterpret_cast<CSScriptHeader*>((unsigned char*)ent + 0x4F0);
}
