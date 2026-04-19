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

// Unwrap helpers for specific entity types, these are dependent on the definitions outside of hl2sdk!

#pragma once
#include "scriptcommon.h"
#include "entity/ccsplayercontroller.h"

template <>
inline std::optional<CCSPlayerController*> UnwrapThis(const CallContext& context)
{
	auto targetEntHandle = UnwrapThis<CEntityHandle>(context);
	if (!targetEntHandle)
		return {};

	if (!targetEntHandle->IsValid())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return {};
	}

	auto ent = static_cast<CCSPlayerController*>(targetEntHandle->Get());
	if (!ent || !ent->IsController())
	{
		ThrowFunctionException(context, "invoked with an unrecognized 'this' value.");
		return {};
	}

	return ent;
}