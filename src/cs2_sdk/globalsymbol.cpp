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
