#pragma once
#include <v8-value.h>
#include "userMessageInfo.h"
#include "src/csscript.h"

namespace ScriptUserMessage {
	// Callback registration
	void OnUserMessage(const v8::FunctionCallbackInfo<v8::Value>& info);

	void InitUserMessageInfoTemplate(CCSBaseScript* script);
	v8::Local<v8::Value> CreateUserMessageInfoInstance(CCSScript_EntityScript* script, ScriptUserMessageInfo* userMsgInfo);

	// Object methods
	void UserMessageInfo_GetField(const v8::FunctionCallbackInfo<v8::Value>& info);
};