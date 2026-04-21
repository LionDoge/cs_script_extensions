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

#include <format>
#include "scriptExtensions/scriptDomainCallbacks.h"
#include "v8.h"
#include "usermessages.pb.h"
#include "entity/ccsplayercontroller.h"
#include "interfaces/interfaces.h"
#include "networksystem/inetworkmessages.h"
#include "schemasystem/schemasystem.h"
#include "igameevents.h"
#include "hudhintmanager.h"
#include "recipientfilters.h"
#include "common.h"
#include "scriptExtensions/userMessagesScriptExt.h"
#include "playermanager.h"
#include "schema.h"
#include "convar.h"
#include "scriptcommon.h"

void ClientPrintAll(int hud_dest, const char* msg, ...);
extern HudHintManager g_hudHintManager;
extern ScriptExtensions g_scriptExtensions;
extern PlayerManager g_playerManager;

void ScriptDomainCallbacks::NewMsg(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	if (args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.MsgCustom requires a string argument.");
		return;
	}
	v8::Local<v8::String> v8String = args[0].As<v8::String>();
	int length = v8String->Utf8Length(isolate);
	std::string cppString(length, '\0');
	v8String->WriteUtf8(isolate, &cppString[0], length);

	PluginMsg("CustomMsg: %s", cppString.c_str());
}

void ScriptDomainCallbacks::GetSchemaField(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto targetEntHandle = UnwrapThis<CEntityHandle>(context);
	auto className = UnwrapArg<std::string>(context, 0);
	auto fieldName = UnwrapArg<std::string>(context, 1);
	
	if(!targetEntHandle || !className || !fieldName)
		return;

	if (!targetEntHandle->IsValid())
	{
		ThrowFunctionException(context, "failed to get entity from 'this' object.");
		return;
	}

	auto ent = static_cast<CBaseEntity*>(targetEntHandle->Get());
	if (!ent)
	{
		ThrowFunctionException(context, "called on invalid entity instance.");
		return;
	}

	uint32_t classNameHash = hash_32_fnv1a_const(className->c_str());
	uint32_t fieldNameHash = hash_32_fnv1a_const(fieldName->c_str());

	SchemaKey schemaFieldInfo = schema::GetOffset(className->c_str(), classNameHash, fieldName->c_str(), fieldNameHash);

	auto offset = schemaFieldInfo.offset;
	switch (schemaFieldInfo.keyType) {
	case SchemaKeyType::Int8: SetSchemaReturnValue<int8_t>(args, ent, offset); break;
	case SchemaKeyType::Uint8: SetSchemaReturnValue<uint8_t>(args, ent, offset); break;
	case SchemaKeyType::Int16: SetSchemaReturnValue<int16_t>(args, ent, offset); break;
	case SchemaKeyType::Uint16: SetSchemaReturnValue<uint16_t>(args, ent, offset); break;
	case SchemaKeyType::Int32: SetSchemaReturnValue<int32_t>(args, ent, offset); break;
	case SchemaKeyType::Uint32: SetSchemaReturnValue<uint32_t>(args, ent, offset); break;
	case SchemaKeyType::Int64: SetSchemaReturnValue<int64_t>(args, ent, offset); break;
	case SchemaKeyType::Uint64: SetSchemaReturnValue<uint64_t>(args, ent, offset); break;
	case SchemaKeyType::Bool: SetSchemaReturnValue<bool>(args, ent, offset); break;
	case SchemaKeyType::UtlString: SetSchemaReturnValue<CUtlString>(args, ent, offset); break;
	case SchemaKeyType::UtlSymbolLarge: SetSchemaReturnValue<CUtlSymbolLarge>(args, ent, offset); break;
	case SchemaKeyType::GameTime: SetSchemaReturnValue<GameTime_t>(args, ent, offset); break;
	case SchemaKeyType::EntityHandle: SetSchemaReturnValue<CEntityHandle>(args, ent, offset); break;
	case SchemaKeyType::Vector: SetSchemaReturnValue<Vector>(args, ent, offset); break;
	case SchemaKeyType::QAngle: SetSchemaReturnValue<QAngle>(args, ent, offset); break;
	default:
		ThrowFunctionException(context, "This schema field's type is not supported in script");
	}
}

void ScriptDomainCallbacks::ShowHudHintAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto message = UnwrapArg<std::string>(context, 0);
	auto isAlert = UnwrapArg<bool>(context, 1, true).value_or(false);

	if (!message)
		return;

	ClientPrintAll(isAlert ? HUD_PRINTALERT : HUD_PRINTCENTER, message->c_str());
}

void ScriptDomainCallbacks::AddSampleCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);
	auto script = (CCSScript_EntityScript*)ScriptExtensions::GetCurrentCsScriptInstance();

	if(args.Length() < 1 || !args[0]->IsFunction())
	{
		ThrowFunctionException(context, "First argument must be a function.");
		return;
	}
	auto callback = args[0].As<v8::Function>();
	script->AddCallback("OnSampleCallback", callback);
}

