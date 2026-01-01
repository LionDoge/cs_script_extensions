
#include "v8callbacks.h"
#include "v8-object.h"
#include "v8-isolate.h"
#include "csscript.h"
#include "protobuf/generated/usermessages.pb.h"
#include "interfaces/interfaces.h"
#include "entity/ccsplayercontroller.h"
#include "scriptextensions.h"
#include "schemasystem/schemasystem.h"
#include "hudhintmanager.h"
#include "igameevents.h"
#include "scriptclasses/schemaobject.h"

extern LoggingChannelID_t g_logChanScript;

void ClientPrintAll(int hud_dest, const char* msg, ...);
void ClientPrint(CCSPlayerController* player, int hud_dest, const char* msg, ...);
extern ISchemaSystem* g_pSchemaSystem;
extern IGameEventManager2* g_gameEventManager;
extern HudHintManager g_hudHintManager;
extern CSScriptExtensionsSystem g_scriptExtensions;

static void V8ThrowException(v8::Isolate* isolate, const std::string_view& message)
{
	auto v8ExceptionText = v8::String::NewFromUtf8(isolate, message.data()).ToLocalChecked();
	isolate->ThrowException(v8ExceptionText);
}

bool VerifyScriptScope(const std::string_view& instName, const std::string_view& methodName)
{
	if (!CSScriptExtensionsSystem::GetCurrentCsScriptInstance())
	{
		V8ThrowException(
			v8::Isolate::GetCurrent(),
			std::format("Method {}.{} invoked in incorrect scope.", instName, methodName)
		);
		return false;
	}
	return true;
}

#define V8_SETUP_AND_VERIFY(domain, name) \
	auto isolate = args.GetIsolate(); \
	v8::HandleScope handleScope(isolate); \
	if (!VerifyScriptScope(domain, name)) \
		return;

