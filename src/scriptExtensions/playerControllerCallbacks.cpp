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

void ScriptPlayerControllerCallbacks::GetSteamID(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto isolate = v8::Isolate::GetCurrent();

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

	auto ent = entHandle.Get();
	if (ent->GetEntityIndex().Get() <= 0 || ent->GetEntityIndex().Get() > 65)
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID invoked with incorrect 'this' value.");
	}

	auto playerController = reinterpret_cast<CCSPlayerController*>(ent);
	if (!playerController->IsConnected())
	{
		V8ThrowException(isolate, "CSPlayerController.GetSteamID: Target player is not connected.");
		return;
	}

	args.GetReturnValue().Set(v8::Number::New(isolate, playerController->m_steamID()));
}

