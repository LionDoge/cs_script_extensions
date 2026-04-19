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

#include "userMessagesScriptExt.h"
#include "v8-external.h"
#include "v8-object.h"
#include "v8-isolate.h"
#include "v8-exception.h"
#include "usermessages.pb.h"
#include "scriptExtensions/scriptextensions.h"
#include "igameevents.h"
#include "igameeventsystem.h"

#include "recipientfilters.h"
#include "scriptcommon.h"
#include "userMessageInfo.h"
#include "managedObject.h"
#include "plugin.h"
#include "scripttypes.h"

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

	auto tplPointer = script->GetFunctionTemplate("UserMessageInfo");
	if (!tplPointer)
	{
		Log_Warning(g_logChanScript, "UserMessageInfo template not initialized!\n");
		return {};
	}

	auto tpl = tplPointer->Get(isolate);
	v8::Local<v8::Object> instance = tpl->InstanceTemplate()->NewInstance(script->GetContext().Get(isolate)).ToLocalChecked();

	auto obj = new ManagedObject(isolate, instance, userMsgInfo);
	instance->SetAlignedPointerInInternalField(0, &ScriptTypeMarkers::userMessageInfo);
	instance->SetAlignedPointerInInternalField(1, obj);
	return instance;
}

ScriptUserMessageInfo* ScriptUserMessage::GetUserMessageInfoObject(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto wrapper = static_cast<ManagedObject<ScriptUserMessageInfo>*>(args.This()->GetAlignedPointerFromInternalField(1));
	return wrapper->GetData();
}

void ScriptUserMessage::OnUserMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	const auto script = ScriptExtensions::GetCurrentCsScriptInstance();

	if (args.Length() != 1 || !args[0]->IsObject())
	{
		ThrowFunctionException(context, "requires 1 argument: (object {str messageName, function callback(info)})");
		return;
	}

	auto v8Obj = args[0]->ToObject(context.isolate->GetCurrentContext()).ToLocalChecked();
	auto maybeMsgName = v8Obj->Get(context.isolate->GetCurrentContext(), v8::String::NewFromUtf8(context.isolate, "messageName").ToLocalChecked());
	if (maybeMsgName.IsEmpty())
	{
		ThrowFunctionException(context, "argument 0 must have a messageName property");
		return;
	}
	auto v8MsgName = maybeMsgName.ToLocalChecked();
	if (!v8MsgName->IsString())
	{
		ThrowFunctionException(context, "argument 0.messageName must be a string");
		return;
	}
	v8::String::Utf8Value msgNameUtf8(context.isolate, v8MsgName);

	auto maybeCallback = v8Obj->Get(context.isolate->GetCurrentContext(), v8::String::NewFromUtf8(context.isolate, "callback").ToLocalChecked());
	if (maybeCallback.IsEmpty())
	{
		ThrowFunctionException(context, "argument 0 must have a callback property");
		return;
	}
	auto v8Callback = maybeCallback.ToLocalChecked();
	if (!v8Callback->IsFunction())
	{
		ThrowFunctionException(context, "argument 0.callback must be a function");
		return;
	}

	auto v8FuncCallback = v8::Local<v8::Function>::Cast(v8Callback);
	auto netMessageInternal = g_pNetworkMessages->FindNetworkMessagePartial(*msgNameUtf8);
	if (!netMessageInternal)
	{
		ThrowFunctionException(context, std::format("could not find user message with partial name '{}'", *msgNameUtf8));
		return;
	}

	NetMessageInfo_t* msgInfo = netMessageInternal->GetNetMessageInfo();
	if (!msgInfo)
	{
		ThrowFunctionException(context, std::format("failed to get message info '{}'", *msgNameUtf8));
		return;
	}

	CBufferString cbName("OnUserMessage:");
	cbName.AppendFormat("%d", msgInfo->m_MessageId);
	script->AddCallback(cbName.Get(), v8FuncCallback);
}