void ScriptDomainCallbacks::SetEntityMoveType(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto entHandle = UnwrapThis<CEntityHandle>(context);
	auto moveType = UnwrapArg<uint8>(context, 0, false);
	if (!entHandle || !moveType)
		return;

	if (entHandle->IsValid())
		static_cast<CBaseEntity*>(entHandle->Get())->SetMoveType(static_cast<MoveType_t>(*moveType));
}

void ScriptDomainCallbacks::EmitSound(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args)
	auto v8Context = isolate->GetCurrentContext();

	if (args.Length() != 1)
	{
		ThrowFunctionException(context, "requires an object with EmitSound info");
		return;
	}
	if (!args[0]->IsObject())
	{
		ThrowFunctionException(context, "argument 0 must be an object {soundName: string, source?: Entity, volume?: number, pitch?: number, recipients?: CSPlayerController[]}\n");
		return;
	}

	auto obj = args[0]->ToObject(v8Context).ToLocalChecked();
	auto maybeSoundName = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "soundName").ToLocalChecked());
	if (maybeSoundName.IsEmpty())
		return;
	auto soundName = maybeSoundName.ToLocalChecked();
	if (!soundName->IsString())
	{
		ThrowFunctionException(context, "argument 0.msgName must be a string\n");
		return;
	}
	v8::String::Utf8Value soundNameUtf8(isolate, soundName);
	auto maybeSourceEntity = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "source").ToLocalChecked());
	// entIndex -1 should be fine, it will just play globally.
	CEntityIndex entIndex = -1;
	if (!maybeSourceEntity.IsEmpty())
	{
		auto sourceEntityVal = maybeSourceEntity.ToLocalChecked();
		if (!sourceEntityVal->IsUndefined())
		{
			if (!sourceEntityVal->IsObject())
			{
				V8ThrowException(isolate, "EmitSound argument 0.source must be an Entity");
				return;
			}
			auto sourceEntityObj = sourceEntityVal.As<v8::Object>();
			CEntityHandle entHandle = ScriptExtensions::GetEntityHandleFromScriptObject(sourceEntityObj);
			if (!entHandle.IsValid())
			{
				ThrowFunctionException(context, "argument 0.source is not a valid Entity");
				return;
			}
			entIndex = entHandle.GetEntryIndex();
		}
	}

	float volume = 1.0f;
	auto maybeVolume = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "volume").ToLocalChecked());
	if (!maybeVolume.IsEmpty())
	{
		auto volumeVal = maybeVolume.ToLocalChecked();
		if (!volumeVal->IsUndefined())
		{
			if (!volumeVal->IsNumber())
			{
				ThrowFunctionException(context, "argument 0.volume must be a number");
				return;
			}
			volume = static_cast<float>(volumeVal.As<v8::Number>()->Value());
		}

		if (volume < 0.0f)
			volume = 0.0f;
	}

	int pitch = 1;
	auto maybePitch = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "pitch").ToLocalChecked());
	if (!maybePitch.IsEmpty())
	{
		auto pitchVal = maybePitch.ToLocalChecked();
		if (!pitchVal->IsUndefined())
		{
			if (!pitchVal->IsNumber())
			{
				ThrowFunctionException(context, "argument 0.pitch must be a number");
				return;
			}
			pitch = static_cast<int>(pitchVal.As<v8::Number>()->Value());
		}
			
		if (pitch < 0)
			pitch = 0;
	}

	CRecipientFilter filter;
	auto maybeRecipientsArr = obj->Get(v8Context, v8::String::NewFromUtf8(isolate, "recipients").ToLocalChecked());
	if (!maybeRecipientsArr.IsEmpty())
	{
		if (const auto recipientsVal = maybeRecipientsArr.ToLocalChecked(); recipientsVal->IsArray())
		{
			auto recipientsArr = recipientsVal.As<v8::Array>();
			uint32_t length = recipientsArr->Length();
			for (uint32_t i = 0; i < length; ++i)
			{
				auto recipientVal = recipientsArr->Get(v8Context, i).ToLocalChecked();
				if (!recipientsVal->IsUndefined() && !recipientVal->IsObject())
				{
					ThrowFunctionException(context, "argument 0.recipients must be an array of CSPlayerController objects");
					return;
				}
				auto recipientObj = recipientVal.As<v8::Object>();
				auto recipientEntHandle = ScriptExtensions::GetEntityHandleFromScriptObject(recipientObj);
				if (!recipientEntHandle.IsValid())
					continue;

				auto recipientController = static_cast<CCSPlayerController*>(recipientEntHandle.Get());
				if (!recipientController || !recipientController->IsController())
				{
					ThrowFunctionException(context, "argument 0.recipients must be an array of CSPlayerController objects");
					return;
				}
				filter.AddRecipient(recipientController->GetPlayerSlot());
			}
		}
		else
		{
			if (recipientsVal->IsUndefined())
			{
				filter.AddAllPlayers();
			}
			else
			{
				ThrowFunctionException(context, "argument 0.recipients must be an array of CSPlayerController objects");
				return;
			}
		}

	}
	EmitSound_t emitSoundInfo;
	emitSoundInfo.m_pSoundName = *soundNameUtf8;
	emitSoundInfo.m_flVolume = volume;
	emitSoundInfo.m_nPitch = static_cast<int>(pitch);
	addresses::CBaseEntity_EmitSoundFilter(filter, entIndex, emitSoundInfo);
}

