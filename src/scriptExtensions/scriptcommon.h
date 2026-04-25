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
#include "scriptextensions.h"
#include "managedObject.h"

extern LoggingChannelID_t g_logChanScript;

struct CallContext {
	v8::Isolate* isolate;
	const v8::FunctionCallbackInfo<v8::Value>& args;
	const char* name;
};

/* Common utility functions that can be used in script callbacks */

inline void V8ThrowException(v8::Isolate* isolate, const std::string_view& message)
{
	auto v8ExceptionText = v8::String::NewFromUtf8(isolate, message.data()).ToLocalChecked();
	isolate->ThrowException(v8ExceptionText);
}

inline void ThrowFunctionException(const CallContext& ctx, const std::string_view& message)
{
	V8ThrowException(ctx.isolate, std::format("{} {}", ctx.name, message));
}

inline void V8FakeObjectConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = info.GetIsolate();
	v8::HandleScope handleScope(isolate);
	V8ThrowException(isolate, "Cannot be constructed from script\n");
}

inline bool VerifyScriptScope(const CallContext& context)
{
	if (!ScriptExtensions::GetCurrentCsScriptInstance())
	{
		V8ThrowException(
			context.isolate,
			std::format("Method {} invoked in incorrect script scope.", context.name)
		);
		return false;
	}
	return true;
}

template <typename T>
concept arithmetic = std::integral<T> || std::floating_point<T>;

template <typename T>
inline std::optional<T> UnwrapThis(const CallContext& context) = delete;

inline std::optional<CEntityHandle> ExtractEntityHandleFromObject(v8::Isolate* isolate, v8::Local<v8::Object> obj, const char* contextName)
{
	if (obj->InternalFieldCount() != 3)
	{
		V8ThrowException(isolate,
			std::format("{} invoked with an invalid 'this' value (mismatched internal field count).", contextName)
		);
		return std::nullopt;
	}

	// This might look quite dumb, but native type markers are just pointer values, and the native code checks for literal pointer value, which we do not have.
	// It's fine to not check for that, if in the whole code base no one defined something else than a pointer value inside their custom object at field 0
	// But I trust no one...
	// JS can't manipulate internal fields anyways, but it can bind any object as 'this' to our functions which could cause unfun security issues if unchecked.
	auto marker = (uint32_t*)obj->GetAlignedPointerFromInternalField(0);
	if (!modules::server->IsAddressInSectionRange(marker, ".data") || *marker != 1)
	{
		V8ThrowException(isolate,
			std::format("{} invoked with an invalid 'this' value (internal field 0 is not valid).", contextName)
		);
		return std::nullopt;
	}

	CSScriptHandle* scriptHandle = (CSScriptHandle*)obj->GetAlignedPointerFromInternalField(1);
	if (!scriptHandle || scriptHandle->typeIdentifier != ScriptHandleType::Entity)
	{
		V8ThrowException(isolate,
			std::format("{} invoked with an invalid 'this' value (not an entity type).", contextName)
		);
		return std::nullopt;
	}
	return scriptHandle->handle;
}

template <>
inline std::optional<CEntityHandle> UnwrapThis(const CallContext& context)
{
	auto obj = context.args.This();
	return ExtractEntityHandleFromObject(context.isolate, obj, context.name);
}

template <typename T>
inline std::optional<ManagedObject<T>*> UnwrapThisAsManagedObject(const CallContext& context, ScriptTypeMarker* typeMarker)
{
	auto obj = context.args.This();
	// Generally we need minimum of 2 internal fields for this type, if we want to be able to do anything with it.
	// Indeed we are comparing by the pointer value, as it's supposed to be one global.
	// This actually mimics the behavior of official functions as well.
	if (obj->InternalFieldCount() < 2 ||
		!typeMarker || obj->GetAlignedPointerFromInternalField(0) != typeMarker)
	{
		V8ThrowException(context.isolate,
			std::format("{} invoked with an invalid 'this' value.", context.name)
		);
		return std::nullopt;
	}

	return static_cast<ManagedObject<T>*>(obj->GetAlignedPointerFromInternalField(1));
}