#define SCRIPT_FUNCTION(name, classname, scopename) \
	void name##_V8ScriptCallback(const v8::FunctionCallbackInfo<v8::Value>& args); \
	g_scriptExtensions.AddNewFunction(classname, scopename, #name, name##_V8ScriptCallback); \
	void name##_V8ScriptCallback(const v8::FunctionCallbackInfo<v8::Value>& args)


std::optional<SchemaClassFieldData_t*> GetSchemaFieldInfo(const char* className, const char* fieldName)
{
	CSchemaSystemTypeScope* pType = g_pSchemaSystem->FindTypeScopeForModule(MODULE_PREFIX "server" MODULE_EXT);

	if (!pType)
		return std::nullopt;

	SchemaClassInfoData_t* pClassInfo = pType->FindDeclaredClass(className).Get();
	if (!pClassInfo)
		return std::nullopt;

	short fieldsSize = pClassInfo->m_nFieldCount;
	SchemaClassFieldData_t* pFields = pClassInfo->m_pFields;

	SchemaClassFieldData_t* field = nullptr;
	for (int i = 0; i < fieldsSize; ++i)
	{
		SchemaClassFieldData_t& currentField = pFields[i];
		if (V_strcmp(fieldName, currentField.m_pszName) == 0)
		{
			return &currentField;
		}

#ifdef _DEBUG
		//Msg("%s::%s found at -> 0x%X - %llx\n", pClassInfo->m_pszName, field.m_pszName, field.m_nSingleInheritanceOffset, &field);
#endif
	}
	return std::nullopt;
}

void V8Callbacks::V8NewMsg(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	VerifyScriptScope("point_script", "NewMsg");
	//auto handle = v8::HandleScope::CreateHandleForCurrentIsolate(script->handleScopeAddr);
	//v8::Local<v8::Object> thisVal = info.This();
	if (args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.MsgCustom requires a string argument.");
		return;
	}
	v8::Local<v8::String> v8String = args[0].As<v8::String>();
	int length = v8String->Utf8Length(isolate);
	std::string cppString(length, '\0');
	v8String->WriteUtf8(isolate, &cppString[0], length);

	Msg("[cs_script] CustomMsg: %s", cppString.c_str());
}

void V8Callbacks::V8GetSchemaField(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("point_script", "GetSchemaField"))
		return;

	if (!args.This()->IsObject())
	{
		V8ThrowException(isolate, "Method point_script.GetSchemaField invoked with incorrect 'this' value.");
		return;
	}

	if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString())
	{
		V8ThrowException(isolate, "Method point_script.GetSchemaField requires 2 arguments (className: string, fieldName: string)");
		return;
	}

	auto ent = CSScriptExtensionsSystem::GetEntityInstanceFromScriptObject(args.This().As<v8::Object>());
	if (!ent)
	{
		// not in-built type, maybe our SchemaObject?
		V8ThrowException(isolate, "Method point_script.GetSchemaField failed to get entity from 'this' object.");
		return;
	}
	else 
	{
		ent = dynamic_cast<CBaseEntity*>(ent);
		if (!ent)
		{
			V8ThrowException(isolate, "Method point_script.GetSchemaField called on invalid entity instance.");
			return;
		}
	}

	v8::Local<v8::String> v8StrClassname = args[0].As<v8::String>();
	v8::String::Utf8Value v8StrClassnameUtf8(isolate, v8StrClassname);
	std::string strClassname(*v8StrClassnameUtf8);

	v8::Local<v8::String> v8StrFieldname = args[1].As<v8::String>();
	v8::String::Utf8Value v8StrFieldnameUtf8(isolate, v8StrFieldname);
	std::string strFieldname(*v8StrFieldnameUtf8);

	auto fieldInfo = GetSchemaFieldInfo(strClassname.c_str(), strFieldname.c_str());
	if (fieldInfo.has_value())
	{
		auto type = fieldInfo.value()->m_pType;
		auto offset = fieldInfo.value()->m_nSingleInheritanceOffset;
		if (!type)
			return;
		switch (type->m_eTypeCategory) {
		case SCHEMA_TYPE_DECLARED_ENUM:
		{
			auto val = *(uint32_t*)((unsigned char*)ent + offset);
			args.GetReturnValue().Set(v8::Number::New(isolate, val));
			break;
		}
		case SCHEMA_TYPE_BUILTIN:
		{
			if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_INT8)) SetV8NumericReturnValue<int8_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_UINT8)) SetV8NumericReturnValue<uint8_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_INT16)) SetV8NumericReturnValue<int16_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_UINT16)) SetV8NumericReturnValue<uint16_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_INT32)) SetV8NumericReturnValue<int32_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_UINT32)) SetV8NumericReturnValue<uint32_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_INT64)) SetV8NumericReturnValue<int64_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_UINT64)) SetV8NumericReturnValue<uint64_t>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_FLOAT32)) SetV8NumericReturnValue<float>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_FLOAT64)) SetV8NumericReturnValue<double>(args, ent, offset);
			/*else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_BOOL)) SetV8NumericReturnValue<bool>(args, ent, offset);
			else if (type->IsBuiltinType(SCHEMA_BUILTIN_TYPE_CHAR)) SetV8NumericReturnValue<char>(args, ent, offset);*/
			break;
		}
		case SCHEMA_TYPE_DECLARED_CLASS:
		{
			if (type->IsDeclaredClass("CUtlString"))
			{
				CUtlString* str = reinterpret_cast<std::add_pointer_t<CUtlString>>((unsigned char*)ent + offset);
				if (str)
				{
					auto v8Str = v8::String::NewFromUtf8(isolate, str->Get()).ToLocalChecked();
					args.GetReturnValue().Set(v8Str);
				}
			}
			else if (type->IsDeclaredClass("GameTime_t"))
			{
				GameTime_t* time = reinterpret_cast<std::add_pointer_t<GameTime_t>>((unsigned char*)ent + offset);
				if (time)
				{
					args.GetReturnValue().Set(v8::Number::New(isolate, time->GetTime()));
				}
			}
			break;
		}
		}
	}
}

void V8Callbacks::V8ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Entity", "ShowHudMessageHTML"))
		return;

	if (!args.This()->IsObject())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudMessageHTML invoked with incorrect 'this' argument.");
		return;
	}
	if (args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudMessageHTML requires 1 string argument.");
		return;
	}
	auto controller = dynamic_cast<CCSPlayerController*>(CSScriptExtensionsSystem::GetEntityInstanceFromScriptObject(args.This().As<v8::Object>()));
	if (!controller)
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudMessageHTML failed to get entity from 'this' object.");
		return;
	}
	double duration = 1.0;
	if (args.Length() >= 2 && args[1]->IsNumber())
	{
		duration = args[1].As<v8::Number>()->Value();
		if (duration < 0.0)
			duration = 0.0;
	}
	v8::String::Utf8Value v8StrTextUtf8(isolate, args[0].As<v8::String>());
	std::string text(*v8StrTextUtf8);
	g_hudHintManager.AddHintMessage(controller->GetPlayerSlot(), text, duration);
}

