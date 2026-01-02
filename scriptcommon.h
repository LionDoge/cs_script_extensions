#pragma once
#include <string_view>
#include <v8-primitive.h>
#include <v8-isolate.h>

// Common utility functions that can be used in script callbacks
inline void V8ThrowException(v8::Isolate* isolate, const std::string_view& message)
{
	auto v8ExceptionText = v8::String::NewFromUtf8(isolate, message.data()).ToLocalChecked();
	isolate->ThrowException(v8ExceptionText);
}

inline void V8FakeObjectConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	V8ThrowException(isolate, "Cannot be constructed from script\n");
}