template <typename T>
inline std::optional<T> UnwrapArg(const CallContext& context, int argIndex, bool isOptional = false)
{
	if (isOptional && argIndex >= context.args.Length())
		return std::nullopt;

	if (argIndex >= context.args.Length())
	{
		ThrowFunctionException(context, std::format("Argument number {} is required but was not provided.", argIndex));
		return std::nullopt;
	}

	return context.args[argIndex];
}

template <>
inline std::optional<std::string> UnwrapArg<std::string>(const CallContext& context, int argIndex, bool isOptional)
{
	if (isOptional && argIndex >= context.args.Length())
		return std::nullopt;

	if (argIndex >= context.args.Length() || !context.args[argIndex]->IsString())
	{
		V8ThrowException(
			context.isolate,
			std::format("{}: Argument number {} is expected to be a string", context.name, argIndex)
		);
		return std::nullopt;
	}
	v8::String::Utf8Value utf8Value(context.isolate, context.args[argIndex].As<v8::String>());
	return std::string(*utf8Value);
}

template <arithmetic T>
inline std::optional<T> UnwrapArg(const CallContext& context, int argIndex, bool isOptional)
{
	if (isOptional && argIndex >= context.args.Length())
		return std::nullopt;

	if (argIndex >= context.args.Length() || !context.args[argIndex]->IsNumber())
	{
		V8ThrowException(
			context.isolate,
			std::format("{}: Argument number {} is expected to be a number", context.name, argIndex)
		);
		return std::nullopt;
	}
	return static_cast<T>(context.args[argIndex].As<v8::Number>()->Value());
}

template <>
inline std::optional<bool> UnwrapArg<bool>(const CallContext& context, int argIndex, bool isOptional)
{
	if (isOptional && argIndex >= context.args.Length())
		return std::nullopt;

	if (argIndex >= context.args.Length() || !context.args[argIndex]->IsBoolean())
	{
		V8ThrowException(
			context.isolate,
			std::format("{}: Argument number {} is expected to be a bool", context.name, argIndex)
		);
		return std::nullopt;
	}

	return context.args[argIndex].As<v8::Boolean>()->Value();
}

template <>
inline std::optional<CEntityHandle> UnwrapArg(const CallContext& context, int argIndex, bool isOptional)
{
	if (isOptional && argIndex >= context.args.Length())
		return std::nullopt;

	if (argIndex >= context.args.Length() || !context.args[argIndex]->IsObject())
	{
		V8ThrowException(
			context.isolate,
			std::format("{}: Argument number {} is expected to be an object", context.name, argIndex)
		);
		return std::nullopt;
	}
	auto obj = context.args[argIndex]->ToObject(context.isolate->GetCurrentContext()).ToLocalChecked();
	return ExtractEntityHandleFromObject(context.isolate, obj, context.name);
}

// Boilerplate. Put this at the beginning of each callback to be able to use some of the getters that are provided here.
#define SCRIPT_SETUP(args) \
	auto isolate = args.GetIsolate(); \
	CallContext context {isolate, args, __func__}; \
	if(!VerifyScriptScope(context)) \
		return; \
	v8::HandleScope handleScope(isolate);


// These functions create common JS object from C++ structs, they adhere to the native definitions.

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