void ScriptDomainCallbacks::SetTransmitState(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);
	auto v8Context = isolate->GetCurrentContext();

	auto targetEnt = UnwrapThis<CEntityHandle>(context);
	auto plrEnt = UnwrapArg<CEntityHandle>(context, 0, false);
	auto state = UnwrapArg<bool>(context, 1, false);

	if (!targetEnt || !plrEnt || !state)
		return;

	if (!targetEnt->IsValid())
	{
		ThrowFunctionException(context, "failed to get entity from 'this' object.");
		return;
	}

	auto targetEntValue = static_cast<CBaseEntity*>(targetEnt->Get());
	if (!targetEntValue || targetEntValue->IsController())
	{
		ThrowFunctionException(context, "can not change transmit state on a player controller");
		return;
	}

	if (!plrEnt->IsValid())
	{
		ThrowFunctionException(context, "target player entity is not valid");
		return;
	}

	auto targetPlr = static_cast<CCSPlayerController*>(plrEnt->Get());
	if (!targetPlr || !targetPlr->IsController())
	{
		ThrowFunctionException(context, "target player entity is not a player controller");
		return;
	}

	auto entIndex = targetEnt->GetEntryIndex();
	g_playerManager.SetEntityTransmitBlocked(targetPlr->GetPlayerSlot(), entIndex, !*state);
}

void ScriptDomainCallbacks::SetTransmitStateAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto targetEntHandle = UnwrapThis<CEntityHandle>(context);
	auto state = UnwrapArg<bool>(context, 0, false);

	if(!targetEntHandle || !state)
		return;

	if (!targetEntHandle->IsValid())
	{
		ThrowFunctionException(context, "failed to get entity from 'this' object.");
		return;
	}
	
	auto entity = static_cast<CBaseEntity*>(targetEntHandle->Get());
	if (!entity || entity->IsController())
	{
		ThrowFunctionException(context, "can not set transmit state on player controllers");
		return;
	}

	auto entIndex = targetEntHandle->GetEntryIndex();
	g_playerManager.SetEntityTransmitBlockedForAll(entIndex, !*state);
}

void ScriptDomainCallbacks::GetConVarValue(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);
	auto v8Context = isolate->GetCurrentContext();

	auto cvarName = UnwrapArg<std::string>(context, 0);
	if (!cvarName)
		return;

	ConVarRefAbstract cvar(cvarName->c_str());
	if (!cvar.IsValidRef())
	{
		ThrowFunctionException(context, std::format("ConVar {} does not exist", *cvarName));
		return;
	}

	if (!cvar.IsConVarDataAvailable())
	{
		ThrowFunctionException(context, std::format("ConVar {} was registered partially, it's not possible to retrieve it's data", *cvarName));
		return;
	}

	if (cvar.IsFlagSet(FCVAR_PROTECTED) || cvar.IsFlagSet(FCVAR_DONTRECORD))
	{
		ThrowFunctionException(context, std::format("ConVar {} is protected or marked as 'dontrecord', its value cannot be accessed", *cvarName));
		return;
	}

	auto cvarType = cvar.GetType();
	switch (cvarType)
	{
	case EConVarType_Bool:
		args.GetReturnValue().Set(v8::Boolean::New(isolate, cvar.GetBool())); break;
	case EConVarType_Int16:
	case EConVarType_UInt16:
	case EConVarType_Int32:
	case EConVarType_UInt32:
	case EConVarType_Int64:
	case EConVarType_UInt64:
		args.GetReturnValue().Set(v8::Number::New(isolate, cvar.GetInt())); break;
	case EConVarType_Float32:
	case EConVarType_Float64:
		args.GetReturnValue().Set(v8::Number::New(isolate, cvar.GetFloat())); break;
	
	case EConVarType_Vector3:
	{
		auto vec = cvar.GetAs<Vector>();
		auto vecObj = CreateVectorObject(v8Context, vec);
		args.GetReturnValue().Set(vecObj);
		break;
	}
	case EConVarType_Qangle:
	{
		auto ang = cvar.GetAs<QAngle>();
		auto angObj = CreateQAngleObject(v8Context, ang);
		args.GetReturnValue().Set(angObj);
		break;
	}
	case EConVarType_Color:
	{
		auto clr = cvar.GetAs<Color>();
		auto clrObj = CreateColorObject(v8Context, clr);
		args.GetReturnValue().Set(clrObj);
		break;
	}
	case EConVarType_String:
	default: // some types aren't handled here, just return as string.
		args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, cvar.GetString()).ToLocalChecked());
	}
}

