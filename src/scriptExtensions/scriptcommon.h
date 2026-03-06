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