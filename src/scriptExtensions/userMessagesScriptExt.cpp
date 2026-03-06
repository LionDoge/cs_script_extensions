#include "userMessagesScriptExt.h"
#include "v8-external.h"
#include "v8-object.h"
#include "v8-isolate.h"
#include "v8-exception.h"
#include "protobuf/generated/usermessages.pb.h"
#include "scriptExtensions/scriptextensions.h"
#include "igameevents.h"
#include "igameeventsystem.h"

#include "recipientfilters.h"
#include "scriptcommon.h"
#include "userMessageInfo.h"
#include "managedObject.h"
#include "plugin.h"

extern LoggingChannelID_t g_logChanScript;

extern ISchemaSystem* g_pSchemaSystem;
extern IGameEventManager2* g_gameEventManager;
extern IGameEventSystem* g_gameEventSystem;

void ScriptUserMessage::InitUserMessageInfoTemplate(CCSBaseScript* script)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, V8FakeObjectConstructorCallback);
	tpl->SetClassName(v8::String::NewFromUtf8(isolate, "UserMessageInfo").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	// All functions
	auto proto = tpl->PrototypeTemplate();
	proto->Set(isolate,"GetField",v8::FunctionTemplate::New(isolate, UserMessageInfo_GetField));

	script->AddFunctionTemplate("UserMessageInfo", tpl);
}

v8::Local<v8::Value> ScriptUserMessage::CreateUserMessageInfoInstance(CCSScript_EntityScript* script, ScriptUserMessageInfo* userMsgInfo)
{
	auto isolate = v8::Isolate::GetCurrent();
	//v8::HandleScope _scop(isolate); // needed, because otherwise we get a fatal error?
	//v8::EscapableHandleScope handleScope(isolate);

	auto tplPointer = script->GetFunctionTemplate("UserMessageInfo");
	if (!tplPointer)
	{
		Log_Warning(g_logChanScript, "UserMessageInfo template not initialized!");
		return {};
	}

	auto tpl = tplPointer->Get(isolate);
	v8::Local<v8::Object> instance = tpl->InstanceTemplate()->NewInstance(script->GetContext().Get(isolate)).ToLocalChecked();

	auto obj = new ManagedObjectTest(isolate, instance, userMsgInfo);
	instance->SetAlignedPointerInInternalField(0, obj);
	return instance;
	//return handleScope.Escape(instance);
}

ScriptUserMessageInfo* ScriptUserMessage::GetUserMessageInfoObject(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto wrapper = static_cast<ManagedObjectTest*>(info.This()->GetAlignedPointerFromInternalField(0));
	return wrapper->GetData();
}

void ScriptUserMessage::OnUserMessage(const v8::FunctionCallbackInfo<v8::Value>& info)
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
	NetMessageInfo_t* msgInfo = netMessageInternal->GetNetMessageInfo();
	if (!msgInfo)
	{
		V8ThrowException(isolate, std::string("OnUserMessage: failed to get message info'") + *msgNameUtf8 + "'");
		return;
	}

	CBufferString cbName("OnUserMessage:");
	cbName.AppendFormat("%d", msgInfo->m_MessageId);
	script->AddCallback(cbName.Get(), v8FuncCallback);
}

