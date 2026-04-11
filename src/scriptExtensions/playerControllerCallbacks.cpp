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
#include "pluginconfig.h"

void ClientPrint(CCSPlayerController* player, int hud_dest, const char* msg, ...);
extern HudHintManager g_hudHintManager;

void ScriptPlayerControllerCallbacks::GetSteamID(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto targetEntHandle = UnwrapThis<CEntityHandle>(context);
	if (!targetEntHandle)
		return;

	if (!targetEntHandle->IsValid())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}

	auto ent = static_cast<CBaseEntity*>(targetEntHandle->Get());
	if (!ent || !ent->IsController())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}

	auto playerController = static_cast<CCSPlayerController*>(ent);
	if (!playerController->IsConnected())
	{
		ThrowFunctionException(context, "target player is not connected.");
		return;
	}

	args.GetReturnValue().Set(v8::Number::New(isolate, playerController->m_steamID()));
}

void ScriptPlayerControllerCallbacks::ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto controllerHandle = UnwrapThis<CEntityHandle>(context);
	auto message = UnwrapArg<std::string>(context, 0);
	auto duration = UnwrapArg<double>(context, 1, true).value_or(0);

	if (!controllerHandle || !message)
		return;

	if (!g_pluginConfig.AreClientNetworkRequestsEnabled() && V_stristr(message->c_str(), "<img") != nullptr)
	{
		ThrowFunctionException(context, "'img' tag is not allowed in HTML message per server's configuration.");
		return;
	}

	if (!controllerHandle->IsValid())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}
	auto controller = static_cast<CCSPlayerController*>(controllerHandle->Get());
	if(!controller || !controller->IsController())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}

	if (duration < 0.0)
		duration = 0.0;

	g_hudHintManager.AddHintMessage(controller->GetPlayerSlot(), *message, duration);
}

void ScriptPlayerControllerCallbacks::ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto controllerHandle = UnwrapThis<CEntityHandle>(context);
	auto message = UnwrapArg<std::string>(context, 0);
	auto isAlert = UnwrapArg<bool>(context, 1, true).value_or(false);

	if(!controllerHandle || !message)
		return;

	if (!controllerHandle->IsValid())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}
	auto controller = static_cast<CCSPlayerController*>(controllerHandle->Get());
	if (!controller || !controller->IsController())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}

	ClientPrint(controller, isAlert ? HUD_PRINTALERT : HUD_PRINTCENTER, message->c_str());
}

void ScriptPlayerControllerCallbacks::Respawn(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto controllerHandle = UnwrapThis<CEntityHandle>(context);

	if (!controllerHandle)
		return;

	if (!controllerHandle->IsValid())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}
	auto controller = static_cast<CCSPlayerController*>(controllerHandle->Get());
	if (!controller || !controller->IsController())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return;
	}

	controller->Respawn();
}