inline std::optional<Vector> ObjectToVector(v8::Local<v8::Context> context, v8::Local<v8::Object> obj)
{
	auto isolate = context->GetIsolate();

	auto maybeX = obj->Get(context, v8::String::NewFromUtf8(isolate, "x").ToLocalChecked());
	auto maybeY = obj->Get(context, v8::String::NewFromUtf8(isolate, "y").ToLocalChecked());
	auto maybeZ = obj->Get(context, v8::String::NewFromUtf8(isolate, "z").ToLocalChecked());

	if (maybeX.IsEmpty() || maybeY.IsEmpty() || maybeZ.IsEmpty())
		return std::nullopt;

	auto xVal = maybeX.ToLocalChecked();
	auto yVal = maybeY.ToLocalChecked();
	auto zVal = maybeZ.ToLocalChecked();

	if (!xVal->IsNumber() || !yVal->IsNumber() || !zVal->IsNumber())
		return std::nullopt;

	return Vector{ 
		static_cast<float>(xVal.As<v8::Number>()->Value()),
		static_cast<float>(yVal.As<v8::Number>()->Value()),
		static_cast<float>(zVal.As<v8::Number>()->Value())
	};
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

inline std::optional<QAngle> ObjectToQAngle(v8::Local<v8::Context> context, v8::Local<v8::Object> obj)
{
	auto isolate = context->GetIsolate();

	auto maybePitch = obj->Get(context, v8::String::NewFromUtf8(isolate, "pitch").ToLocalChecked());
	auto maybeYaw = obj->Get(context, v8::String::NewFromUtf8(isolate, "yaw").ToLocalChecked());
	auto maybeRoll = obj->Get(context, v8::String::NewFromUtf8(isolate, "roll").ToLocalChecked());

	if (maybePitch.IsEmpty() || maybeYaw.IsEmpty() || maybeRoll.IsEmpty())
		return std::nullopt;

	auto pitch = maybePitch.ToLocalChecked();
	auto yaw = maybeYaw.ToLocalChecked();
	auto roll = maybeRoll.ToLocalChecked();

	if (!pitch->IsNumber() || !yaw->IsNumber() || !roll->IsNumber())
		return std::nullopt;

	return QAngle{ 
		static_cast<float>(pitch.As<v8::Number>()->Value()),
		static_cast<float>(yaw.As<v8::Number>()->Value()),
		static_cast<float>(roll.As<v8::Number>()->Value())
	};
}

inline v8::Local<v8::Object> CreateColorObject(v8::Local<v8::Context> context, const Color& clr)
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::EscapableHandleScope handleScope(isolate);

	auto obj = v8::Object::New(isolate);
	obj->Set(context, v8::String::NewFromUtf8(isolate, "r").ToLocalChecked(), v8::Number::New(isolate, clr.r()));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "g").ToLocalChecked(), v8::Number::New(isolate, clr.g()));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked(), v8::Number::New(isolate, clr.b()));
	obj->Set(context, v8::String::NewFromUtf8(isolate, "a").ToLocalChecked(), v8::Number::New(isolate, clr.a()));

	return handleScope.Escape(obj);
}

inline std::optional<Color> ObjectToColor(v8::Local<v8::Context> context, v8::Local<v8::Object> obj)
{
	auto isolate = context->GetIsolate();

	auto maybeR = obj->Get(context, v8::String::NewFromUtf8(isolate, "r").ToLocalChecked());
	auto maybeG = obj->Get(context, v8::String::NewFromUtf8(isolate, "g").ToLocalChecked());
	auto maybeB = obj->Get(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked());
	auto maybeA = obj->Get(context, v8::String::NewFromUtf8(isolate, "a").ToLocalChecked());

	if (maybeR.IsEmpty() || maybeG.IsEmpty() || maybeB.IsEmpty())
		return std::nullopt;

	auto r = maybeR.ToLocalChecked();
	auto g = maybeG.ToLocalChecked();
	auto b = maybeB.ToLocalChecked();
	int a = 0;
	if(!maybeA.IsEmpty())
		a = static_cast<int>(maybeA.ToLocalChecked().As<v8::Number>()->Value());

	if (!r->IsNumber() || !g->IsNumber() || !b->IsNumber())
		return std::nullopt;

	return Color{ 
		static_cast<int>(r.As<v8::Number>()->Value()),
		static_cast<int>(g.As<v8::Number>()->Value()),
		static_cast<int>(b.As<v8::Number>()->Value()),
		a
	};
}