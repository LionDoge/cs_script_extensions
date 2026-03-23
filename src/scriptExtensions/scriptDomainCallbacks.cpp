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

extern LoggingChannelID_t g_logChanScript;

void ClientPrintAll(int hud_dest, const char* msg, ...);
void ClientPrint(CCSPlayerController* player, int hud_dest, const char* msg, ...);
extern ISchemaSystem* g_pSchemaSystem;
extern IGameEventManager2* g_gameEventManager;
extern HudHintManager g_hudHintManager;
extern ScriptExtensions g_scriptExtensions;
extern PlayerManager g_playerManager;

#define V8_SETUP_AND_VERIFY(domain, name) \
	auto isolate = args.GetIsolate(); \
	v8::HandleScope handleScope(isolate); \
	if (!VerifyScriptScope(domain, name)) \
		return;

#define SCRIPT_FUNCTION(name, classname, scopename) \
	void name##_V8ScriptCallback(const v8::FunctionCallbackInfo<v8::Value>& args); \
	g_scriptExtensions.AddNewFunction(classname, scopename, #name, name##_V8ScriptCallback); \
	void name##_V8ScriptCallback(const v8::FunctionCallbackInfo<v8::Value>& args)

void ScriptDomainCallbacks::NewMsg(const v8::FunctionCallbackInfo<v8::Value>& args)
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

void ScriptDomainCallbacks::GetSchemaField(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Entity", "GetSchemaField"))
		return;

	if (!args.This()->IsObject())
	{
		V8ThrowException(isolate, "Method Entity.GetSchemaField invoked with incorrect 'this' value.");
		return;
	}

	if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString())
	{
		V8ThrowException(isolate, "Method Entity.GetSchemaField requires 2 arguments (className: string, fieldName: string)");
		return;
	}

	CEntityHandle entHandle = ScriptExtensions::GetEntityHandleFromScriptObject(args.This().As<v8::Object>());
	if (!entHandle.IsValid())
	{
		V8ThrowException(isolate, "Method Entity.GetSchemaField failed to get entity from 'this' object.");
		return;
	}

	auto ent = static_cast<CBaseEntity*>(entHandle.Get());
	if (!ent)
	{
		V8ThrowException(isolate, "Method Entity.GetSchemaField called on invalid entity instance.");
		return;
	}

	v8::Local<v8::String> v8StrClassname = args[0].As<v8::String>();
	v8::String::Utf8Value v8StrClassnameUtf8(isolate, v8StrClassname);

	v8::Local<v8::String> v8StrFieldname = args[1].As<v8::String>();
	v8::String::Utf8Value v8StrFieldnameUtf8(isolate, v8StrFieldname);

	uint32_t classNameHash = hash_32_fnv1a_const(*v8StrClassnameUtf8);
	uint32_t fieldNameHash = hash_32_fnv1a_const(*v8StrFieldnameUtf8);

	SchemaKey schemaFieldInfo = schema::GetOffset(*v8StrClassnameUtf8, classNameHash, *v8StrFieldnameUtf8, fieldNameHash);

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
		V8ThrowException(isolate, "This schema field's type is not supported in script");
	}
}

void ScriptDomainCallbacks::ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
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
	auto controllerHandle = ScriptExtensions::GetEntityHandleFromScriptObject(args.This().As<v8::Object>());
	if (!controllerHandle.IsValid())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudMessageHTML failed to get entity from 'this' object.");
		return;
	}
	auto controller = dynamic_cast<CCSPlayerController*>(controllerHandle.Get());
	double duration = 0.0;
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

void ScriptDomainCallbacks::ShowHudHintAll(const v8::FunctionCallbackInfo<v8::Value>& args)
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

