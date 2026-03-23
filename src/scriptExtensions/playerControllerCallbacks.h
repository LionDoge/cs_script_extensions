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

namespace ScriptPlayerControllerCallbacks {
	void GetSteamID(const v8::FunctionCallbackInfo<v8::Value>& args);
	void ShowHTMLMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
	void ShowHudHint(const v8::FunctionCallbackInfo<v8::Value>& args);
	void Respawn(const v8::FunctionCallbackInfo<v8::Value>& args);
};