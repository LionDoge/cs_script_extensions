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

#include "globalsymbol.h"

CGlobalSymbol::CGlobalSymbol(const char* str)
{
	m_handle = MakeGlobalSymbol(str);
}

CGlobalSymbol::operator const char* () const 
{
	return Get(); 
}

uint32_t CGlobalSymbol::Hash() const
{
	// Storing the hash behind the string pointer is apparently a characteristic of CUtlSymbolLarge.
	// MakeGlobalSymbol however hides the actual symbol table that is used, so we can't access it the intended way.
	return *(uint32_t*)(m_handle - 4);
}

bool CGlobalSymbol::operator==(const CGlobalSymbol& rhs) const
{
	return m_handle == rhs.Get();
}

const char* CGlobalSymbol::Get() const
{
	return m_handle;
}

unsigned int GlobalSymbolHashFunctor::operator()(const CGlobalSymbol& globalSymbol) const
{
	return globalSymbol.Hash();
}