void ScriptDomainCallbacks::PrintToChatAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto message = UnwrapArg<std::string>(context, 0);

	if (!message)
		return;

	ClientPrintAll(HUD_PRINTTALK, message->c_str());
}

void ScriptDomainCallbacks::OnDispatchClientCommand(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto callbackVal = UnwrapArg<v8::Local<v8::Value>>(context, 0);
	if (!callbackVal)
		return;

	if (!(*callbackVal)->IsFunction())
	{
		ThrowFunctionException(context, "provided argument must be a function");
		return;
	}

	auto callback = (*callbackVal).As<v8::Function>();
	const auto script = ScriptExtensions::GetCurrentCsScriptInstance();
	if(script)
		script->AddCallback("OnDispatchClientCommand", callback);
}

void ScriptDomainCallbacks::OnClientCommand(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto callbackVal = UnwrapArg<v8::Local<v8::Value>>(context, 0);
	if (!callbackVal)
		return;

	if (!(*callbackVal)->IsFunction())
	{
		ThrowFunctionException(context, "provided argument must be a function");
		return;
	}

	auto callback = (*callbackVal).As<v8::Function>();
	const auto script = ScriptExtensions::GetCurrentCsScriptInstance();
	if(script)
		script->AddCallback("OnClientCommand", callback);
}

template<typename T>
inline constexpr bool always_false_v = false;

template <typename T>
constexpr void ScriptDomainCallbacks::SetSchemaReturnValue(const v8::FunctionCallbackInfo<v8::Value>& args, void* ent, size_t offset)
{
	auto val = *reinterpret_cast<std::add_pointer_t<T>>(static_cast<unsigned char*>(ent) + offset);

	if constexpr (std::is_arithmetic_v<T>)
	{
		args.GetReturnValue().Set(v8::Number::New(args.GetIsolate(), val));
	}
	else if constexpr (std::is_same_v<T, bool>)
	{
		args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), val));
	}
	else if constexpr (std::is_same_v<T, CUtlString>)
	{
		args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(), val.Get()).ToLocalChecked());
	}
	else if constexpr (std::is_same_v<T, CUtlSymbolLarge>)
	{
		args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(), val.String()).ToLocalChecked());
	}
	else if constexpr (std::is_same_v<T, GameTime_t>)
	{
		args.GetReturnValue().Set(v8::Number::New(args.GetIsolate(), val.GetTime()));
	}
	else if constexpr (std::is_same_v<T, CEntityHandle>)
	{
		if (val.IsValid())
		{
			auto obj = ScriptExtensions::CreateEntityObjectAuto(val.Get());
			args.GetReturnValue().Set(obj);
		}
	}
	else if constexpr (std::is_same_v<T, Vector>)
	{
		auto isolate = args.GetIsolate();
		auto context = isolate->GetCurrentContext();
		auto obj = CreateVectorObject(context, val);
		args.GetReturnValue().Set(obj);
	}
	else if constexpr (std::is_same_v<T, QAngle>)
	{
		auto isolate = args.GetIsolate();
		auto context = isolate->GetCurrentContext();
		auto obj = CreateQAngleObject(context, val);
		args.GetReturnValue().Set(obj);
	}
	else 
	{
		static_assert(always_false_v<T>, "Unsupported type for SetSchemaReturnValue");
	}
}

void ScriptDomainCallbacks::CreateUserMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto messageName = UnwrapArg<std::string>(context, 0);

	if (!messageName)
		return;

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial(messageName->c_str());
	if(!pNetMsg)
	{
		ThrowFunctionException(context, std::format("failed to find message with name '{}'", *messageName));
		return;
	}

	auto data = pNetMsg->AllocateMessage()->ToPB<google::protobuf::Message>();
	if(!data)
	{
		ThrowFunctionException(context, std::format("failed to allocate message for '{}'", *messageName));
		return;
	}

	ScriptUserMessageInfo* msgInfo = new ScriptUserMessageInfo(data, 0, pNetMsg);
	auto script = (CCSScript_EntityScript*)ScriptExtensions::GetCurrentCsScriptInstance();
	auto msg = ScriptUserMessage::CreateUserMessageInfoInstance(script, msgInfo);
	args.GetReturnValue().Set(msg);
}