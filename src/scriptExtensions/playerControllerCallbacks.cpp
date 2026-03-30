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
#include "scriptExtensions/playerControllerCallbacks.h"
#include "scriptExtensions/scriptcommon.h"
#include "entity/ccsplayercontroller.h"
#include "hudhintmanager.h"

void ClientPrint(CCSPlayerController* player, int hud_dest, const char* msg, ...);
extern HudHintManager g_hudHintManager;

void ScriptPlayerControllerCallbacks::GetSteamID(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("CSPlayerController", "GetSteamID"))
		return;

	if (!args.This()->IsObject())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID invoked with incorrect 'this' value.");
		return;
	}

	auto entHandle = ScriptExtensions::GetEntityHandleFromScriptObject(args.This());
	if (!entHandle.IsValid())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID invoked with incorrect 'this' value.");
		return;
	}

	auto ent = static_cast<CBaseEntity*>(entHandle.Get());
	if (!ent || !ent->IsController())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID invoked with incorrect 'this' value.");
		return;
	}

	auto playerController = reinterpret_cast<CCSPlayerController*>(ent);
	if (!playerController->IsConnected())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID: Target player is not connected.");
		return;
	}

	args.GetReturnValue().Set(v8::Number::New(isolate, playerController->m_steamID()));
}

void ScriptPlayerControllerCallbacks::ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
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

void ScriptPlayerControllerCallbacks::ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args)
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

void ScriptPlayerControllerCallbacks::Respawn(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);

	if (!VerifyScriptScope("CSPlayerController", "GetSteamID"))
		return;

	if (!args.This()->IsObject())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID invoked with incorrect 'this' value.");
		return;
	}

	auto entHandle = ScriptExtensions::GetEntityHandleFromScriptObject(args.This());
	if (!entHandle.IsValid())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID invoked with incorrect 'this' value.");
		return;
	}

	auto ent = static_cast<CBaseEntity*>(entHandle.Get());
	if (!ent || !ent->IsController())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID invoked with incorrect 'this' value.");
		return;
	}

	auto playerController = reinterpret_cast<CCSPlayerController*>(ent);
	if (!playerController->IsConnected())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID: Target player is not connected.");
		return;
	}

	playerController->Respawn();
}
