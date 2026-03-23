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
#include <format>
#include <string_view>
#include <v8-primitive.h>
#include <v8-isolate.h>
#include "scriptExtensions.h"

extern LoggingChannelID_t g_logChanScript;
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

inline bool VerifyScriptScope(const std::string_view& instName, const std::string_view& methodName)
{
	if (!ScriptExtensions::GetCurrentCsScriptInstance())
	{
		V8ThrowException(
			v8::Isolate::GetCurrent(),
			std::format("Method {}.{} invoked in incorrect scope.", instName, methodName)
		);
		return false;
	}
	return true;
}

// These functions create common JS object from C++ structs, they adhere to the default definitions.

inline v8::Local<v8::Object> CreateVectorObject(v8::Local<v8::Context> context, const Vector& vec)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::EscapableHandleScope handleScope(isolate);

	auto obj = v8::Object::New(isolate);
	obj->Set(context, v8::String::NewFromUtf8(isolate, "x").ToLocalChecked(), v8::Number::New(isolate, vec.x));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "y").ToLocalChecked(), v8::Number::New(isolate, vec.y));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "z").ToLocalChecked(), v8::Number::New(isolate, vec.z));

	return handleScope.Escape(obj);
}

inline v8::Local<v8::Object> CreateQAngleObject(v8::Local<v8::Context> context, const QAngle& ang)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::EscapableHandleScope handleScope(isolate);

	auto obj = v8::Object::New(isolate);
	obj->Set(context, v8::String::NewFromUtf8(isolate, "pitch").ToLocalChecked(), v8::Number::New(isolate, ang.x));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "yaw").ToLocalChecked(), v8::Number::New(isolate, ang.y));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "roll").ToLocalChecked(), v8::Number::New(isolate, ang.z));

	return handleScope.Escape(obj);
}

inline v8::Local<v8::Object> CreateColorObject(v8::Local<v8::Context> context, const Color& clr)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::EscapableHandleScope handleScope(isolate);

	auto obj = v8::Object::New(isolate);
	obj->Set(context, v8::String::NewFromUtf8(isolate, "r").ToLocalChecked(), v8::Number::New(isolate, clr.r()));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "g").ToLocalChecked(), v8::Number::New(isolate, clr.g()));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked(), v8::Number::New(isolate, clr.b()));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked(), v8::Number::New(isolate, clr.a()));

	return handleScope.Escape(obj);
}