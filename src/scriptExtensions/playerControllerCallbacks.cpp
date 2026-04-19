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
#include "scriptExtensions/scriptcommon_entities.h"
#include "entity/ccsplayercontroller.h"
#include "hudhintmanager.h"
#include "pluginconfig.h"

void ClientPrint(CCSPlayerController* player, int hud_dest, const char* msg, ...);
extern HudHintManager g_hudHintManager;

void ScriptPlayerControllerCallbacks::GetSteamID(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto playerController = UnwrapThis<CCSPlayerController*>(context);
	if (!playerController)
		return;

	if (!(*playerController)->IsConnected())
	{
		ThrowFunctionException(context, "target player is not connected.");
		return;
	}

	args.GetReturnValue().Set(v8::Number::New(isolate, (*playerController)->m_steamID()));
}

void ScriptPlayerControllerCallbacks::ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto playerController = UnwrapThis<CCSPlayerController*>(context);
	auto message = UnwrapArg<std::string>(context, 0);
	auto duration = UnwrapArg<double>(context, 1, true).value_or(0);

	if (!playerController || !message)
		return;

	if (!g_pluginConfig.AreClientNetworkRequestsEnabled() && V_stristr(message->c_str(), "<img") != nullptr)
	{
		ThrowFunctionException(context, "'img' tag is not allowed in HTML message per server's configuration.");
		return;
	}

	if (duration < 0.0)
		duration = 0.0;

	g_hudHintManager.AddHintMessage((*playerController)->GetPlayerSlot(), *message, duration);
}

void ScriptPlayerControllerCallbacks::ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto playerController = UnwrapThis<CCSPlayerController*>(context);
	auto message = UnwrapArg<std::string>(context, 0);
	auto isAlert = UnwrapArg<bool>(context, 1, true).value_or(false);

	if(!playerController || !message)
		return;

	ClientPrint(*playerController, isAlert ? HUD_PRINTALERT : HUD_PRINTCENTER, message->c_str());
}

void ScriptPlayerControllerCallbacks::Respawn(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto playerController = UnwrapThis<CCSPlayerController*>(context);

	if (!playerController)
		return;

	(*playerController)->Respawn();
}

void ScriptPlayerControllerCallbacks::PrintToChat(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	SCRIPT_SETUP(args);

	auto playerController = UnwrapThis<CCSPlayerController*>(context);
	auto message = UnwrapArg<std::string>(context, 0);

	if (!playerController || !message)
		return;

	ClientPrint(*playerController, HUD_PRINTTALK, message->c_str());
}