void ScriptUserMessage::UserMessageInfo_GetField(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	if (info.This().IsEmpty())
	{
		V8ThrowException(isolate, "UserMessageInfo.GetField invoked on an invalid 'this' value\n");
		return;
	}
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
	auto value = GetUserMessageInfoObject(info);
	
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
		case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		{
			float val_f;
			value->GetFloat(*fieldNameUtf8, val_f);
			returnValue.Set(v8::Number::New(isolate, val_f));
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
		case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		{
			bool val_b;
			value->GetBool(*fieldNameUtf8, val_b);
			returnValue.Set(v8::Boolean::New(isolate, val_b));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			V8ThrowException(isolate, "Unsupported usermessage field type");
	}
}

void ScriptUserMessage::UserMessageInfo_SetField(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	auto context = isolate->GetCurrentContext();
	if (info.This().IsEmpty())
	{
		V8ThrowException(isolate, "UserMessageInfo.SetField invoked on an invalid 'this' value\n");
		return;
	}
	if (info.Length() != 2)
	{
		V8ThrowException(isolate, "UserMessageInfo.SetField requires 2 arguments: (fieldName: string, value: any)\n");
		return;
	}
	if (info[0].IsEmpty() || !info[0]->IsString())
	{
		V8ThrowException(isolate, "UserMessageInfo.SetField argument 0 must be a string\n");
		return;
	}
	auto value = GetUserMessageInfoObject(info);

	bool isRepeated = false;
	auto cppType = google::protobuf::FieldDescriptor::CPPTYPE_INT32;
	v8::String::Utf8Value fieldNameUtf8(isolate, info[0]);
	if (!value->GetFieldType(*fieldNameUtf8, cppType, isRepeated))
	{
		V8ThrowException(isolate, "UserMessageInfo.SetField: could not find field with given name\n");
		return;
	}
	if (isRepeated)
	{
		V8ThrowException(isolate, "UserMessageInfo.SetField: repeated field not supported yet\n");
		return;
	}

	v8::TryCatch tryCatch(isolate);
	auto val = info[1];
	switch (cppType)
	{
		case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
		{
			if (val->IsNumber())
				value->SetInt32(*fieldNameUtf8, val.As<v8::Number>()->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
		{
			if (val->IsNumber())
				value->SetUInt32(*fieldNameUtf8, val.As<v8::Number>()->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
		{
			if (val->IsNumber())
				value->SetInt64(*fieldNameUtf8, val.As<v8::Number>()->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
		{
			if (val->IsNumber())
				value->SetUInt64(*fieldNameUtf8, val.As<v8::Number>()->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		{
			if (val->IsNumber())
				value->SetFloat(*fieldNameUtf8, val.As<v8::Number>()->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
		{
			if (val->IsNumber())
				value->SetDouble(*fieldNameUtf8, val.As<v8::Number>()->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		{
			if (val->IsString())
			{
				v8::String::Utf8Value str(isolate, val);
				value->SetString(*fieldNameUtf8, *str);
			}
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		{
			value->SetBool(*fieldNameUtf8, val->ToBoolean(isolate)->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			V8ThrowException(isolate, "Unsupported usermessage field type");
	}

	if (tryCatch.HasCaught())
	{
		V8ThrowException(isolate, "Error converting value to expected type");
		return;
	}
}

void ScriptUserMessage::UserMessageInfo_ClearRecipients(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	if (info.This().IsEmpty())
	{
		V8ThrowException(isolate, "UserMessageInfo.ClearRecipients invoked on an invalid 'this' value\n");
		return;
	}
	auto msgInfo = GetUserMessageInfoObject(info);
	msgInfo->ClearRecipients();
}

void ScriptUserMessage::UserMessageInfo_AddRecipient(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	if (info.This().IsEmpty())
	{
		V8ThrowException(isolate, "UserMessageInfo.AddRecipient invoked on an invalid 'this' value\n");
		return;
	}
	if (info.Length() != 1)
	{
		V8ThrowException(isolate, "UserMessageInfo.AddRecipient requires 1 argument: (playerSlot: number)\n");
		return;
	}
	if (info[0].IsEmpty() || !info[0]->IsNumber())
	{
		V8ThrowException(isolate, "UserMessageInfo.GetField argument 0 must be a number\n");
		return;
	}
	double number = info[0]->ToNumber(isolate->GetCurrentContext()).ToLocalChecked()->Value();
	if (number < 0 || number > 64)
	{
		V8ThrowException(isolate, "UserMessageInfo.AddRecipient argument 0 must be a number between 0 and 64 (player slot)\n");
		return;
	}
	auto msgInfo = GetUserMessageInfoObject(info);
	msgInfo->AddRecipient(static_cast<int>(number));
}

void ScriptUserMessage::UserMessageInfo_AddAllRecipients(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	if (info.This().IsEmpty())
	{
		V8ThrowException(isolate, "UserMessageInfo.AddAllRecipient invoked on an invalid 'this' value\n");
		return;
	}
	auto msgInfo = GetUserMessageInfoObject(info);
	msgInfo->AddAllRecipients();
}

void ScriptUserMessage::UserMessageInfo_RemoveRecipient(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	if (info.This().IsEmpty())
	{
		V8ThrowException(isolate, "UserMessageInfo.RemoveRecipient invoked on an invalid 'this' value\n");
		return;
	}
	if (info.Length() != 1)
	{
		V8ThrowException(isolate, "UserMessageInfo.RemoveRecipient requires 1 argument: (playerSlot: number)\n");
		return;
	}
	if (info[0].IsEmpty() || !info[0]->IsString())
	{
		V8ThrowException(isolate, "UserMessageInfo.GetField argument 0 must be a string\n");
		return;
	}
	double number = info[0]->ToNumber(isolate->GetCurrentContext()).ToLocalChecked()->Value();
	if (number < 0 || number > 64)
	{
		V8ThrowException(isolate, "UserMessageInfo.RemoveRecipient argument 0 must be a number between 0 and 64 (player slot)\n");
		return;
	}
	auto msgInfo = GetUserMessageInfoObject(info);
	msgInfo->RemoveRecipient(number);
}

void ScriptUserMessage::UserMessageInfo_Send(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	if (info.This().IsEmpty())
	{
		V8ThrowException(isolate, "UserMessageInfo.Send invoked on an invalid 'this' value\n");
		return;
	}
	auto msgInfo = GetUserMessageInfoObject(info);
	if(!msgInfo->CanBeSent())
	{
		V8ThrowException(isolate, "This usermessage cannot be sent (either already sent or is a received message)\n");
		return;
	}
	auto data = msgInfo->GetMessage();
	INetworkMessageInternal* netMsgInternal = msgInfo->GetNetMessageInternal();
	if(!netMsgInternal)
	{
		V8ThrowException(isolate, "Cannot send a usermessage that was not created from script!\n");
		return;
	}

	// kinda ugly, should just store recipient filter in the class.
	CRecipientFilter filter;
	auto recipients = msgInfo->GetRecipients();
	for (uint64_t i = 0; i < 64; i++)
	{
		if ((recipients >> i) & 1)
		{
			filter.AddRecipient(i);
		}
	}
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, netMsgInternal, data, 0);
	msgInfo->PostSend();
}
