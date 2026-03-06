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
#include "v8-function-callback.h"

namespace ScriptDomainCallbacks {
	void V8NewMsg(const v8::FunctionCallbackInfo<v8::Value>& info);
	void V8GetSchemaField(const v8::FunctionCallbackInfo<v8::Value>& args);
	void V8ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
	void V8ShowHudHintAll(const v8::FunctionCallbackInfo<v8::Value>& args);
	void V8ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args);
	void AddSampleCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
	void SetEntityMoveType(const v8::FunctionCallbackInfo<v8::Value>& args);

	void CreateUserMessage(const v8::FunctionCallbackInfo<v8::Value>& args);

	template <typename T>
	constexpr void SetV8NumericReturnValue(const v8::FunctionCallbackInfo<v8::Value>& args, void* ent, size_t offset);
};