void V8Callbacks::V8ShowHudHintAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("point_script", "ShowHudHintAll"))
		return;

	if (args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.ShowHudHintAll requires 1 argument (text: string)");
		return;
	}

	bool isAlert = false;
	if (args.Length() >= 2 && !args[1]->IsBoolean())
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.ShowHudHintAll second argument must be a boolean");
		return;
	}
	isAlert = args[1].As<v8::Boolean>()->Value();

	v8::String::Utf8Value v8StrTextUtf8(isolate, args[0].As<v8::String>());
	ClientPrintAll(isAlert ? HUD_PRINTALERT : HUD_PRINTCENTER, *v8StrTextUtf8);
}

void V8Callbacks::V8ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Entity", "ShowHudHint"))
		return;

	if (args.Length() < 1 || !args.This()->IsObject())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudHint requires 1 argument (text: string)");
		return;
	}

	bool isAlert = false;
	if (args.Length() >= 2 && !args[1]->IsBoolean())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudHi=nt second argument must be a boolean");
		return;
	}
	isAlert = args[1].As<v8::Boolean>()->Value();

	CEntityInstance* ent = CSScriptExtensionsSystem::GetEntityInstanceFromScriptObject(args.This().As<v8::Object>());
	if (!ent)
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudHint failed to get entity from 'this' object.");
		return;
	}
	CCSPlayerController* controller = dynamic_cast<CCSPlayerController*>(ent);
	if (!controller)
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.ShowHudHint can only be called on player controller instances.");
		return;
	}

	v8::String::Utf8Value v8StrTextUtf8(isolate, args[0].As<v8::String>());
	ClientPrint(controller, isAlert ? HUD_PRINTALERT : HUD_PRINTCENTER, *v8StrTextUtf8);
}

void V8Callbacks::AddSampleCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = args.GetIsolate();
	auto script = (CCSScript_EntityScript*)CSScriptExtensionsSystem::GetCurrentCsScriptInstance();
	//CUtlStringToken testToken("OnPlayerPing");
	//CUtlStringToken testToken2("OnPlayerPing2");
	//auto elem = script->callbackMap.Insert("OnPlayerPing", (void*)&script);
	//auto exists = script->callbackMap.HasElement("OnScriptInput:Test");
	//auto elem = script->callbackMap.GetPtr("OnScriptInput:Test");
	v8::HandleScope handleScope(isolate);
	//auto count = script->callbackMap.Count();
	
	if (args.Length() < 1)
	{
		V8ThrowException(args.GetIsolate(), "AddSampleCallback requires at least 1 argument.");
		return;
	}
	if(!args[0]->IsFunction())
	{
		V8ThrowException(args.GetIsolate(), "First argument to AddSampleCallback must be a function.");
		return;
	}
	auto callback = args[0].As<v8::Function>();

	//const char* test = func("OnPlayerPing");
	//v8::Global<v8::Function> def;
	/*uint64_t a = 50;
	auto elem = script->callbackMap.Insert(test, a);*/
	//auto& functor = script->callbackMap.GetHashRef();
	g_scriptExtensions.AddCallbackNative(
		script,
		"OnSampleCallback",
		callback
	);
}

void V8Callbacks::SetEntityMoveType(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	V8_SETUP_AND_VERIFY("Entity", "SetMoveType");

	if (args.Length() < 1 || !args.This()->IsObject() || !args[0]->IsNumber())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetMoveType requires 1 argument (moveType: number)"); \
		return;
	}

	CEntityInstance* ent = CSScriptExtensionsSystem::GetEntityInstanceFromScriptObject(args.This().As<v8::Object>());
	if (!ent)
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetMoveType failed to get entity from 'this' object.");
		return;
	}
	CBaseEntity* baseEnt = dynamic_cast<CBaseEntity*>(ent);
	if (!baseEnt)
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetMoveType called on invalid entity instance.");
		return;
	}

	int moveType = static_cast<int>(args[0].As<v8::Number>()->Value());
	baseEnt->SetMoveType(static_cast<MoveType_t>(moveType));
	/*baseEnt->m_MoveType();
	baseEnt->m_nActualMoveType(static_cast<MoveType_t>(moveType));*/
}

template <typename T>
constexpr void V8Callbacks::SetV8NumericReturnValue(const v8::FunctionCallbackInfo<v8::Value>& args, void* ent, size_t offset)
{
	auto val = *reinterpret_cast<std::add_pointer_t<T>>(static_cast<unsigned char*>(ent) + offset);
	args.GetReturnValue().Set(v8::Number::New(args.GetIsolate(), val));
}