void ScriptDomainCallbacks::ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args)
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

	CEntityHandle entHandle = ScriptExtensions::GetEntityHandleFromScriptObject(args.This().As<v8::Object>());
	if (!entHandle.IsValid())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.ShowHudHint failed to get entity from 'this' object.");
		return;
	}
	auto controller = dynamic_cast<CCSPlayerController*>(entHandle.Get());
	if (!controller)
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.ShowHudHint can only be called on player controller instances.");
		return;
	}

	v8::String::Utf8Value v8StrTextUtf8(isolate, args[0].As<v8::String>());
	ClientPrint(controller, isAlert ? HUD_PRINTALERT : HUD_PRINTCENTER, *v8StrTextUtf8);
}

void ScriptDomainCallbacks::AddSampleCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = args.GetIsolate();
	auto script = (CCSScript_EntityScript*)ScriptExtensions::GetCurrentCsScriptInstance();
	v8::HandleScope handleScope(isolate);
	
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
	script->AddCallback("OnSampleCallback", callback);
}

void ScriptDomainCallbacks::SetEntityMoveType(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	V8_SETUP_AND_VERIFY("Entity", "SetMoveType");

	if (args.Length() < 1 || !args.This()->IsObject() || !args[0]->IsNumber())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetMoveType requires 1 argument (moveType: number)"); \
		return;
	}

	CEntityHandle entHandle = ScriptExtensions::GetEntityHandleFromScriptObject(args.This().As<v8::Object>());
	if (!entHandle.IsValid())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetMoveType failed to get entity from 'this' object.");
		return;
	}
	auto baseEnt = dynamic_cast<CBaseEntity*>(entHandle.Get());
	if (!baseEnt)
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetMoveType called on invalid entity instance.");
		return;
	}

	int moveType = static_cast<int>(args[0].As<v8::Number>()->Value());
	baseEnt->SetMoveType(static_cast<MoveType_t>(moveType));
}

