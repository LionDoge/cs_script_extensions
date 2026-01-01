
#pragma once
#include "userMessagesScriptExt.h"
#include <v8-external.h>
#include "v8-object.h"
#include "v8-isolate.h"
#include "src/protobuf/generated/usermessages.pb.h"
#include "src/scriptextensions.h"
#include "igameevents.h"
#include "userMessageInfo.h"
#include "src/plugin.h"

extern LoggingChannelID_t g_logChanScript;

extern ISchemaSystem* g_pSchemaSystem;
extern IGameEventManager2* g_gameEventManager;
extern CSScriptExtensionsSystem g_scriptExtensions;

static void V8ThrowException(v8::Isolate* isolate, const std::string_view& message)
{
	auto v8ExceptionText = v8::String::NewFromUtf8(isolate, message.data()).ToLocalChecked();
	isolate->ThrowException(v8ExceptionText);
}

void UserMessageInfoObjectConstructor(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	V8ThrowException(isolate, "Cannot be constructed from script\n");
}

void V8CallbacksUserMsg::InitUserMessageInfoTemplate(CCSScript_EntityScript* script)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, UserMessageInfoObjectConstructor);
	tpl->SetClassName(v8::String::NewFromUtf8(isolate, "UserMessageInfo").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	// All functions
	auto proto = tpl->PrototypeTemplate();
	proto->Set(isolate,"GetField",v8::FunctionTemplate::New(isolate, UserMessageInfo_GetField));

	auto persistentTp = new v8::Global<v8::FunctionTemplate>(isolate, tpl);
	script->functionTemplateMap.Insert(MakeGlobalSymbol("UserMessageInfo"), persistentTp);
}

v8::Local<v8::Value> V8CallbacksUserMsg::CreateUserMessageInfoInstance(CCSScript_EntityScript* script, ScriptUserMessageInfo* userMsgInfo)
{
	auto isolate = v8::Isolate::GetCurrent();
	//v8::HandleScope _scop(isolate); // needed, because otherwise we get a fatal error?
	//v8::EscapableHandleScope handleScope(isolate);

	auto tplPointer = script->functionTemplateMap.Get(MakeGlobalSymbol("UserMessageInfo"), nullptr);
	if (!tplPointer)
	{
		Log_Warning(g_logChanScript, "UserMessageInfo template not initialized!");
		return {};
	}

	auto tpl = tplPointer->Get(isolate);
	v8::Local<v8::Object> instance = tpl->InstanceTemplate()->NewInstance(script->context.Get(isolate)).ToLocalChecked();
	instance->SetAlignedPointerInInternalField(0, userMsgInfo);
	return instance;
	//return handleScope.Escape(instance);
}

void V8CallbacksUserMsg::OnUserMessage(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	const auto isolate = v8::Isolate::GetCurrent();
	const auto baseScript = CSScriptExtensionsSystem::GetCurrentCsScriptInstance();
	const auto script = dynamic_cast<CCSScript_EntityScript*>(baseScript);
	if (!script)
	{
		V8ThrowException(isolate, "OnUserMessage invoked in incorrect scope\n");
		return;
	}
	v8::HandleScope handleScope(isolate);
	if (info.Length() != 1)
	{
		V8ThrowException(isolate, "OnUserMessage requires 1 argument: (object {str msgName, function callback(info)})\n");
		return;
	}
	if (!info[0]->IsObject())
	{
		V8ThrowException(isolate, "OnUserMessage argument 0 must be an object {str msgName, function callback(info)}\n");
		return;
	}
	auto v8Obj = info[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
	auto v8MsgName = v8Obj->Get(isolate->GetCurrentContext(), v8::String::NewFromUtf8(isolate, "messageName").ToLocalChecked()).ToLocalChecked();
	if (!v8MsgName->IsString())
	{
		V8ThrowException(isolate, "OnUserMessage argument 0.msgName must be a string\n");
		return;
	}
	v8::String::Utf8Value msgNameUtf8(isolate, v8MsgName);
	auto v8Callback = v8Obj->Get(isolate->GetCurrentContext(), v8::String::NewFromUtf8(isolate, "callback").ToLocalChecked()).ToLocalChecked();
	if (!v8Callback->IsFunction())
	{
		V8ThrowException(isolate, "OnUserMessage argument 0.callback must be a function\n");
		return;
	}
	auto v8FuncCallback = v8::Local<v8::Function>::Cast(v8Callback);
	auto netMessageInternal = g_pNetworkMessages->FindNetworkMessagePartial(*msgNameUtf8);
	if (!netMessageInternal)
	{
		V8ThrowException(isolate, std::string("OnUserMessage: could not find user message with partial name '") + *msgNameUtf8 + "'");
		return;
	}
	NetworkMessageId msgId = netMessageInternal->GetNetMessageInfo()->m_MessageId;

	CUtlString cbName("OnUserMessage:");
	cbName += msgId;
	g_scriptExtensions.AddCallbackNative(script, cbName.Get(), v8FuncCallback);
}

void V8CallbacksUserMsg::UserMessageInfo_GetField(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	if (info.Length() != 1)
	{
		V8ThrowException(isolate, "UserMessageInfo.GetField requires 1 argument: (string fieldName)\n");
		return;
	}
	if (info[0].IsEmpty() || !info[0]->IsString())
	{
		V8ThrowException(isolate, "UserMessageInfo.GetField argument 0 must be a string\n");
		return;
	}
	auto value = static_cast<ScriptUserMessageInfo*>(info.This()->GetAlignedPointerFromInternalField(0));
	
	bool isRepeated = false;
	auto cppType = google::protobuf::FieldDescriptor::CPPTYPE_INT32;
	v8::String::Utf8Value fieldNameUtf8(isolate, info[0]);
	if (!value->GetFieldType(*fieldNameUtf8, cppType, isRepeated))
	{
		V8ThrowException(isolate, "UserMessageInfo.GetField: could not find field with given name\n");
		return;
	}
	if (isRepeated)
	{
		V8ThrowException(isolate, "UserMessageInfo.GetField: repeated field not supported yet\n");
		return;
	}
	auto returnValue = info.GetReturnValue();
	switch (cppType)
	{
		case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
		{
			int32_t val_i32;
			value->GetInt32(*fieldNameUtf8, val_i32);
			returnValue.Set(v8::Number::New(isolate, val_i32));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
		{
			uint32_t val_u32;
			value->GetUInt32(*fieldNameUtf8, val_u32);
			returnValue.Set(v8::Number::New(isolate, val_u32));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
		{
			int64_t val_i64;
			value->GetInt64(*fieldNameUtf8, val_i64);
			returnValue.Set(v8::Number::New(isolate, static_cast<double>(val_i64)));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
		{
			uint64_t val_u64;
			value->GetUInt64(*fieldNameUtf8, val_u64);
			returnValue.Set(v8::Number::New(isolate, static_cast<double>(val_u64)));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
		{
			double val_d;
			value->GetDouble(*fieldNameUtf8, val_d);
			returnValue.Set(v8::Number::New(isolate, val_d));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		{
			std::string val_s;
			value->GetString(*fieldNameUtf8, val_s);
			returnValue.Set(v8::String::NewFromUtf8(isolate, val_s.c_str()).ToLocalChecked());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			V8ThrowException(isolate, "Unsupported usermessage field type");
	}
}