void ScriptUserMessage::UserMessageInfo_GetField(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto userMessageInfoWrap = UnwrapThisAsManagedObject<ScriptUserMessageInfo>(context, &ScriptTypeMarkers::userMessageInfo);
	auto fieldName = UnwrapArg<std::string>(context, 0);

	if (!userMessageInfoWrap || !fieldName)
		return;

	auto value = (*userMessageInfoWrap)->GetData();
	if (!value->IsValid())
	{
		ThrowFunctionException(context, "UserMessageInfo object is no longer valid");
		return;
	}
	
	bool isRepeated = false;
	auto cppType = google::protobuf::FieldDescriptor::CPPTYPE_INT32;
	if (!value->GetFieldType(fieldName->c_str(), cppType, isRepeated))
	{
		ThrowFunctionException(context, "could not find field with given name");
		return;
	}
	if (isRepeated)
	{
		ThrowFunctionException(context, "repeated field not supported yet");
		return;
	}
	auto returnValue = args.GetReturnValue();
	switch (cppType)
	{
		case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
		{
			int32_t val_i32 = 0;
			value->GetInt32(fieldName->c_str(), val_i32);
			returnValue.Set(v8::Number::New(isolate, val_i32));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
		{
			uint32_t val_u32 = 0;
			value->GetUInt32(fieldName->c_str(), val_u32);
			returnValue.Set(v8::Number::New(isolate, val_u32));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
		{
			int64_t val_i64 = 0;
			value->GetInt64(fieldName->c_str(), val_i64);
			returnValue.Set(v8::Number::New(isolate, static_cast<double>(val_i64)));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
		{
			uint64_t val_u64 = 0;
			value->GetUInt64(fieldName->c_str(), val_u64);
			returnValue.Set(v8::Number::New(isolate, static_cast<double>(val_u64)));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		{
			float val_f = 0;
			value->GetFloat(fieldName->c_str(), val_f);
			returnValue.Set(v8::Number::New(isolate, val_f));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
		{
			double val_d = 0;
			value->GetDouble(fieldName->c_str(), val_d);
			returnValue.Set(v8::Number::New(isolate, val_d));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		{
			std::string val_s;
			value->GetString(fieldName->c_str(), val_s);
			returnValue.Set(v8::String::NewFromUtf8(isolate, val_s.c_str()).ToLocalChecked());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		{
			bool val_b = false;
			value->GetBool(fieldName->c_str(), val_b);
			returnValue.Set(v8::Boolean::New(isolate, val_b));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			ThrowFunctionException(context, "unsupported usermessage field type");
	}
}

void ScriptUserMessage::UserMessageInfo_SetField(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto userMessageInfoWrap = UnwrapThisAsManagedObject<ScriptUserMessageInfo>(context, &ScriptTypeMarkers::userMessageInfo);
	auto fieldName = UnwrapArg<std::string>(context, 0);

	if (!userMessageInfoWrap || !fieldName)
		return;

	auto value = (*userMessageInfoWrap)->GetData();
	if (!value->IsValid())
	{
		ThrowFunctionException(context, "UserMessageInfo object is no longer valid");
		return;
	}

	bool isRepeated = false;
	auto cppType = google::protobuf::FieldDescriptor::CPPTYPE_INT32;
	if (!value->GetFieldType(fieldName->c_str(), cppType, isRepeated))
	{
		ThrowFunctionException(context, "could not find field with given name");
		return;
	}
	if (isRepeated)
	{
		ThrowFunctionException(context, "repeated field not supported yet");
		return;
	}

	v8::TryCatch tryCatch(isolate);
	auto val = args[1];

	switch (cppType)
	{
		case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
		{
			if (val->IsNumber())
				value->SetInt32(fieldName->c_str(), static_cast<int32_t>(val.As<v8::Number>()->Value()));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
		{
			if (val->IsNumber())
				value->SetUInt32(fieldName->c_str(), static_cast<uint32_t>(val.As<v8::Number>()->Value()));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
		{
			if (val->IsNumber())
				value->SetInt64(fieldName->c_str(), static_cast<int64_t>(val.As<v8::Number>()->Value()));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
		{
			if (val->IsNumber())
				value->SetUInt64(fieldName->c_str(), static_cast<uint64_t>(val.As<v8::Number>()->Value()));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		{
			if (val->IsNumber())
				value->SetFloat(fieldName->c_str(), static_cast<float>(val.As<v8::Number>()->Value()));
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
		{
			if (val->IsNumber())
				value->SetDouble(fieldName->c_str(), val.As<v8::Number>()->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		{
			if (val->IsString())
			{
				v8::String::Utf8Value str(isolate, val);
				value->SetString(fieldName->c_str(), *str);
			}
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		{
			value->SetBool(fieldName->c_str(), val->ToBoolean(isolate)->Value());
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			ThrowFunctionException(context, "unsupported usermessage field type");
	}

	if (tryCatch.HasCaught())
	{
		ThrowFunctionException(context, "error converting value to expected type");
	}
}

void ScriptUserMessage::UserMessageInfo_ClearRecipients(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto userMessageInfoWrap = UnwrapThisAsManagedObject<ScriptUserMessageInfo>(context, &ScriptTypeMarkers::userMessageInfo);
	if (!userMessageInfoWrap)
		return;

	auto msgInfo = (*userMessageInfoWrap)->GetData();
	if (!msgInfo->IsValid())
	{
		ThrowFunctionException(context, "UserMessageInfo object is no longer valid");
		return;
	}
	msgInfo->ClearRecipients();
}

void ScriptUserMessage::UserMessageInfo_AddRecipient(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto userMessageInfoWrap = UnwrapThisAsManagedObject<ScriptUserMessageInfo>(context, &ScriptTypeMarkers::userMessageInfo);
	auto playerSlot = UnwrapArg<int>(context, 0, false);

	if (!userMessageInfoWrap || !playerSlot)
		return;

	if (*playerSlot < 0 || *playerSlot > 64)
	{
		ThrowFunctionException(context, "argument 0 must be a number between 0 and 64 (player slot)");
		return;
	}

	auto msgInfo = (*userMessageInfoWrap)->GetData();
	if (!msgInfo->IsValid())
	{
		ThrowFunctionException(context, "UserMessageInfo object is no longer valid");
		return;
	}
	msgInfo->AddRecipient(*playerSlot);
}

void ScriptUserMessage::UserMessageInfo_AddAllRecipients(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto userMessageInfoWrap = UnwrapThisAsManagedObject<ScriptUserMessageInfo>(context, &ScriptTypeMarkers::userMessageInfo);
	if (!userMessageInfoWrap)
		return;

	auto msgInfo = (*userMessageInfoWrap)->GetData();
	if (!msgInfo->IsValid())
	{
		ThrowFunctionException(context, "UserMessageInfo object is no longer valid");
		return;
	}
	msgInfo->AddAllRecipients();
}

void ScriptUserMessage::UserMessageInfo_RemoveRecipient(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto userMessageInfoWrap = UnwrapThisAsManagedObject<ScriptUserMessageInfo>(context, &ScriptTypeMarkers::userMessageInfo);
	auto playerSlot = UnwrapArg<int>(context, 0, false);

	if (!userMessageInfoWrap || !playerSlot)
		return;

	if (*playerSlot < 0 || *playerSlot > 64)
	{
		ThrowFunctionException(context, "argument 0 must be a number between 0 and 64 (player slot)");
		return;
	}

	auto msgInfo = (*userMessageInfoWrap)->GetData();
	if (!msgInfo->IsValid())
	{
		ThrowFunctionException(context, "UserMessageInfo object is no longer valid");
		return;
	}
	msgInfo->RemoveRecipient(*playerSlot);
}

void ScriptUserMessage::UserMessageInfo_Send(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto userMessageInfoWrap = UnwrapThisAsManagedObject<ScriptUserMessageInfo>(context, &ScriptTypeMarkers::userMessageInfo);
	if (!userMessageInfoWrap)
		return;

	auto msgInfo = (*userMessageInfoWrap)->GetData();
	if (!msgInfo->CanBeSent())
	{
		ThrowFunctionException(context, "this usermessage cannot be sent (either already sent or is a received message)");
		return;
	}

	auto data = msgInfo->GetMessage();
	INetworkMessageInternal* netMsgInternal = msgInfo->GetNetMessageInternal();
	if (!netMsgInternal)
	{
		ThrowFunctionException(context, "cannot send a usermessage that was not created from script");
		return;
	}

	// Kinda ugly, should just store recipient filter in the class.
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