void ScriptDomainCallbacks::EmitSound(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Domain", "EmitSound"))
		return;
	auto context = isolate->GetCurrentContext();

	if (args.Length() != 1)
	{
		V8ThrowException(args.GetIsolate(), "Method Domain.EmitSound requires an object with EmitSound info");
		return;
	}
	if (!args[0]->IsObject())
	{
		V8ThrowException(isolate, "EmitSound argument 0 must be an object {soundName: string, source?: Entity, volume?: number, pitch?: number, recipients?: CSPlayerController[]}\n");
		return;
	}

	auto obj = args[0]->ToObject(context).ToLocalChecked();
	auto maybeSoundName = obj->Get(context, v8::String::NewFromUtf8(isolate, "soundName").ToLocalChecked());
	if (maybeSoundName.IsEmpty())
		return;
	auto soundName = maybeSoundName.ToLocalChecked();
	if (!soundName->IsString())
	{
		V8ThrowException(isolate, "EmitSound argument 0.msgName must be a string\n");
		return;
	}
	v8::String::Utf8Value soundNameUtf8(isolate, soundName);
	auto maybeSourceEntity = obj->Get(context, v8::String::NewFromUtf8(isolate, "source").ToLocalChecked());
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
				V8ThrowException(isolate, "EmitSound argument 0.source is not a valid Entity");
				return;
			}
			entIndex = entHandle.GetEntryIndex();
		}
	}

	float volume = 1.0f;
	auto maybeVolume = obj->Get(context, v8::String::NewFromUtf8(isolate, "volume").ToLocalChecked());
	if (!maybeVolume.IsEmpty())
	{
		auto volumeVal = maybeVolume.ToLocalChecked();
		if (!volumeVal->IsUndefined())
		{
			if (!volumeVal->IsNumber())
			{
				V8ThrowException(isolate, "EmitSound argument 0.volume must be a number");
				return;
			}
			volume = static_cast<float>(volumeVal.As<v8::Number>()->Value());
		}

		if (volume < 0.0f)
			volume = 0.0f;
	}

	int pitch = 1;
	auto maybePitch = obj->Get(context, v8::String::NewFromUtf8(isolate, "pitch").ToLocalChecked());
	if (!maybePitch.IsEmpty())
	{
		auto pitchVal = maybePitch.ToLocalChecked();
		if (!pitchVal->IsUndefined())
		{
			if (!pitchVal->IsNumber())
			{
				V8ThrowException(isolate, "EmitSound argument 0.pitch must be a number");
				return;
			}
			pitch = static_cast<int>(pitchVal.As<v8::Number>()->Value());
		}
			
		if (pitch < 0)
			pitch = 0;
	}

	CRecipientFilter filter;
	auto maybeRecipientsArr = obj->Get(context, v8::String::NewFromUtf8(isolate, "recipients").ToLocalChecked());
	if (!maybeRecipientsArr.IsEmpty())
	{
		if (const auto recipientsVal = maybeRecipientsArr.ToLocalChecked(); recipientsVal->IsArray())
		{
			auto recipientsArr = recipientsVal.As<v8::Array>();
			uint32_t length = recipientsArr->Length();
			for (uint32_t i = 0; i < length; ++i)
			{
				auto recipientVal = recipientsArr->Get(context, i).ToLocalChecked();
				if (!recipientsVal->IsUndefined() && !recipientVal->IsObject())
				{
					V8ThrowException(isolate, "EmitSound argument 0.recipients must be an array of CSPlayerController objects");
					return;
				}
				auto recipientObj = recipientVal.As<v8::Object>();
				auto recipientEntHandle = ScriptExtensions::GetEntityHandleFromScriptObject(recipientObj);
				if (!recipientEntHandle.IsValid() || recipientEntHandle.GetEntryIndex() > (MAXPLAYERS + 1))
				{
					V8ThrowException(isolate, "EmitSound argument 0.recipients must be an array of CSPlayerController objects");
					return;
				}
				auto recipientController = (CCSPlayerController*)recipientEntHandle.Get();
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
				V8ThrowException(isolate, "EmitSound argument 0.recipients must be an array of CSPlayerController objects");
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
	auto isolate = v8::Isolate::GetCurrent();
	auto context = isolate->GetCurrentContext();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Entity", "SetTransmitState"))
		return;

	if (args.Length() < 2 || !args.This()->IsObject() || !args[0]->IsObject() || !args[1]->IsBoolean())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetTransmitState requires 2 arguments (player: CSPlayerController, state: boolean)");
		return;
	}

	auto targetObj = args.This()->ToObject(context).ToLocalChecked();
	auto targetEnt = ScriptExtensions::GetEntityHandleFromScriptObject(targetObj);
	if (!targetEnt.IsValid())
	{
		V8ThrowException(isolate, "Method Entity.SetTransmitState failed to get entity from 'this' object.");
		return;
	}
	auto entIndex = targetEnt.GetEntryIndex();
	if (entIndex >= 0 && entIndex < MAXPLAYERS + 1)
	{
		V8ThrowException(isolate, "Can not set transmit state on player controllers");
		return;
	}

	auto plrObject = args[0]->ToObject(context).ToLocalChecked();
	CEntityHandle targetPlrHandle = ScriptExtensions::GetEntityHandleFromScriptObject(plrObject);
	if (!targetPlrHandle.IsValid() || targetPlrHandle.GetEntryIndex() <= 0 || targetPlrHandle.GetEntryIndex() > MAXPLAYERS + 1)
	{
		V8ThrowException(isolate, "Method Entity.SetTransmitState first argument must be a CSPlayerController instance");
		return;
	}

	auto targetPlr = (CCSPlayerController*)targetPlrHandle.Get();
	bool state = args[1].As<v8::Boolean>()->Value();
	g_playerManager.SetEntityTransmitBlocked(targetPlr->GetPlayerSlot(), entIndex, !state);
}

void ScriptDomainCallbacks::SetTransmitStateAll(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	auto context = isolate->GetCurrentContext();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Entity", "SetTransmitStateAll"))
		return;

	if (args.Length() < 1 || !args.This()->IsObject() || !args[0]->IsBoolean())
	{
		V8ThrowException(args.GetIsolate(), "Method Entity.SetTransmitStateAll requires 1 argument (state: boolean)");
		return;
	}

	auto targetObj = args.This()->ToObject(context).ToLocalChecked();
	auto targetEntHandle = ScriptExtensions::GetEntityHandleFromScriptObject(targetObj);
	if (!targetEntHandle.IsValid())
	{
		V8ThrowException(isolate, "Method Entity.SetTransmitStateAll failed to get entity from 'this' object.");
		return;
	}
	auto entIndex = targetEntHandle.GetEntryIndex();
	if (entIndex >= 0 && entIndex < MAXPLAYERS + 1)
	{
		V8ThrowException(isolate, "Can not set transmit state on player controllers");
		return;
	}

	bool state = args[1].As<v8::Boolean>()->Value();
	g_playerManager.SetEntityTransmitBlockedForAll(entIndex, !state);
}

void ScriptDomainCallbacks::GetConVarValue(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	auto context = isolate->GetCurrentContext();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Domain", "GetConVarValue"))
		return;

	if (args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.GetConVarValue requires 1 argument (cvar: string)");
		return;
	}

	v8::String::Utf8Value v8StrCvarUtf8(isolate, args[0].As<v8::String>());
	std::string cvarName(*v8StrCvarUtf8);
	ConVarRefAbstract cvar(cvarName.c_str());
	if (!cvar.IsValidRef())
	{
		V8ThrowException(args.GetIsolate(), std::format("GetConVarValue: ConVar {} does not exist", cvarName));
		return;
	}

	if (!cvar.IsConVarDataAvailable())
	{
		V8ThrowException(args.GetIsolate(), std::format("GetConVarValue: ConVar {} was registered partially, and it's not possible to retrieve it's data", cvarName));
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
		auto vecObj = CreateVectorObject(context, vec);
		args.GetReturnValue().Set(vecObj);
		break;
	}
	case EConVarType_Qangle:
	{
		auto ang = cvar.GetAs<QAngle>();
		auto angObj = CreateQAngleObject(context, ang);
		args.GetReturnValue().Set(angObj);
		break;
	}
	case EConVarType_Color:
	{
		auto clr = cvar.GetAs<Color>();
		auto clrObj = CreateColorObject(context, clr);
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
	auto isolate = v8::Isolate::GetCurrent();
	auto context = isolate->GetCurrentContext();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("Domain", "PrintToChatAll"))
		return;

	if (args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(args.GetIsolate(), "Method point_script.PrintToChatAll requires 1 argument (message: string)");
		return;
	}

	v8::String::Utf8Value v8Message(isolate, args[0].As<v8::String>());
	std::string message(*v8Message);

	ClientPrintAll(HUD_PRINTTALK, message.c_str());
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
	V8_SETUP_AND_VERIFY("Domain", "CreateUserMessage");
	if(args.Length() < 1 || !args[0]->IsString())
	{
		V8ThrowException(isolate, "Method Domain.CreateUserMessage requires 1 argument (messageName: string)");
		return;
	}
	v8::String::Utf8Value v8StrMessageNameUtf8(isolate, args[0].As<v8::String>());
	std::string messageName(*v8StrMessageNameUtf8);
	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial(messageName.c_str());
	if(!pNetMsg)
	{
		V8ThrowException(isolate, std::format("Method Domain.CreateUserMessage failed to find message with name '{}'", messageName));
		return;
	}
	auto data = pNetMsg->AllocateMessage()->ToPB<google::protobuf::Message>();
	ScriptUserMessageInfo* msgInfo = new ScriptUserMessageInfo(data, 0, pNetMsg);
	auto script = (CCSScript_EntityScript*)ScriptExtensions::GetCurrentCsScriptInstance();
	if (!script)
	{
		V8ThrowException(isolate, "Method Domain.CreateUserMessage invoked in incorrect scope.");
		delete msgInfo;
		return;
	}

	auto msg = ScriptUserMessage::CreateUserMessageInfoInstance(script, msgInfo);
	args.GetReturnValue().Set(msg);
}