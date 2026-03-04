#pragma once
#include <v8-value.h>
#include "userMessageInfo.h"
#include "src/csscript.h"

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