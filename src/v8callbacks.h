#pragma once
#include "v8-function.h"

namespace V8Callbacks {
	void V8NewMsg(const v8::FunctionCallbackInfo<v8::Value>& info);
	void V8GetSchemaField(const v8::FunctionCallbackInfo<v8::Value>& args);
	void V8ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
	void V8ShowHudHintAll(const v8::FunctionCallbackInfo<v8::Value>& args);
	void V8ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args);
	void AddSampleCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
	void SetEntityMoveType(const v8::FunctionCallbackInfo<v8::Value>& args);

	template <typename T>
	constexpr void SetV8NumericReturnValue(const v8::FunctionCallbackInfo<v8::Value>& args, void* ent, size_t offset);
};