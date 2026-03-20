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
#include <iostream>
#include <ostream>
#include "platform.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

// Likely just CUtlSymbolLarge utilizing a global symbol table.
class CGlobalSymbol
{
public:
	CGlobalSymbol() = default;
	CGlobalSymbol(const char* str);

	uint32_t Hash() const;
	const char* Get() const;
	operator const char* () const;
	bool operator==(const CGlobalSymbol& rhs) const;
private:
	const char* m_handle;
};

using CGlobalSymbolCaseSensitive = CGlobalSymbol;

struct GlobalSymbolHashFunctor
{
	unsigned int operator() (const CGlobalSymbol& globalSymbol) const;
};

namespace {
	PLATFORM_INTERFACE CGlobalSymbol _MakeGlobalSymbol(const char* str);
	PLATFORM_INTERFACE CGlobalSymbolCaseSensitive _MakeGlobalSymbolCaseSensitive(const char* str);
	PLATFORM_INTERFACE CGlobalSymbol _FindGlobalSymbol(const char* str);
	PLATFORM_INTERFACE CGlobalSymbol _FindGlobalSymbolByHash(uint32 hash);
}

#define MakeGlobalSymbol(str) _MakeGlobalSymbol(str)