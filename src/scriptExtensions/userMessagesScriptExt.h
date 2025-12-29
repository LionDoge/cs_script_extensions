#pragma once
#include <v8-value.h>
#include "userMessageInfo.h"
#include "src/csscript.h"

namespace V8CallbacksUserMsg {
	void OnUserMessage(const v8::FunctionCallbackInfo<v8::Value>& info);
	void InitUserMessageInfoTemplate(CCSScript_EntityScript* script);
	void UserMessageInfo_GetField(const v8::FunctionCallbackInfo<v8::Value>& info);
	std::optional<v8::Local<v8::Value>> CreateUserMessageInfoInstance(CCSScript_EntityScript* script, ScriptUserMessageInfo* userMsgInfo);
};