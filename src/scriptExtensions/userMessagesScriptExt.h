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

#pragma once
#include <v8-value.h>
#include "userMessageInfo.h"
#include "csscript.h"

namespace ScriptUserMessage {
	// Callback registration
	void OnUserMessage(const v8::FunctionCallbackInfo<v8::Value>& info);

	// Not used anymore, but serves as an example of custom template registration
	void InitUserMessageInfoTemplate(CCSBaseScript* script);
	v8::Local<v8::Value> CreateUserMessageInfoInstance(CCSScript_EntityScript* script, ScriptUserMessageInfo* userMsgInfo);
	ScriptUserMessageInfo* GetUserMessageInfoObject(const v8::FunctionCallbackInfo<v8::Value>& info);

	// Object methods
	void UserMessageInfo_GetField(const v8::FunctionCallbackInfo<v8::Value>& info);
	void UserMessageInfo_SetField(const v8::FunctionCallbackInfo<v8::Value>& info);
	void UserMessageInfo_ClearRecipients(const v8::FunctionCallbackInfo<v8::Value>& info);
	void UserMessageInfo_AddRecipient(const v8::FunctionCallbackInfo<v8::Value>& info);
	void UserMessageInfo_AddAllRecipients(const v8::FunctionCallbackInfo<v8::Value>& info);
	void UserMessageInfo_RemoveRecipient(const v8::FunctionCallbackInfo<v8::Value>& info);
	void UserMessageInfo_Send(const v8::FunctionCallbackInfo<v8::Value>& info);
};